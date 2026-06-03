#include "CViewGT2TitleBar.h"
#include "GT2ViewCommon.h"
#include "GT2RenderHelper.h"
#include "CGT2FontAtlas.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>

extern "C" {
#include "gcommon.h"
#include "gplay.h"
#include "goattrk2.h"

extern int cursorcolortable[];
}

// Original GT2 title/status row packed color: fg=15, bg=1.
#define CTITLEBAR ((u8)(15 + 16))

CViewGT2TitleBar::CViewGT2TitleBar(const char *name, float posX, float posY, float posZ,
									float sizeX, float sizeY, CGT2FontAtlas *fontAtlas)
: CGuiView(posX, posY, posZ, sizeX, sizeY)
{
	this->name = name;
	this->fontAtlas = fontAtlas;
	imGuiNoScrollbar = true;
}

CViewGT2TitleBar::~CViewGT2TitleBar()
{
}

const char *CViewGT2TitleBar::GetDisplayProgramName()
{
	return "GoatNoiseTracker v0.1 (2.75)";
}

u8 CViewGT2TitleBar::GetTitleBarColor()
{
	return CTITLEBAR;
}

void CViewGT2TitleBar::RenderImGui()
{
	PreRenderImGui();
	if (!fontAtlas->TryLoad()) { PostRenderImGui(); return; }

	ImDrawList *dl = ImGui::GetWindowDrawList();
	ImVec2 origin = ImGui::GetCursorScreenPos();

	int cc = cursorcolortable[cursorflash];

	char textbuffer[256];

	// Forked from gdisplay.c:62-108 (non-menu branch of printstatus title row)
	// Blank background across the row
	ImVec2 avail = ImGui::GetContentRegionAvail();
	DrawBlankGT2(dl, fontAtlas,
				 origin.x, origin.y,
				 GetTitleBarColor(), (int)(avail.x / GT2CellW()) + 1);

	// Program name / loaded song filename at col 0 (up to 49 chars)
	if (!strlen(loadedsongfilename))
		sprintf(textbuffer, "%s", GetDisplayProgramName());
	else
		sprintf(textbuffer, "%s - %s", GetDisplayProgramName(), loadedsongfilename);
	textbuffer[49] = '\0';
	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(0),
				origin.y + GT2RowToPixel(0),
				GetTitleBarColor(), textbuffer);

	// Flag indicators — original cols 50..91 (40+10 through 72+20)
	if (usefinevib)
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(50),
					origin.y + GT2RowToPixel(0),
					GetTitleBarColor(), "FV");

	if (optimizepulse)
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(53),
					origin.y + GT2RowToPixel(0),
					GetTitleBarColor(), "PO");

	if (optimizerealtime)
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(56),
					origin.y + GT2RowToPixel(0),
					GetTitleBarColor(), "RO");

	if (ntsc)
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(59),
					origin.y + GT2RowToPixel(0),
					GetTitleBarColor(), "NTSC");
	else
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(59),
					origin.y + GT2RowToPixel(0),
					GetTitleBarColor(), " PAL");

	if (!sidmodel)
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(64),
					origin.y + GT2RowToPixel(0),
					GetTitleBarColor(), "6581");
	else
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(64),
					origin.y + GT2RowToPixel(0),
					GetTitleBarColor(), "8580");

	sprintf(textbuffer, "HR:%04X", adparam);
	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(69),
				origin.y + GT2RowToPixel(0),
				GetTitleBarColor(), textbuffer);

	if (eamode)
		DrawBgGT2(dl, fontAtlas,
				  origin.x + GT2ColToPixel(72 + eacolumn),
				  origin.y + GT2RowToPixel(0),
				  (u8)cc, 1);

	if (multiplier)
	{
		sprintf(textbuffer, "%2dX", multiplier);
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(77),
					origin.y + GT2RowToPixel(0),
					GetTitleBarColor(), textbuffer);
	}
	else
	{
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(77),
					origin.y + GT2RowToPixel(0),
					GetTitleBarColor(), "25Hz");
	}

	// Mouse click handling — coordinates forwarded to GT2 event queue in Task 12
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		ImVec2 mousePos = ImGui::GetIO().MousePos;
		int gridCol = GT2PixelToCol(mousePos.x - origin.x);
		int gridRow = GT2PixelToRow(mousePos.y - origin.y);
		// TODO: forward to GT2 event queue (will be connected in Task 12)
		(void)gridCol;
		(void)gridRow;
	}

	PostRenderImGui();
}

bool CViewGT2TitleBar::KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	return GT2_HandleRenoiseOrForwardKeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}

bool CViewGT2TitleBar::KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	GT2_ForwardKeyUp(keyCode);
	return true;
}

bool CViewGT2TitleBar::KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	return KeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}
