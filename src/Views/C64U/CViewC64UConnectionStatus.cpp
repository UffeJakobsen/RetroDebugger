#include "CViewC64UConnectionStatus.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/C64UConnectionStatus.h"
#include "C64SettingsStorage.h"

#include "imgui.h"

CViewC64UConnectionStatus::CViewC64UConnectionStatus(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
}

void CViewC64UConnectionStatus::Render()
{
}

void CViewC64UConnectionStatus::RenderImGui()
{
	PreRenderImGui();
	EC64UConnectionStatus status = debugInterface->GetConnectionStatus();

	// Status header with connect button floated right
	ImGui::AlignTextToFramePadding();
	ImGui::Text("Connection: %s", debugInterface->GetConnectionStatusString());
	ImGui::SameLine();
	float avail = ImGui::GetContentRegionAvail().x;
	if (status == C64U_CONNECTION_STATUS_DISCONNECTED)
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - 80);
		if (ImGui::Button("Connect##C64UStatus", ImVec2(75, 0)))
			debugInterface->Connect();
	}
	else
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - 100);
		if (ImGui::Button("Disconnect##C64UStatus", ImVec2(95, 0)))
			debugInterface->Disconnect();
	}

	ImGui::Text("Mode: %s", debugInterface->GetModeString());
	ImGui::TextDisabled("Frames: %llu | Age: %llums", (unsigned long long)debugInterface->GetVideoFrameCounter(), (unsigned long long)debugInterface->GetMillisecondsSinceLastVideoFrame());
	ImGui::TextDisabled("Media: %s", debugInterface->GetMediaStatusString());
	ImGui::Separator();

	// Memory auto-refresh toggle with PEEK warning
	bool autoRefresh = c64SettingsC64UAutoRefreshMemory;
	if (ImGui::Checkbox("Auto-refresh memory##C64UConnStatus", &autoRefresh))
	{
		c64SettingsC64UAutoRefreshMemory = autoRefresh;
		C64DebuggerStoreSettings();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(!)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("WARNING: reads use CPU PEEK via REST.\nReading I/O ranges ($D000-$DFFF) may\ntrigger side effects on real hardware.");

	bool refreshIO = c64SettingsC64URefreshIORange;
	if (ImGui::Checkbox("Allow I/O reads $D000-$DFFF##C64UConnStatus", &refreshIO))
	{
		c64SettingsC64URefreshIORange = refreshIO;
		C64DebuggerStoreSettings();
	}

	ImGui::Separator();

	// Trace mode selection in compact 2-column layout
	ImGui::Text("Trace:");
	int currentMode = c64SettingsC64UTraceMode;
	float colWidth = ImGui::GetContentRegionAvail().x * 0.5f;

	// Row 1
	if (ImGui::RadioButton("Auto##C64UConnStatus0", &currentMode, 0)) { c64SettingsC64UTraceMode = 0; C64DebuggerStoreSettings(); }
	ImGui::SameLine(colWidth);
	if (ImGui::RadioButton("VIC Only##C64UConnStatus2", &currentMode, 2)) { c64SettingsC64UTraceMode = 2; C64DebuggerStoreSettings(); }

	// Row 2
	if (ImGui::RadioButton("6510 Only##C64UConnStatus1", &currentMode, 1)) { c64SettingsC64UTraceMode = 1; C64DebuggerStoreSettings(); }
	ImGui::SameLine(colWidth);
	if (ImGui::RadioButton("6510 & VIC##C64UConnStatus3", &currentMode, 3)) { c64SettingsC64UTraceMode = 3; C64DebuggerStoreSettings(); }

	// Row 3
	if (ImGui::RadioButton("1541 Only##C64UConnStatus4", &currentMode, 4)) { c64SettingsC64UTraceMode = 4; C64DebuggerStoreSettings(); }
	ImGui::SameLine(colWidth);
	if (ImGui::RadioButton("6510 & 1541##C64UConnStatus5", &currentMode, 5)) { c64SettingsC64UTraceMode = 5; C64DebuggerStoreSettings(); }

	ImGui::Separator();

	// Mode toggle button with dynamic label
	if (debugInterface->IsInTraceMode())
	{
		if (ImGui::Button(">> Screen Mode", ImVec2(-1, 0)))
			debugInterface->EnterScreenMode();
	}
	else
	{
		if (ImGui::Button("<< Trace Mode", ImVec2(-1, 0)))
			debugInterface->EnterTraceMode(c64SettingsC64UTraceMode);
	}

	PostRenderImGui();
}
