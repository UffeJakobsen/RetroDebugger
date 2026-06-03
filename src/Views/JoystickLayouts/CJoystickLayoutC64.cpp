#include "CJoystickLayoutC64.h"
#include "imgui.h"
#include <stdio.h>

// Joystick axis bit masks
#define JOYPAD_N		0x01
#define JOYPAD_S		0x02
#define JOYPAD_W		0x04
#define JOYPAD_E		0x08
#define JOYPAD_FIRE		0x10

// UTF-8 arrow characters
static const char* ARROW_UP    = "\xe2\x96\xb2";  // ▲
static const char* ARROW_DOWN  = "\xe2\x96\xbc";  // ▼
static const char* ARROW_LEFT  = "\xe2\x97\x84";  // ◄
static const char* ARROW_RIGHT = "\xe2\x96\xba";  // ►

// Colors
static const ImU32 COLOR_DPAD_BG       = IM_COL32( 50,  50,  50, 255);
static const ImU32 COLOR_BTN_NORMAL    = IM_COL32( 80,  80,  80, 255);
static const ImU32 COLOR_BTN_PRESSED   = IM_COL32( 60, 130, 255, 255);
static const ImU32 COLOR_BTN_BORDER    = IM_COL32(120, 120, 120, 255);
static const ImU32 COLOR_STICKY_BORDER = IM_COL32(255, 220,   0, 255);
static const ImU32 COLOR_FIRE_NORMAL   = IM_COL32(180,  30,  30, 255);
static const ImU32 COLOR_FIRE_PRESSED  = IM_COL32( 60, 130, 255, 255);
static const ImU32 COLOR_LABEL         = IM_COL32(220, 220, 220, 255);

static const float STICKY_BORDER_THICKNESS = 3.0f;

int CJoystickLayoutC64::GetButtonCount()
{
	return 5;
}

uint32 CJoystickLayoutC64::GetButtonAxis(int buttonIndex)
{
	switch (buttonIndex)
	{
		case 0: return JOYPAD_N;
		case 1: return JOYPAD_S;
		case 2: return JOYPAD_W;
		case 3: return JOYPAD_E;
		case 4: return JOYPAD_FIRE;
		default: return 0;
	}
}

const char* CJoystickLayoutC64::GetButtonLabel(int buttonIndex)
{
	switch (buttonIndex)
	{
		case 0: return ARROW_UP;
		case 1: return ARROW_DOWN;
		case 2: return ARROW_LEFT;
		case 3: return ARROW_RIGHT;
		case 4: return "FIRE";
		default: return "";
	}
}

// Manual hit testing — more reliable than InvisibleButton with custom draw positioning
static void DrawRectButton(ImVec2 pos, ImVec2 size,
						   bool isPressed, bool isSticky,
						   const char* label,
						   bool* outClicked,
						   ImDrawList* dl)
{
	ImVec2 pMax(pos.x + size.x, pos.y + size.y);
	ImVec2 mousePos = ImGui::GetIO().MousePos;
	bool hovered = (mousePos.x >= pos.x && mousePos.x < pMax.x &&
					mousePos.y >= pos.y && mousePos.y < pMax.y);

	if (outClicked) *outClicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

	ImU32 fillColor = (isPressed || (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)))
					  ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
	dl->AddRectFilled(pos, pMax, fillColor, 3.0f);
	dl->AddRect(pos, pMax, COLOR_BTN_BORDER, 3.0f, 0, 1.0f);

	if (isSticky)
		dl->AddRect(pos, pMax, COLOR_STICKY_BORDER, 3.0f, 0, STICKY_BORDER_THICKNESS);

	ImVec2 textSize = ImGui::CalcTextSize(label);
	ImVec2 labelPos(pos.x + (size.x - textSize.x) * 0.5f,
					pos.y + (size.y - textSize.y) * 0.5f);
	dl->AddText(labelPos, COLOR_LABEL, label);
}

static void DrawCircleButton(ImVec2 center, float radius,
							 bool isPressed, bool isSticky,
							 const char* label,
							 bool* outClicked,
							 ImDrawList* dl)
{
	ImVec2 mousePos = ImGui::GetIO().MousePos;
	float dx = mousePos.x - center.x;
	float dy = mousePos.y - center.y;
	bool hovered = (dx * dx + dy * dy) <= (radius * radius);

	if (outClicked) *outClicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

	ImU32 fillColor = (isPressed || (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)))
					  ? COLOR_FIRE_PRESSED : COLOR_FIRE_NORMAL;
	dl->AddCircleFilled(center, radius, fillColor, 32);
	dl->AddCircle(center, radius, COLOR_BTN_BORDER, 32, 1.0f);

	if (isSticky)
		dl->AddCircle(center, radius - 1.0f, COLOR_STICKY_BORDER, 32, STICKY_BORDER_THICKNESS);

	ImVec2 textSize = ImGui::CalcTextSize(label);
	ImVec2 labelPos(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);
	dl->AddText(labelPos, COLOR_LABEL, label);
}

void CJoystickLayoutC64::RenderPort(int portIndex, bool buttonPressed[], bool buttonSticky[],
									bool buttonClicked[], bool buttonReleased[],
									float availWidth, float availHeight)
{
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 origin = ImGui::GetCursorScreenPos();

	// Layout proportions:
	//   Left 55% of width: d-pad
	//   Right 45%: FIRE button
	//
	// D-pad: centered circle background, 4 directional rectangular buttons
	// The d-pad circle radius fills most of the left column

	float dpadColW = availWidth * 0.55f;
	float fireColW = availWidth - dpadColW;

	// D-pad sizing
	float dpadRadius = (availHeight < dpadColW ? availHeight : dpadColW) * 0.45f;
	ImVec2 dpadCenter(origin.x + dpadColW * 0.5f, origin.y + availHeight * 0.5f);

	// Draw d-pad background circle
	dl->AddCircleFilled(dpadCenter, dpadRadius, COLOR_DPAD_BG, 48);

	// Each directional button is a rectangle inside the d-pad area
	// Arm width is ~40% of radius, arm length is ~55% of radius
	float armW  = dpadRadius * 0.42f;
	float armH  = dpadRadius * 0.55f;
	float gap   = 2.0f;

	// Button positions (top-left corner of each rectangle)
	// Up: centered horizontally, above center
	// Down: centered horizontally, below center
	// Left: to the left of center
	// Right: to the right of center

	// Reserve space in ImGui layout
	ImGui::Dummy(ImVec2(availWidth, availHeight));

	// Up (index 0)
	DrawRectButton(ImVec2(dpadCenter.x - armW * 0.5f, dpadCenter.y - dpadRadius + gap),
				   ImVec2(armW, armH),
				   buttonPressed[0], buttonSticky[0], ARROW_UP,
				   &buttonClicked[0], dl);

	// Down (index 1)
	DrawRectButton(ImVec2(dpadCenter.x - armW * 0.5f, dpadCenter.y + dpadRadius - armH - gap),
				   ImVec2(armW, armH),
				   buttonPressed[1], buttonSticky[1], ARROW_DOWN,
				   &buttonClicked[1], dl);

	// Left (index 2)
	DrawRectButton(ImVec2(dpadCenter.x - dpadRadius + gap, dpadCenter.y - armW * 0.5f),
				   ImVec2(armH, armW),
				   buttonPressed[2], buttonSticky[2], ARROW_LEFT,
				   &buttonClicked[2], dl);

	// Right (index 3)
	DrawRectButton(ImVec2(dpadCenter.x + dpadRadius - armH - gap, dpadCenter.y - armW * 0.5f),
				   ImVec2(armH, armW),
				   buttonPressed[3], buttonSticky[3], ARROW_RIGHT,
				   &buttonClicked[3], dl);

	// FIRE button (index 4): round red button in the right column
	float fireRadius = (availHeight < fireColW ? availHeight : fireColW) * 0.38f;
	ImVec2 fireCenter(origin.x + dpadColW + fireColW * 0.5f, origin.y + availHeight * 0.5f);
	DrawCircleButton(fireCenter, fireRadius,
					 buttonPressed[4], buttonSticky[4], "FIRE",
					 &buttonClicked[4], dl);

	// buttonReleased not used — CViewJoystick tracks mouse release globally
	(void)buttonReleased;
}
