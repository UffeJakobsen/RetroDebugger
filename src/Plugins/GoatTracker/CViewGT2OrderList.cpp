#include "CViewGT2OrderList.h"
#include "GT2ViewCommon.h"
#include "GT2RenderHelper.h"
#include "CGT2FontAtlas.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>

extern "C" {
#include "gcommon.h"
#include "gplay.h"
#include "gsong.h"
#include "gorder.h"

extern int songlen[MAX_SONGS][MAX_CHN];
extern unsigned char songorder[MAX_SONGS][MAX_CHN][MAX_SONGLEN+2];
extern int esnum;
extern int eseditpos;
extern int esview;
extern int eschn;
extern int escolumn;
extern int espos[MAX_CHN];
extern int esend[MAX_CHN];
extern int esmarkchn;
extern int esmarkstart;
extern int esmarkend;
extern int editmode;
extern int eamode;
extern int cursorflash;
extern int cursorcolortable[];
}

// GT2 color constants (from gdisplay.h)
#define CNORMAL  8
#define CEDIT    10
#define CPLAYING 12
#define CCOMMAND 7
#define CTITLE   15

// Edit mode constant (from goattrk2.h)
#define EDIT_ORDERLIST 1

CViewGT2OrderList::CViewGT2OrderList(const char *name, float posX, float posY, float posZ,
									 float sizeX, float sizeY, CGT2FontAtlas *fontAtlas)
: CGuiView(posX, posY, posZ, sizeX, sizeY)
{
	this->name = name;
	this->fontAtlas = fontAtlas;
}

CViewGT2OrderList::~CViewGT2OrderList()
{
}

void CViewGT2OrderList::RenderImGui()
{
	PreRenderImGui();
	if (!fontAtlas->TryLoad()) { PostRenderImGui(); return; }

	ImDrawList *dl = ImGui::GetWindowDrawList();
	ImVec2 origin = ImGui::GetCursorScreenPos();

	// Compute visible entries and channels from window size
	ImVec2 avail = ImGui::GetContentRegionAvail();
	float windowW = avail.x;
	float windowH = avail.y;

	// Layout: row 0 = title, rows 1..N = one row per channel
	// Each row: 3 chars channel label + entries (3 chars each) horizontally
	// visibleChannels from HEIGHT, visibleEntries from WIDTH
	int visibleChannels = (int)(windowH / GT2CellH()) - 1; // -1 for title row
	if (visibleChannels > MAX_CHN) visibleChannels = MAX_CHN;
	if (visibleChannels < 1) visibleChannels = 1;

	int visibleEntries = ((int)(windowW / GT2CellW()) - 3) / 3; // -3 for channel label, /3 per entry
	if (visibleEntries < 1) visibleEntries = 1;

	int cc = cursorcolortable[cursorflash];

	char textbuffer[256];

	// Forked from gdisplay.c:246-336
	// Title: "CHN ORDERLIST (SUBTUNE %02X, POS %02X)"
	sprintf(textbuffer, "CHN ORDERLIST (SUBTUNE %02X, POS %02X)", esnum, eseditpos);
	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(0),
				origin.y + GT2RowToPixel(0),
				CTITLE, textbuffer);

	for (int c = 0; c < visibleChannels; c++)
	{
		// Channel label (text-mode row 3+c, col 40+10 -> our layout: row 1+c, col 0)
		sprintf(textbuffer, " %d ", c+1);
		DrawTextGT2(dl, fontAtlas,
					origin.x + GT2ColToPixel(0),
					origin.y + GT2RowToPixel(1+c),
					15, textbuffer);

		for (int d = 0; d < visibleEntries; d++)
		{
			int p = esview+d;
			int color = CNORMAL;

			if (isplaying())
			{
				int chnpos = chn[c].songptr;
				chnpos--;
				if (chnpos < 0) chnpos = 0;
				if ((p == chnpos) && (chn[c].advance)) color = CPLAYING;
			}
			if (p == espos[c]) color = CEDIT;
			if ((esend[c]) && (p == esend[c])) color = CEDIT;

			if ((p < 0) || (p > (songlen[esnum][c]+1)) || (p > MAX_SONGLEN+1))
			{
				sprintf(textbuffer, "   ");
			}
			else
			{
				if (songorder[esnum][c][p] < LOOPSONG)
				{
					if ((songorder[esnum][c][p] < REPEAT) || (p >= songlen[esnum][c]))
					{
						sprintf(textbuffer, "%02X ", songorder[esnum][c][p]);
						if ((p >= songlen[esnum][c]) && (color == CNORMAL)) color = CCOMMAND;
					}
					else
					{
						if (songorder[esnum][c][p] >= TRANSUP)
						{
							sprintf(textbuffer, "+%01X ", songorder[esnum][c][p]&0xf);
							if (color == CNORMAL) color = CCOMMAND;
						}
						else
						{
							if (songorder[esnum][c][p] >= TRANSDOWN)
							{
								sprintf(textbuffer, "-%01X ", 16-(songorder[esnum][c][p] & 0x0f));
								if (color == CNORMAL) color = CCOMMAND;
							}
							else
							{
								sprintf(textbuffer, "R%01X ", (songorder[esnum][c][p]+1) & 0x0f);
								if (color == CNORMAL) color = CCOMMAND;
							}
						}
					}
				}
				if (songorder[esnum][c][p] == LOOPSONG)
				{
					sprintf(textbuffer, "RST");
					if (color == CNORMAL) color = CCOMMAND;
				}
			}

			// Each entry occupies 3 chars; channel label is 3 chars wide at col 0
			// original: printtext(44+10+d*3, 3+c, color, textbuffer)
			DrawTextGT2(dl, fontAtlas,
						origin.x + GT2ColToPixel(3 + d*3),
						origin.y + GT2RowToPixel(1+c),
						(u8)color, textbuffer);

			// Mark selection highlight
			if (c == esmarkchn)
			{
				if (esmarkstart <= esmarkend)
				{
					if ((p >= esmarkstart) && (p <= esmarkend))
					{
						int markLen = (p != esmarkend) ? 3 : 2;
						DrawBgGT2(dl, fontAtlas,
								  origin.x + GT2ColToPixel(3 + d*3),
								  origin.y + GT2RowToPixel(1+c),
								  1, markLen);
					}
				}
				else
				{
					if ((p <= esmarkstart) && (p >= esmarkend))
					{
						int markLen = (p != esmarkstart) ? 3 : 2;
						DrawBgGT2(dl, fontAtlas,
								  origin.x + GT2ColToPixel(3 + d*3),
								  origin.y + GT2RowToPixel(1+c),
								  1, markLen);
					}
				}
			}

			// Cursor highlight for active edit position
			// original: if ((p == eseditpos) && (editmode == EDIT_ORDERLIST) && (eschn == c))
			//               if (!eamode) printbg(44+10+d*3+escolumn, 3+c, cc, 1);
			if ((p == eseditpos) && (editmode == EDIT_ORDERLIST) && (eschn == c))
			{
				if (!eamode) DrawBgGT2(dl, fontAtlas,
									   origin.x + GT2ColToPixel(3 + d*3 + escolumn),
									   origin.y + GT2RowToPixel(1+c),
									   (u8)cc, 1);
			}
		}
	}

	// Mouse click handling — set editmode and cursor position
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		ImVec2 mousePos = ImGui::GetIO().MousePos;
		int gridCol = GT2PixelToCol(mousePos.x - origin.x);
		int gridRow = GT2PixelToRow(mousePos.y - origin.y);

		// Rows 1+ are channels, cols 3+ are entries (3 chars each)
		if (gridRow >= 1 && gridRow <= visibleChannels && gridCol >= 3)
		{
			editmode = EDIT_ORDERLIST;
			int newChn = gridRow - 1;
			int entryIdx = (gridCol - 3) / 3;
			int newPos = esview + entryIdx;
			int newColumn = (gridCol - 3) % 3;
			if (newColumn > 1) newColumn = 1;

			if (newPos > songlen[esnum][newChn] + 1)
				newPos = songlen[esnum][newChn] + 1;

			eschn = newChn;
			eseditpos = newPos;
			escolumn = newColumn;
		}
		else if (gridRow >= 1)
		{
			editmode = EDIT_ORDERLIST;
		}
	}

	PostRenderImGui();
}

bool CViewGT2OrderList::KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	// Pattern-number / repeat-marker / transpose digit entry in the order
	// list uses native gorder.c — forward unconditionally so hex digits
	// reach those handlers under KEY_RENOISE.
	return GT2_HandleRenoiseOrForwardKeyDownToNative(keyCode, isShift, isAlt, isControl, isSuper);
}

bool CViewGT2OrderList::KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	GT2_ForwardKeyUp(keyCode);
	return true;
}

bool CViewGT2OrderList::KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	return KeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}
