#include "CViewInputEventsTable.h"
#include "SYS_Main.h"
#include "CGuiMain.h"
#include "CViewC64.h"
#include "CDebugInterface.h"
#include "CSnapshotsManager.h"
#include "DebuggerDefs.h"
#include "CByteBuffer.h"
#include "imgui.h"
#include "SYS_FileSystem.h"

// VICE-specific fast flag for CPU hot loop
extern "C" { extern volatile int c64d_vice_input_tasks_flag; }
// Atari-specific fast flag
extern "C" { extern volatile int atrd_input_tasks_flag; }
// Deterministic latch flag — forces zero delay on keyboard/joystick latch
extern "C" { extern volatile int c64d_input_latch_immediate; }

static void UpdateInputTasksFlags(CDebugInterface *di)
{
	if (di->GetEmulatorType() == EMULATOR_TYPE_C64_VICE)
		c64d_vice_input_tasks_flag = 1;
	else if (di->GetEmulatorType() == EMULATOR_TYPE_ATARI800)
		atrd_input_tasks_flag = 1;
}

static const char* GetAxisName(u32 axis)
{
	switch (axis)
	{
		case JOYPAD_N: return "Up";
		case JOYPAD_S: return "Down";
		case JOYPAD_W: return "Left";
		case JOYPAD_E: return "Right";
		case JOYPAD_FIRE: return "Fire";
		case JOYPAD_FIRE_B: return "Fire B";
		case JOYPAD_START: return "Start";
		case JOYPAD_SELECT: return "Select";
		default: return "?";
	}
}

CViewInputEventsTable::CViewInputEventsTable(const char *name, float posX, float posY, float posZ,
											 float sizeX, float sizeY, CDebugInterface *debugInterface)
: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
	this->snapshotsManager = debugInterface->snapshotsManager;

	imGuiNoScrollbar = true;
	selectedRow = -1;
}

CViewInputEventsTable::~CViewInputEventsTable()
{
}

void CViewInputEventsTable::RenderImGui()
{
	PreRenderImGui();

	// Lock snapshots manager to safely iterate inputEventsByCycle from UI thread
	snapshotsManager->LockMutex();

	// Top bar: Record/Stop/Play buttons + event count + Save/Load

	bool isRecording = snapshotsManager->isStoreInputEventsEnabled;
	bool isReplaying = snapshotsManager->isReplayInputEventsEnabled;

	// Record button (red when active)
	if (isRecording)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
	if (ImGui::Button(isRecording ? "Recording" : "Record"))
	{
		if (!isRecording)
		{
			// Start recording: truncate future events and enable store
			u64 currentCycle = debugInterface->GetMainCpuCycleCounter();
			snapshotsManager->TruncateInputEventsAfterCycle(currentCycle);
			snapshotsManager->isStoreInputEventsEnabled = true;
			snapshotsManager->isReplayInputEventsEnabled = false;
			c64d_input_latch_immediate = 1;
			UpdateInputTasksFlags(debugInterface);
		}
		else
		{
			// Stop recording
			snapshotsManager->isStoreInputEventsEnabled = false;
			c64d_input_latch_immediate = 0;
		}
	}
	if (isRecording)
		ImGui::PopStyleColor();

	ImGui::SameLine();

	// Play button (green when active)
	if (isReplaying)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
	if (ImGui::Button(isReplaying ? "Playing" : "Play"))
	{
		if (!isReplaying)
		{
			// Reset replay marker to current cycle so replay starts from here
			snapshotsManager->lastReplayedCycle = debugInterface->GetMainCpuCycleCounter();
			snapshotsManager->isReplayInputEventsEnabled = true;
			snapshotsManager->isStoreInputEventsEnabled = false;
			c64d_input_latch_immediate = 1;
			UpdateInputTasksFlags(debugInterface);
		}
		else
		{
			snapshotsManager->isReplayInputEventsEnabled = false;
			c64d_input_latch_immediate = 0;
		}
	}
	if (isReplaying)
		ImGui::PopStyleColor();

	ImGui::SameLine();

	// Stop button
	if (ImGui::Button("Stop"))
	{
		snapshotsManager->isStoreInputEventsEnabled = false;
		snapshotsManager->isReplayInputEventsEnabled = false;
		c64d_input_latch_immediate = 0;
	}

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	ImGui::Text("%d events", (int)snapshotsManager->inputEventsByCycle.size());

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	// Save/Load buttons — saves to documents folder
	if (ImGui::Button("Save"))
	{
		char path[1024];
		snprintf(path, sizeof(path), "%s/input_events.rdie", gPathToDocuments);
		snapshotsManager->SaveInputEventsToFile(path);
	}
	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		char path[1024];
		snprintf(path, sizeof(path), "%s/input_events.rdie", gPathToDocuments);
		snapshotsManager->LoadInputEventsFromFile(path);
	}

	ImGui::Separator();

	// Table
	if (ImGui::BeginTable("##InputEvents", 5,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
		| ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("Cycle", ImGuiTableColumnFlags_DefaultSort);
		ImGui::TableSetupColumn("Frame");
		ImGui::TableSetupColumn("Type");
		ImGui::TableSetupColumn("Port");
		ImGui::TableSetupColumn("Detail");
		ImGui::TableSetupScrollFreeze(0, 1);  // freeze header row
		ImGui::TableHeadersRow();

		// Determine auto-scroll target cycle:
		// Recording: scroll to the latest event. Replaying: scroll to last replayed event.
		u64 autoScrollCycle = 0;
		bool shouldAutoScroll = false;
		if (isRecording && !snapshotsManager->inputEventsByCycle.empty())
		{
			autoScrollCycle = snapshotsManager->inputEventsByCycle.rbegin()->first;
			shouldAutoScroll = true;
		}
		else if (isReplaying)
		{
			autoScrollCycle = snapshotsManager->lastReplayedCycle;
			shouldAutoScroll = true;
		}

		int rowIndex = 0;
		for (auto &pair : snapshotsManager->inputEventsByCycle)
		{
			u64 cycle = pair.first;
			CStoredInputEvent *event = pair.second;

			// Parse byte buffer
			event->byteBuffer->Rewind();
			while (event->byteBuffer->IsEof() == false)
			{
				u8 eventType = event->byteBuffer->GetU8();

				ImGui::TableNextRow();

				// Auto-scroll to the last row at or before the target cycle.
				// Calling SetScrollHereY on every matching row is fine — last call wins.
				if (shouldAutoScroll && cycle <= autoScrollCycle)
					ImGui::SetScrollHereY(1.0f);

				// Check if selected
				bool isSelected = (rowIndex == selectedRow);

				// Cycle column
				ImGui::TableNextColumn();
				char label[32];
				snprintf(label, sizeof(label), "%llu##row%d", cycle, rowIndex);
				if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_SpanAllColumns))
				{
					selectedRow = rowIndex;
					// Jump to this cycle
					snapshotsManager->RestoreSnapshotByCycle(cycle);
				}

				// Frame column
				ImGui::TableNextColumn();
				ImGui::Text("%u", event->frame);

				if (eventType == DEBUGGER_EVENT_TYPE_JOYSTICK)
				{
					int port = event->byteBuffer->GetI32();
					u32 axis = event->byteBuffer->GetU32();
					u8 buttonState = event->byteBuffer->GetU8();

					// Type
					ImGui::TableNextColumn();
					ImGui::Text("Joy");

					// Port
					ImGui::TableNextColumn();
					ImGui::Text("%d", port);

					// Detail
					ImGui::TableNextColumn();
					ImGui::Text("%s %s", GetAxisName(axis),
							   buttonState == DEBUGGER_EVENT_BUTTON_DOWN ? "Down" : "Up");
				}
				else if (eventType == DEBUGGER_EVENT_TYPE_KEYBOARD)
				{
					u32 keyCode = event->byteBuffer->GetU32();
					u8 buttonState = event->byteBuffer->GetU8();

					ImGui::TableNextColumn();
					ImGui::Text("Kbd");

					ImGui::TableNextColumn();
					ImGui::Text("-");

					ImGui::TableNextColumn();
					ImGui::Text("Key=$%04X %s", keyCode,
							   buttonState == DEBUGGER_EVENT_BUTTON_DOWN ? "Down" : "Up");
				}
				else
				{
					// Unknown type -- skip display, break to avoid corruption
					ImGui::TableNextColumn(); ImGui::Text("?");
					ImGui::TableNextColumn(); ImGui::Text("-");
					ImGui::TableNextColumn(); ImGui::Text("Unknown type=%d", eventType);
					break;
				}

				rowIndex++;
			}
		}

		ImGui::EndTable();
	}

	// Delete key removes the specific selected sub-event (not the whole cycle)
	if (selectedRow >= 0 && ImGui::IsKeyPressed(ImGuiKey_Delete))
	{
		int idx = 0;
		for (auto it = snapshotsManager->inputEventsByCycle.begin();
			 it != snapshotsManager->inputEventsByCycle.end(); ++it)
		{
			CStoredInputEvent *ev = it->second;
			ev->byteBuffer->Rewind();

			// Count sub-events and find which one to delete
			int subEventStart = idx;
			int subEventCount = 0;
			int deleteSubIndex = -1;

			while (ev->byteBuffer->IsEof() == false)
			{
				u8 type = ev->byteBuffer->GetU8();
				int eventSize = 0;
				if (type == DEBUGGER_EVENT_TYPE_JOYSTICK) eventSize = 4 + 4 + 1; // I32+U32+U8
				else if (type == DEBUGGER_EVENT_TYPE_KEYBOARD) eventSize = 4 + 1; // U32+U8
				else break;

				// Skip payload
				for (int s = 0; s < eventSize; s++)
					ev->byteBuffer->GetU8();

				if (idx == selectedRow)
					deleteSubIndex = subEventCount;

				subEventCount++;
				idx++;
			}

			if (deleteSubIndex >= 0)
			{
				if (subEventCount <= 1)
				{
					// Only one sub-event — delete the whole entry
					snapshotsManager->inputEventsToReuse.push_back(ev);
					snapshotsManager->inputEventsByCycle.erase(it);
				}
				else
				{
					// Rebuild buffer without the deleted sub-event
					CByteBuffer *oldBuf = ev->byteBuffer;
					CByteBuffer *newBuf = new CByteBuffer();

					oldBuf->Rewind();
					int subIdx = 0;
					while (oldBuf->IsEof() == false)
					{
						u8 type = oldBuf->GetU8();
						int payloadSize = 0;
						if (type == DEBUGGER_EVENT_TYPE_JOYSTICK) payloadSize = 4 + 4 + 1;
						else if (type == DEBUGGER_EVENT_TYPE_KEYBOARD) payloadSize = 4 + 1;
						else break;

						if (subIdx == deleteSubIndex)
						{
							// Skip this event
							for (int s = 0; s < payloadSize; s++)
								oldBuf->GetU8();
						}
						else
						{
							// Copy this event
							newBuf->PutU8(type);
							for (int s = 0; s < payloadSize; s++)
								newBuf->PutU8(oldBuf->GetU8());
						}
						subIdx++;
					}

					// Swap buffers
					delete oldBuf;
					ev->byteBuffer = newBuf;
				}

				selectedRow = -1;
				break;
			}
		}
	}

	snapshotsManager->UnlockMutex();

	PostRenderImGui();
}
