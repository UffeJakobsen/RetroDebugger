#include "CViewJoystick.h"
#include "SYS_Main.h"
#include "CGuiMain.h"
#include "CViewC64.h"
#include "CDebugInterface.h"
#include "CMainMenuBar.h"
#include "C64SettingsStorage.h"
#include "DebuggerDefs.h"
#include "imgui.h"
#include "GAM_GamePads.h"

static const uint32 kJoypadBits[JOYSTICK_MAX_BUTTONS] = {
	JOYPAD_N, JOYPAD_S, JOYPAD_W, JOYPAD_E,
	JOYPAD_FIRE, JOYPAD_FIRE_B, JOYPAD_START, JOYPAD_SELECT
};

CViewJoystick::CViewJoystick(const char *name, float posX, float posY, float posZ,
							  float sizeX, float sizeY,
							  CDebugInterface *debugInterface, CJoystickLayout *layout, int maxPorts)
: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
	this->layout = layout;
	this->maxPorts = maxPorts;

	visiblePortConfig = maxPorts; // show all ports
	stickyMode = false;

	memset(buttonSticky,  0, sizeof(buttonSticky));
	memset(buttonPressed, 0, sizeof(buttonPressed));

	mouseHeldPort = -1;
	mouseHeldButton = -1;

	imGuiNoScrollbar = true;
}

CViewJoystick::~CViewJoystick()
{
}

void CViewJoystick::UpdateButtonStates()
{
	for (int port = 0; port < maxPorts; port++)
	{
		uint32 state = debugInterface->GetJoystickState(port);
		int btnCount = layout->GetButtonCount();
		for (int btn = 0; btn < btnCount && btn < JOYSTICK_MAX_BUTTONS; btn++)
		{
			uint32 axis = layout->GetButtonAxis(btn);
			buttonPressed[port][btn] = ((state & axis) != 0);
		}
	}
}

void CViewJoystick::HandleButtonClick(int portIndex, int buttonIndex)
{
	uint32 axis = layout->GetButtonAxis(buttonIndex);

	if (stickyMode)
	{
		if (buttonSticky[portIndex][buttonIndex])
		{
			// Already sticky — unstick it
			buttonSticky[portIndex][buttonIndex] = false;
			debugInterface->JoystickUp(portIndex, axis);
		}
		else
		{
			// Stick it down
			buttonSticky[portIndex][buttonIndex] = true;
			debugInterface->JoystickDown(portIndex, axis);
		}
	}
	else
	{
		if (buttonSticky[portIndex][buttonIndex])
		{
			// Was sticky from a previous sticky session — unstick it
			buttonSticky[portIndex][buttonIndex] = false;
			debugInterface->JoystickUp(portIndex, axis);
		}
		else
		{
			// Normal press
			debugInterface->JoystickDown(portIndex, axis);
		}
	}
}

void CViewJoystick::HandleButtonRelease(int portIndex, int buttonIndex)
{
	if (!stickyMode && !buttonSticky[portIndex][buttonIndex])
	{
		uint32 axis = layout->GetButtonAxis(buttonIndex);
		debugInterface->JoystickUp(portIndex, axis);
	}
}

void CViewJoystick::RenderInputSourceCombo(int portIndex)
{
	viewC64->mainMenuBar->UpdateGamepads();

	int &selectedJoystick = (portIndex == 0)
		? viewC64->mainMenuBar->selectedJoystick1
		: viewC64->mainMenuBar->selectedJoystick2;

	// Build preview string
	const char *previewStr = "Off";
	if (selectedJoystick == SelectedJoystickKeyboard)
	{
		previewStr = "Keyboard";
	}
	else if (selectedJoystick >= SelectedJoystickGamepad1)
	{
		int gpIndex = selectedJoystick - SelectedJoystickGamepad1; // 0-based
		int numGamepads = 0;
		CGamePad **gamepadDevices = GAM_EnumerateGamepads(&numGamepads);
		if (gpIndex < numGamepads)
		{
			previewStr = gamepadDevices[gpIndex]->name;
		}
	}

	char comboId[32];
	snprintf(comboId, sizeof(comboId), "##joystick_src_%d", portIndex);

	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::BeginCombo(comboId, previewStr))
	{
		bool saveSettings = false;

		// Off
		bool isOff = (selectedJoystick == SelectedJoystickOff);
		if (ImGui::Selectable("Off", isOff))
		{
			selectedJoystick = SelectedJoystickOff;
			saveSettings = true;
		}
		if (isOff)
			ImGui::SetItemDefaultFocus();

		// Keyboard
		bool isKeyboard = (selectedJoystick == SelectedJoystickKeyboard);
		if (ImGui::Selectable("Keyboard", isKeyboard))
		{
			selectedJoystick = SelectedJoystickKeyboard;
			saveSettings = true;
		}
		if (isKeyboard)
			ImGui::SetItemDefaultFocus();

		// Gamepads
		int numGamepads = 0;
		CGamePad **gamepadDevices = GAM_EnumerateGamepads(&numGamepads);
		for (int i = 0; i < numGamepads; i++)
		{
			char gpLabel[128];
			if (gamepadDevices[i]->isActive)
				snprintf(gpLabel, sizeof(gpLabel), "GamePad #%d: %s", (i + 1), gamepadDevices[i]->name);
			else
				snprintf(gpLabel, sizeof(gpLabel), "GamePad #%d", (i + 1));

			int gpValue = SelectedJoystickGamepad1 + i;
			bool isSelected = (selectedJoystick == gpValue);
			if (ImGui::Selectable(gpLabel, isSelected))
			{
				selectedJoystick = gpValue;
				saveSettings = true;
			}
			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}

		if (saveSettings)
		{
			if (portIndex == 0)
				c64SettingsSelectedJoystick1 = (u8)selectedJoystick;
			else
				c64SettingsSelectedJoystick2 = (u8)selectedJoystick;
			C64DebuggerStoreSettings();
		}

		ImGui::EndCombo();
	}
}

void CViewJoystick::RenderTopBar()
{
	// Port visibility toggle buttons — individual port buttons + "All"
	for (int p = 0; p < maxPorts; p++)
	{
		bool isSelected = (visiblePortConfig == p);
		if (isSelected)
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		char btnLabel[8];
		snprintf(btnLabel, sizeof(btnLabel), "%d", p + 1);
		if (ImGui::Button(btnLabel))
			visiblePortConfig = p;
		if (isSelected)
			ImGui::PopStyleColor();
		ImGui::SameLine();
	}

	// "All" button shows all ports
	bool allSelected = (visiblePortConfig == maxPorts);
	if (allSelected)
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	if (ImGui::Button("All"))
		visiblePortConfig = maxPorts;
	if (allSelected)
		ImGui::PopStyleColor();

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	// Per-visible-port input source combos (only for ports 0 and 1 — settings exist for these)
	if (visiblePortConfig == maxPorts)
	{
		// Show combos for first 2 ports (settings limitation)
		for (int p = 0; p < 2 && p < maxPorts; p++)
		{
			char label[8];
			snprintf(label, sizeof(label), "P%d:", p + 1);
			ImGui::Text("%s", label);
			ImGui::SameLine();
			RenderInputSourceCombo(p);
			ImGui::SameLine();
			ImGui::Spacing();
			ImGui::SameLine();
		}
	}
	else if (visiblePortConfig < 2)
	{
		char label[8];
		snprintf(label, sizeof(label), "P%d:", visiblePortConfig + 1);
		ImGui::Text("%s", label);
		ImGui::SameLine();
		RenderInputSourceCombo(visiblePortConfig);
		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();
	}

	ImGui::Checkbox("Sticky", &stickyMode);
}

void CViewJoystick::RenderSinglePort(int portIndex, float width, float height)
{
	bool clicked[JOYSTICK_MAX_BUTTONS]  = {};
	bool released[JOYSTICK_MAX_BUTTONS] = {};

	layout->RenderPort(portIndex,
					   buttonPressed[portIndex],
					   buttonSticky[portIndex],
					   clicked, released,
					   width, height);

	int btnCount = layout->GetButtonCount();
	for (int btn = 0; btn < btnCount && btn < JOYSTICK_MAX_BUTTONS; btn++)
	{
		if (clicked[btn])
		{
			HandleButtonClick(portIndex, btn);
			mouseHeldPort = portIndex;
			mouseHeldButton = btn;
		}
	}
}

void CViewJoystick::RenderImGui()
{
	PreRenderImGui();

	UpdateButtonStates();
	RenderTopBar();
	ImGui::Separator();

	ImVec2 avail = ImGui::GetContentRegionAvail();

	if (visiblePortConfig == maxPorts)
	{
		// All ports side by side
		float portWidth = avail.x / (float)maxPorts - ImGui::GetStyle().ItemSpacing.x;
		float portHeight = avail.y;

		for (int p = 0; p < maxPorts; p++)
		{
			char childId[32];
			snprintf(childId, sizeof(childId), "##joy_port_%p_%d", (void*)this, p);
			if (ImGui::BeginChild(childId, ImVec2(portWidth, portHeight), false))
			{
				char portLabel[16];
				snprintf(portLabel, sizeof(portLabel), "Port %d", p + 1);
				ImGui::Text("%s", portLabel);
				float innerH = portHeight - ImGui::GetTextLineHeightWithSpacing();
				RenderSinglePort(p, portWidth, innerH);
			}
			ImGui::EndChild();
			if (p < maxPorts - 1)
				ImGui::SameLine();
		}
	}
	else
	{
		int portIndex = visiblePortConfig;
		char portLabel[16];
		snprintf(portLabel, sizeof(portLabel), "Port %d", portIndex + 1);
		ImGui::Text("%s", portLabel);
		RenderSinglePort(portIndex, avail.x, avail.y - ImGui::GetTextLineHeightWithSpacing());
	}

	// Handle mouse release for held button
	if (mouseHeldPort >= 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		HandleButtonRelease(mouseHeldPort, mouseHeldButton);
		mouseHeldPort = -1;
		mouseHeldButton = -1;
	}

	PostRenderImGui();
}
