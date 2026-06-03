#include "CViewGT2Tables.h"
#include "C64DebuggerPluginGoatTracker.h"
#include "CViewGT2Patterns.h"
#include "GT2ViewCommon.h"
#include "GT2RenderHelper.h"
#include "CGT2FontAtlas.h"
#include "imgui.h"
#include <cstring>
#include <cstdio>

extern "C" {
#include "gcommon.h"
#include "gtable.h"
#include "gsong.h"

extern unsigned char ltable[MAX_TABLES][MAX_TABLELEN];
extern unsigned char rtable[MAX_TABLES][MAX_TABLELEN];
extern int editmode, cursorflash, eamode;
extern int cursorcolortable[];
}

static const size_t kGT2TableUndoLimit = 32;

// GT2 color constants (from gdisplay.h)
#define CNORMAL  8
#define CEDIT    10
#define CCOMMAND 7
#define CTITLE   15

// Edit mode constant (from goattrk2.h)
#define EDIT_TABLES 3

// Table indices (from gcommon.h)
// WTBL=0, PTBL=1, FTBL=2, STBL=3 — included via gcommon.h

u8 CViewGT2Tables::ApplyTableCellBackground(u8 colorIndex, int backgroundColor) const
{
	if (backgroundColor < 0) return colorIndex;
	return (u8)((colorIndex & 0x0F) | ((backgroundColor & 0x0F) << 4));
}

static void DrawTableTextGT2(CViewGT2Tables *view, ImDrawList *dl, CGT2FontAtlas *fontAtlas,
							 float px, float py, int tableNum, int tableRow,
							 u8 colorIndex, const char *text, int cursorBackgroundColor)
{
	char ch[2] = { 0, 0 };
	for (int i = 0; text[i]; i++)
	{
		int backgroundColor = -1;
		if (etmarknum == tableNum)
		{
			bool isMarked = etmarkstart <= etmarkend
				? tableRow >= etmarkstart && tableRow <= etmarkend
				: tableRow <= etmarkstart && tableRow >= etmarkend;
			if (isMarked && i >= 3 && i <= 7)
				backgroundColor = 1;
		}

		if (editmode == EDIT_TABLES && !eamode && etnum == tableNum && etpos == tableRow)
		{
			int cursorColumn = 3 + (etcolumn & 1) + (etcolumn / 2) * 3;
			if (i == cursorColumn)
				backgroundColor = cursorBackgroundColor;
		}

		ch[0] = text[i];
		DrawTextGT2(dl, fontAtlas, px + i * GT2CellW(), py,
					view->ApplyTableCellBackground(colorIndex, backgroundColor), ch);
	}
}

CViewGT2Tables::TableUndoSnapshot::TableUndoSnapshot()
: tableNum(0)
, tablePos(0)
, tableColumn(0)
, tableLock(1)
, tableMarkNum(-1)
, tableMarkStart(0)
, tableMarkEnd(0)
{
}

CViewGT2Tables::CViewGT2Tables(const char *name, float posX, float posY, float posZ,
								float sizeX, float sizeY, CGT2FontAtlas *fontAtlas)
: CGuiView(posX, posY, posZ, sizeX, sizeY)
{
	this->name = name;
	this->fontAtlas = fontAtlas;
	this->pendingTableUndoSnapshotActive = false;
}

CViewGT2Tables::~CViewGT2Tables()
{
}

CViewGT2Tables::TableUndoSnapshot CViewGT2Tables::CaptureTableUndoSnapshot() const
{
	TableUndoSnapshot snapshot;
	const u8 *leftBegin = &ltable[0][0];
	const u8 *rightBegin = &rtable[0][0];
	const u8 *patternBegin = &pattern[0][0];
	const u8 *instrumentBegin = reinterpret_cast<const u8 *>(&ginstr[0]);
	snapshot.leftTableData.assign(leftBegin, leftBegin + sizeof(ltable));
	snapshot.rightTableData.assign(rightBegin, rightBegin + sizeof(rtable));
	snapshot.patternData.assign(patternBegin, patternBegin + sizeof(pattern));
	snapshot.instrumentData.assign(instrumentBegin, instrumentBegin + sizeof(ginstr));
	snapshot.tableViews.assign(etview, etview + MAX_TABLES);
	snapshot.tableNum = etnum;
	snapshot.tablePos = etpos;
	snapshot.tableColumn = etcolumn;
	snapshot.tableLock = etlock;
	snapshot.tableMarkNum = etmarknum;
	snapshot.tableMarkStart = etmarkstart;
	snapshot.tableMarkEnd = etmarkend;
	return snapshot;
}

void CViewGT2Tables::RestoreTableUndoSnapshot(const TableUndoSnapshot &snapshot)
{
	if (snapshot.leftTableData.size() == sizeof(ltable))
		memcpy(&ltable[0][0], snapshot.leftTableData.data(), sizeof(ltable));
	if (snapshot.rightTableData.size() == sizeof(rtable))
		memcpy(&rtable[0][0], snapshot.rightTableData.data(), sizeof(rtable));
	if (snapshot.patternData.size() == sizeof(pattern))
		memcpy(&pattern[0][0], snapshot.patternData.data(), sizeof(pattern));
	if (snapshot.instrumentData.size() == sizeof(ginstr))
		memcpy(&ginstr[0], snapshot.instrumentData.data(), sizeof(ginstr));
	if (snapshot.tableViews.size() == MAX_TABLES)
		memcpy(etview, snapshot.tableViews.data(), sizeof(int) * MAX_TABLES);
	etnum = snapshot.tableNum;
	etpos = snapshot.tablePos;
	etcolumn = snapshot.tableColumn;
	etlock = snapshot.tableLock;
	etmarknum = snapshot.tableMarkNum;
	etmarkstart = snapshot.tableMarkStart;
	etmarkend = snapshot.tableMarkEnd;
}

bool CViewGT2Tables::TableUndoSnapshotsHaveSameData(const TableUndoSnapshot &a, const TableUndoSnapshot &b) const
{
	return a.leftTableData == b.leftTableData
		&& a.rightTableData == b.rightTableData
		&& a.patternData == b.patternData
		&& a.instrumentData == b.instrumentData;
}

void CViewGT2Tables::PushTableUndoSnapshot(const TableUndoSnapshot &snapshot)
{
	tableUndoStack.push_back(snapshot);
	if (tableUndoStack.size() > kGT2TableUndoLimit)
		tableUndoStack.erase(tableUndoStack.begin());
	tableRedoStack.clear();
}

bool CViewGT2Tables::CommitTableUndoSnapshotIfChanged(const TableUndoSnapshot &before)
{
	TableUndoSnapshot after = CaptureTableUndoSnapshot();
	if (TableUndoSnapshotsHaveSameData(before, after))
		return false;
	PushTableUndoSnapshot(before);
	if (pluginGoatTracker && pluginGoatTracker->viewPatterns)
		pluginGoatTracker->viewPatterns->ClearPatternUndoHistory();
	return true;
}

bool CViewGT2Tables::CanUndoTableEdit() const
{
	return !tableUndoStack.empty();
}

bool CViewGT2Tables::CanRedoTableEdit() const
{
	return !tableRedoStack.empty();
}

bool CViewGT2Tables::UndoTableEdit()
{
	if (tableUndoStack.empty())
		return false;
	TableUndoSnapshot current = CaptureTableUndoSnapshot();
	TableUndoSnapshot previous = tableUndoStack.back();
	tableUndoStack.pop_back();
	tableRedoStack.push_back(current);
	if (tableRedoStack.size() > kGT2TableUndoLimit)
		tableRedoStack.erase(tableRedoStack.begin());
	RestoreTableUndoSnapshot(previous);
	return true;
}

bool CViewGT2Tables::RedoTableEdit()
{
	if (tableRedoStack.empty())
		return false;
	TableUndoSnapshot current = CaptureTableUndoSnapshot();
	TableUndoSnapshot next = tableRedoStack.back();
	tableRedoStack.pop_back();
	tableUndoStack.push_back(current);
	if (tableUndoStack.size() > kGT2TableUndoLimit)
		tableUndoStack.erase(tableUndoStack.begin());
	RestoreTableUndoSnapshot(next);
	return true;
}

void CViewGT2Tables::ClearTableUndoHistory()
{
	tableUndoStack.clear();
	tableRedoStack.clear();
	pendingTableUndoSnapshotActive = false;
}

void CViewGT2Tables::BeginTableUndoStep()
{
	if (pendingTableUndoSnapshotActive)
		return;
	pendingTableUndoSnapshot = CaptureTableUndoSnapshot();
	pendingTableUndoSnapshotActive = true;
}

bool CViewGT2Tables::CommitTableUndoStep()
{
	if (!pendingTableUndoSnapshotActive)
		return false;
	TableUndoSnapshot before = pendingTableUndoSnapshot;
	pendingTableUndoSnapshotActive = false;
	return CommitTableUndoSnapshotIfChanged(before);
}

void CViewGT2Tables::CancelTableUndoStep()
{
	pendingTableUndoSnapshotActive = false;
}

void CViewGT2Tables::RenderImGui()
{
	PreRenderImGui();
	if (!fontAtlas->TryLoad()) { PostRenderImGui(); return; }

	ImDrawList *dl = ImGui::GetWindowDrawList();
	ImVec2 origin = ImGui::GetCursorScreenPos();

	// Compute visible rows from window height
	ImVec2 avail = ImGui::GetContentRegionAvail();
	float windowH = avail.y;

	int visibleRows = (int)(windowH / GT2CellH()) - 1; // -1 for header row
	if (visibleRows < 1) visibleRows = 1;

	int cc = cursorcolortable[cursorflash];

	char textbuffer[64];

	// Header row — forked from gdisplay.c:389-390
	DrawTextGT2(dl, fontAtlas,
				origin.x + GT2ColToPixel(0),
				origin.y + GT2RowToPixel(0),
				CTITLE, "WAVE TBL  PULSETBL  FILT.TBL  SPEEDTBL");

	// Table rows — forked from gdisplay.c:392-436
	for (int c = 0; c < MAX_TABLES; c++)
	{
		for (int d = 0; d < visibleRows; d++)
		{
			int p = etview[c] + d;
			int color = CNORMAL;

			switch (c)
			{
				case WTBL: if (ltable[c][p] >= WAVECMD)                                    color = CCOMMAND; break;
				case PTBL: if (ltable[c][p] >= 0x80)                                       color = CCOMMAND; break;
				case FTBL: if ((ltable[c][p] >= 0x80) || ((!ltable[c][p]) && (rtable[c][p]))) color = CCOMMAND; break;
				default:   break;
			}

			if ((p == etpos) && (etnum == c)) color = CEDIT;

			// Overlap highlight: count how many instruments cover (c, p)
			int coverCount = 0;
			for (int ii = 1; ii < MAX_INSTR; ii++)
			{
				if (!ginstr[ii].ptr[c]) continue;
				int ip = ginstr[ii].ptr[c] - 1;
				int il = gettablepartlen(c, ip);
				if (p >= ip && p < ip + il) coverCount++;
			}
			if (coverCount >= 2)
			{
				ImVec2 rmin(origin.x + GT2ColToPixel(10 * c), origin.y + GT2RowToPixel(1 + d));
				ImVec2 rmax(origin.x + GT2ColToPixel(10 * c + 8), origin.y + GT2RowToPixel(2 + d));
				dl->AddRectFilled(rmin, rmax, IM_COL32(255, 0, 0, 40));
			}

			sprintf(textbuffer, "%02X:%02X %02X", p + 1, ltable[c][p], rtable[c][p]);

			DrawTableTextGT2(this, dl, fontAtlas,
							 origin.x + GT2ColToPixel(10 * c),
							 origin.y + GT2RowToPixel(1 + d),
							 c, p, (u8)color, textbuffer, cc);
		}
	}

	// Mouse click handling — set editmode and cursor position
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		ImVec2 mousePos = ImGui::GetIO().MousePos;
		int gridCol = GT2PixelToCol(mousePos.x - origin.x);
		int gridRow = GT2PixelToRow(mousePos.y - origin.y);

		// Determine which table (0-3) from column
		// Each table occupies 10 columns: cols 0-7 for table 0, 10-17 for table 1, etc.
		// Value area within each table: cols 3-7 (XX XX format after "XX:")
		int tableIdx = gridCol / 10;
		int colInTable = gridCol - tableIdx * 10;

		if (tableIdx >= 0 && tableIdx < MAX_TABLES && gridRow >= 1 &&
			colInTable >= 0 && colInTable <= 7)
		{
			int newpos = (gridRow - 1) + etview[tableIdx];
			if (newpos >= 0 && newpos < MAX_TABLELEN)
			{
				editmode = EDIT_TABLES;
				etnum = tableIdx;
				etpos = newpos;
				// Map column within table to etcolumn
				// Format: "XX:XX XX" — cols 0-1=row#, 2=colon, 3-4=left val, 5=space, 6-7=right val
				// etcolumn: 0=left hi, 1=left lo, 2=right hi, 3=right lo
				if (colInTable <= 3)
					etcolumn = 0;
				else if (colInTable == 4)
					etcolumn = 1;
				else if (colInTable == 6)
					etcolumn = 2;
				else if (colInTable == 7)
					etcolumn = 3;
				else
					etcolumn = 0;
			}
		}
		else if (gridRow >= 1)
		{
			// Clicked in table area but not on a value — still switch edit mode
			editmode = EDIT_TABLES;
		}
	}

	PostRenderImGui();
}

bool CViewGT2Tables::KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	// The tables view owns Ctrl+Z / Ctrl+Y for its own table-edit history,
	// ahead of the shared global (pattern) undo dispatcher.
	if ((isControl || isSuper) && !isAlt)
	{
		if (!isShift && (keyCode == 'z' || keyCode == 'Z'))
		{
			UndoTableEdit();
			return true;
		}
		if (keyCode == 'y' || keyCode == 'Y')
		{
			RedoTableEdit();
			return true;
		}
	}
	// Native gtable.c is the source of truth for table value editing.
	// Forward unconditionally to native so hex digits / Q/A/W/S transpose
	// / Enter etc. reach the legacy handler.
	return GT2_HandleRenoiseOrForwardKeyDownToNative(keyCode, isShift, isAlt, isControl, isSuper);
}

bool CViewGT2Tables::KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	GT2_ForwardKeyUp(keyCode);
	return true;
}

bool CViewGT2Tables::KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	return KeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}
