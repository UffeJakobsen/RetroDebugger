#include "CViewC64UExecutionFlow.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/Trace/C64UTraceBuffer.h"
#include "../../Emulators/c64u/Trace/C64U6502Decoder.h"
#include "../../Emulators/c64u/Trace/C64UDebugEntry.h"
#include "../../Emulators/c64u/Trace/C64UDecoderAnnotation.h"
#include "C64SettingsStorage.h"

#include "imgui.h"

CViewC64UExecutionFlow::CViewC64UExecutionFlow(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
}

void CViewC64UExecutionFlow::Render()
{
}

void CViewC64UExecutionFlow::RenderImGui()
{
	PreRenderImGui();

	C64U6502Decoder *decoder = debugInterface->GetDecoder6510();
	C64UTraceBuffer *traceBuffer = debugInterface->GetTraceBuffer();
	uint64_t totalEntries = traceBuffer ? traceBuffer->GetEntryCount() : 0;
	if (!traceBuffer || totalEntries == 0 || !decoder)
	{
		ImGui::TextDisabled("No trace data.");
		PostRenderImGui();
		return;
	}

	ImGui::Text("Execution Flow  PC: $%04X %s", decoder->GetCurrentPC(), decoder->IsSynced() ? "" : "(sync lost)");
	ImGui::Separator();

	if (ImGui::BeginTable("ExecFlow", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("PC", ImGuiTableColumnFlags_WidthFixed, 45);
		ImGui::TableSetupColumn("Op", ImGuiTableColumnFlags_WidthFixed, 25);
		ImGui::TableSetupColumn("Disasm", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Cyc", ImGuiTableColumnFlags_WidthFixed, 25);
		ImGui::TableHeadersRow();

		uint64_t start = (totalEntries > 2000) ? totalEntries - 2000 : 0;
		int shown = 0;

		// Persistent decoder avoids re-processing all entries every frame.
		// Reset when trace buffer is cleared (start goes back to 0).
		static C64U6502Decoder persistentDecoder;
		static uint64_t lastProcessedIndex = 0;
		if (start == 0 || lastProcessedIndex > totalEntries)
		{
			persistentDecoder.Reset();
			persistentDecoder.SetTraceMode(c64SettingsC64UTraceMode > 0 ? c64SettingsC64UTraceMode : 1);
			lastProcessedIndex = 0;
		}

		// Process only new entries since last frame
		uint64_t processFrom = (lastProcessedIndex > start) ? lastProcessedIndex : start;
		for (uint64_t i = processFrom; i < totalEntries; i++)
		{
			C64UDebugEntry entry = traceBuffer->GetEntry(i);
			if (persistentDecoder.ShouldProcessEntry(entry))
				persistentDecoder.ProcessEntry(entry);
		}
		lastProcessedIndex = totalEntries;

		// Render the most recent 100 opcode fetches from the window
		C64U6502Decoder tempDecoder;
		tempDecoder.SetTraceMode(c64SettingsC64UTraceMode > 0 ? c64SettingsC64UTraceMode : 1);

		for (uint64_t i = start; i < totalEntries && shown < 100; i++)
		{
			C64UDebugEntry entry = traceBuffer->GetEntry(i);
			if (!tempDecoder.ShouldProcessEntry(entry)) continue;
			C64UDecoderAnnotation ann = tempDecoder.ProcessEntry(entry);

			if (ann.type == C64UDecoderAnnotation::OPCODE_FETCH)
			{
				ImGui::TableNextRow();

				if (entry.address >= 0xFFFA)
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(80, 80, 20, 255));

				ImGui::TableSetColumnIndex(0); ImGui::Text("$%04X", entry.address);
				ImGui::TableSetColumnIndex(1); ImGui::Text("%02X", entry.data);
				ImGui::TableSetColumnIndex(2); ImGui::Text("%s", C64U6502Decoder::GetMnemonic(entry.data));
				ImGui::TableSetColumnIndex(3); ImGui::Text("%d", C64U6502Decoder::GetCycles(entry.data));
				shown++;
			}
		}

		ImGui::SetScrollHereY(1.0f);
		ImGui::EndTable();
	}

	PostRenderImGui();
}
