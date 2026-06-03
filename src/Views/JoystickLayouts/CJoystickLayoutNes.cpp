#include "CJoystickLayoutNes.h"
#include "imgui.h"
#include <stdio.h>

// NES joypad axis bit masks
#define JOYPAD_N         0x01
#define JOYPAD_S         0x02
#define JOYPAD_W         0x04
#define JOYPAD_E         0x08
#define JOYPAD_FIRE      0x10
#define JOYPAD_FIRE_B    0x20
#define JOYPAD_START     0x40
#define JOYPAD_SELECT    0x80

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
static const ImU32 COLOR_SMALL_NORMAL  = IM_COL32( 70,  70,  70, 255);
static const ImU32 COLOR_FIRE_NORMAL   = IM_COL32(180,  30,  30, 255);
static const ImU32 COLOR_FIRE_PRESSED  = IM_COL32( 60, 130, 255, 255);
static const ImU32 COLOR_LABEL         = IM_COL32(220, 220, 220, 255);

static const float STICKY_BORDER_THICKNESS = 3.0f;

int CJoystickLayoutNes::GetButtonCount()
{
	return 8;
}

uint32 CJoystickLayoutNes::GetButtonAxis(int buttonIndex)
{
	switch (buttonIndex)
	{
		case 0: return JOYPAD_N;
		case 1: return JOYPAD_S;
		case 2: return JOYPAD_W;
		case 3: return JOYPAD_E;
		case 4: return JOYPAD_SELECT;
		case 5: return JOYPAD_START;
		case 6: return JOYPAD_FIRE_B;
		case 7: return JOYPAD_FIRE;
		default: return 0;
	}
}

const char* CJoystickLayoutNes::GetButtonLabel(int buttonIndex)
{
	switch (buttonIndex)
	{
		case 0: return ARROW_UP;
		case 1: return ARROW_DOWN;
		case 2: return ARROW_LEFT;
		case 3: return ARROW_RIGHT;
		case 4: return "SEL";
		case 5: return "STA";
		case 6: return "B";
		case 7: return "A";
		default: return "";
	}
}

// Manual hit testing helpers — bypass InvisibleButton for reliable clicks with custom drawing

static void DrawRectButton(ImVec2 pos, ImVec2 size,
						   bool isPressed, bool isSticky,
						   const char* label, ImU32 normalColor,
						   bool* outClicked, ImDrawList* dl)
{
	ImVec2 pMax(pos.x + size.x, pos.y + size.y);
	ImVec2 mousePos = ImGui::GetIO().MousePos;
	bool hovered = (mousePos.x >= pos.x && mousePos.x < pMax.x &&
					mousePos.y >= pos.y && mousePos.y < pMax.y);

	if (outClicked) *outClicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

	ImU32 fillColor = (isPressed || (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)))
					  ? COLOR_BTN_PRESSED : normalColor;
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
							 bool* outClicked, ImDrawList* dl)
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

void CJoystickLayoutNes::RenderPort(int portIndex, bool buttonPressed[], bool buttonSticky[],
									bool buttonClicked[], bool buttonReleased[],
									float availWidth, float availHeight)
{
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 origin = ImGui::GetCursorScreenPos();

	// Reserve space in ImGui layout
	ImGui::Dummy(ImVec2(availWidth, availHeight));

	// Layout: 3 columns — Left 35%: d-pad, Mid 30%: SELECT+START, Right 35%: B+A
	float dpadColW   = availWidth * 0.35f;
	float centerColW = availWidth * 0.30f;
	float fireColW   = availWidth - dpadColW - centerColW;

	// --- D-pad (left column) ---
	float dpadRadius = (availHeight < dpadColW ? availHeight : dpadColW) * 0.45f;
	ImVec2 dpadCenter(origin.x + dpadColW * 0.5f, origin.y + availHeight * 0.5f);
	dl->AddCircleFilled(dpadCenter, dpadRadius, COLOR_DPAD_BG, 48);

	float armW = dpadRadius * 0.42f;
	float armH = dpadRadius * 0.55f;
	float gap  = 2.0f;

	// Up (0)
	DrawRectButton(ImVec2(dpadCenter.x - armW * 0.5f, dpadCenter.y - dpadRadius + gap),
				   ImVec2(armW, armH),
				   buttonPressed[0], buttonSticky[0], ARROW_UP, COLOR_BTN_NORMAL,
				   &buttonClicked[0], dl);
	// Down (1)
	DrawRectButton(ImVec2(dpadCenter.x - armW * 0.5f, dpadCenter.y + dpadRadius - armH - gap),
				   ImVec2(armW, armH),
				   buttonPressed[1], buttonSticky[1], ARROW_DOWN, COLOR_BTN_NORMAL,
				   &buttonClicked[1], dl);
	// Left (2)
	DrawRectButton(ImVec2(dpadCenter.x - dpadRadius + gap, dpadCenter.y - armW * 0.5f),
				   ImVec2(armH, armW),
				   buttonPressed[2], buttonSticky[2], ARROW_LEFT, COLOR_BTN_NORMAL,
				   &buttonClicked[2], dl);
	// Right (3)
	DrawRectButton(ImVec2(dpadCenter.x + dpadRadius - armH - gap, dpadCenter.y - armW * 0.5f),
				   ImVec2(armH, armW),
				   buttonPressed[3], buttonSticky[3], ARROW_RIGHT, COLOR_BTN_NORMAL,
				   &buttonClicked[3], dl);

	// --- SELECT / START (center column) ---
	float smallBtnW  = centerColW * 0.75f;
	float smallBtnH  = availHeight * 0.22f;
	float smallGap   = availHeight * 0.08f;
	float totalSmall = smallBtnH * 2.0f + smallGap;
	float smallStartY = origin.y + (availHeight - totalSmall) * 0.5f;
	float smallX      = origin.x + dpadColW + (centerColW - smallBtnW) * 0.5f;

	// SELECT (4)
	DrawRectButton(ImVec2(smallX, smallStartY), ImVec2(smallBtnW, smallBtnH),
				   buttonPressed[4], buttonSticky[4], "SEL", COLOR_SMALL_NORMAL,
				   &buttonClicked[4], dl);
	// START (5)
	DrawRectButton(ImVec2(smallX, smallStartY + smallBtnH + smallGap), ImVec2(smallBtnW, smallBtnH),
				   buttonPressed[5], buttonSticky[5], "STA", COLOR_SMALL_NORMAL,
				   &buttonClicked[5], dl);

	// --- B / A buttons (right column) ---
	float fireColX    = origin.x + dpadColW + centerColW;
	float btnRadius   = (availHeight < fireColW * 0.45f ? availHeight * 0.38f : fireColW * 0.20f);
	float fireCenterY = origin.y + availHeight * 0.5f;

	// B (6)
	DrawCircleButton(ImVec2(fireColX + fireColW * 0.28f, fireCenterY), btnRadius,
					 buttonPressed[6], buttonSticky[6], "B",
					 &buttonClicked[6], dl);
	// A (7)
	DrawCircleButton(ImVec2(fireColX + fireColW * 0.72f, fireCenterY), btnRadius,
					 buttonPressed[7], buttonSticky[7], "A",
					 &buttonClicked[7], dl);

	// buttonReleased not used — CViewJoystick tracks mouse release globally
	(void)buttonReleased;
}
