#include "CViewGT2SongInfo.h"
#include "GT2ViewCommon.h"
#include "GT2RenderHelper.h"
#include "CGT2FontAtlas.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>

extern "C" {
#include "gcommon.h"
#include "gsong.h"
#include "gorder.h"
#include "goattrk2.h"

extern int cursorcolortable[];
}

// GT2 color constants (from gdisplay.h)
#define CEDIT    10
#define CTITLE   15

// Edit mode constant (from goattrk2.h)
#define EDIT_NAMES 4

CViewGT2SongInfo::CViewGT2SongInfo(const char *name, float posX, float posY, float posZ,
									float sizeX, float sizeY, CGT2FontAtlas *fontAtlas)
: CGuiView(posX, posY, posZ, sizeX, sizeY)
{
	this->name = name;
	this->fontAtlas = fontAtlas;
	imGuiNoScrollbar = true;
}

CViewGT2SongInfo::~CViewGT2SongInfo()
{
}

void CViewGT2SongInfo::RenderImGui()
{
	PreRenderImGui();
	if (!fontAtlas->TryLoad()) { PostRenderImGui(); return; }

	ImDrawList *dl = ImGui::GetWindowDrawList();
	ImVec2 origin = ImGui::GetCursorScreenPos();

	int cc = cursorcolortable[cursorflash];

	char textbuffer[64];

	// Forked from gdisplay.c:438-464
	// Row 0 = NAME, row 1 = AUTHOR, row 2 = COPYR.

	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(0),
				origin.y + GT2RowToPixel(0),
				CTITLE, "NAME   ");
	sprintf(textbuffer, "%-32s", songname);
	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(7),
				origin.y + GT2RowToPixel(0),
				CEDIT, textbuffer);

	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(0),
				origin.y + GT2RowToPixel(1),
				CTITLE, "AUTHOR ");
	sprintf(textbuffer, "%-32s", authorname);
	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(7),
				origin.y + GT2RowToPixel(1),
				CEDIT, textbuffer);

	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(0),
				origin.y + GT2RowToPixel(2),
				CTITLE, "COPYR. ");
	sprintf(textbuffer, "%-32s", copyrightname);
	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(7),
				origin.y + GT2RowToPixel(2),
				CEDIT, textbuffer);

	if ((editmode == EDIT_NAMES) && (!eamode))
	{
		switch (enpos)
		{
			case 0:
				DrawBgGT2(dl, fontAtlas,
						  origin.x + GT2ColToPixel(7 + (int)strlen(songname)),
						  origin.y + GT2RowToPixel(0),
						  (u8)cc, 1);
				break;
			case 1:
				DrawBgGT2(dl, fontAtlas,
						  origin.x + GT2ColToPixel(7 + (int)strlen(authorname)),
						  origin.y + GT2RowToPixel(1),
						  (u8)cc, 1);
				break;
			case 2:
				DrawBgGT2(dl, fontAtlas,
						  origin.x + GT2ColToPixel(7 + (int)strlen(copyrightname)),
						  origin.y + GT2RowToPixel(2),
						  (u8)cc, 1);
				break;
		}
	}

	// Mouse click handling — set editmode and cursor position
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		ImVec2 mousePos = ImGui::GetIO().MousePos;
		int gridRow = GT2PixelToRow(mousePos.y - origin.y);

		if (gridRow >= 0 && gridRow <= 2)
		{
			editmode = EDIT_NAMES;
			enpos = gridRow;
		}
	}

	PostRenderImGui();
}

bool CViewGT2SongInfo::KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	// Song / author / copyright name editing uses native GT2's EDIT_NAMES
	// modal — forward unconditionally so text-entry keys flow through.
	return GT2_HandleRenoiseOrForwardKeyDownToNative(keyCode, isShift, isAlt, isControl, isSuper);
}

bool CViewGT2SongInfo::KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	GT2_ForwardKeyUp(keyCode);
	return true;
}

bool CViewGT2SongInfo::KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	return KeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}
