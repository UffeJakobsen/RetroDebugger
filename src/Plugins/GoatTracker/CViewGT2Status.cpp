#include "CViewGT2Status.h"
#include "GT2ViewCommon.h"
#include "GT2RenderHelper.h"
#include "CGT2FontAtlas.h"
#include "imgui.h"
#include <cstdio>

extern "C" {
#include "gcommon.h"
#include "gplay.h"
#include "gpattern.h"
#include "gtable.h"
#include "goattrk2.h"

extern int timemin;
extern int timesec;
extern int timeframe;
extern char timechar[];
extern int pattlen[MAX_PATT];
extern int cursorcolortable[];
}

// GT2 color constants (from gdisplay.h)
#define CEDIT    10
#define CTITLE   15

CViewGT2Status::CViewGT2Status(const char *name, float posX, float posY, float posZ,
								float sizeX, float sizeY, CGT2FontAtlas *fontAtlas)
: CGuiView(posX, posY, posZ, sizeX, sizeY)
{
	this->name = name;
	this->fontAtlas = fontAtlas;
	imGuiNoScrollbar = true;
}

CViewGT2Status::~CViewGT2Status()
{
}

void CViewGT2Status::RenderImGui()
{
	PreRenderImGui();
	if (!fontAtlas->TryLoad()) { PostRenderImGui(); return; }

	ImDrawList *dl = ImGui::GetWindowDrawList();
	ImVec2 origin = ImGui::GetCursorScreenPos();

	char textbuffer[64];

	// Forked from gdisplay.c:465-521
	// Row 0 = first status line (OCTAVE, PLAYING/STOPPED, CHN header)
	// Row 1 = second status line (EDITMODE/JAM, time, channel positions)

	// "OCTAVE N" at col 0, row 0
	sprintf(textbuffer, "OCTAVE %d", epoctave);
	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(0),
				origin.y + GT2RowToPixel(0),
				CTITLE, textbuffer);

	// "EDITMODE"/"JAM MODE" at col 0, row 1 — color depends on autoadvance
	int color;
	switch (autoadvance)
	{
		case 0:  color = 10; break;
		case 1:  color = 14; break;
		case 2:  color = 12; break;
		default: color = 10; break;
	}

	if (recordmode)
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(0),
					origin.y + GT2RowToPixel(1),
					(u8)color, "EDITMODE");
	else
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(0),
					origin.y + GT2RowToPixel(1),
					(u8)color, "JAM MODE");

	// "PLAYING"/"STOPPED" at col 10, row 0
	if (isplaying())
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(10),
					origin.y + GT2RowToPixel(0),
					CTITLE, "PLAYING");
	else
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(10),
					origin.y + GT2RowToPixel(0),
					CTITLE, "STOPPED");

	// Time display at col 10, row 1
	if (multiplier)
	{
		if (!ntsc)
			sprintf(textbuffer, " %02d%c%02d ", timemin, timechar[timeframe / (25 * multiplier) & 1], timesec);
		else
			sprintf(textbuffer, " %02d%c%02d ", timemin, timechar[timeframe / (30 * multiplier) & 1], timesec);
	}
	else
	{
		if (!ntsc)
			sprintf(textbuffer, " %02d%c%02d ", timemin, timechar[(timeframe / 13) & 1], timesec);
		else
			sprintf(textbuffer, " %02d%c%02d ", timemin, timechar[(timeframe / 15) & 1], timesec);
	}
	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(10),
				origin.y + GT2RowToPixel(1),
				CEDIT, textbuffer);

	// Channel header at col 20, row 0
	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(20),
				origin.y + GT2RowToPixel(0),
				CTITLE, " CHN1   CHN2   CHN3 ");

	// Channel positions at row 1
	for (int c = 0; c < MAX_CHN; c++)
	{
		int chnpos = chn[c].songptr;
		int chnrow = chn[c].pattptr / 4;
		chnpos--;
		if (chnpos < 0) chnpos = 0;
		if (chnrow > pattlen[chn[c].pattnum]) chnrow = pattlen[chn[c].pattnum];
		if (chnrow >= 100) chnrow -= 100;

		sprintf(textbuffer, "%03d/%02d", chnpos, chnrow);
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(20 + 7 * c),
					origin.y + GT2RowToPixel(1),
					CEDIT, textbuffer);
	}

	// Table lock indicator: "U" (unlocked) or " " (locked) at col 18, row 1
	if (etlock)
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(18),
					origin.y + GT2RowToPixel(1),
					CTITLE, " ");
	else
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(18),
					origin.y + GT2RowToPixel(1),
					CTITLE, "U");

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

bool CViewGT2Status::KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	return GT2_HandleRenoiseOrForwardKeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}

bool CViewGT2Status::KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	GT2_ForwardKeyUp(keyCode);
	return true;
}

bool CViewGT2Status::KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	return KeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}
