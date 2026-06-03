#ifndef _CViewGT2Instrument_H_
#define _CViewGT2Instrument_H_

#include "SYS_Defs.h"
#include "CGuiView.h"

class CGT2FontAtlas;

int GT2WtblSelectedWaveformIndex(unsigned char leftValue);
int GT2WtblSelectedCommandIndex(unsigned char leftValue);
unsigned char GT2WtblApplyWaveformSelection(unsigned char leftValue, int waveformIndex);
unsigned char GT2WtblApplyCommandSelection(int commandIndex);
bool GT2WtblContextHasValidRow(int tableStart, int tableLength, int position);
int GT2WtblContextRowFromClick(int tableStart, int tableLength, int clickedOffset);
bool GT2WtblContextShouldEnterEditMode(int tableStart, int tableLength, int clickedOffset);
bool GT2WtblContextShouldCreateRowOnSelection(int tableStart, int tableLength, int clickedOffset);
int GT2WtblContextSelectableFlags();
int GT2InstrumentTableFromGridCol(int gridCol);
int GT2WtblContextTextColorMode(bool selected, bool hovered);

class CViewGT2Instrument : public CGuiView
{
public:
	CViewGT2Instrument(const char *name, float posX, float posY, float posZ,
					   float sizeX, float sizeY, CGT2FontAtlas *fontAtlas);
	virtual ~CViewGT2Instrument();

	virtual void RenderImGui();
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	u8 ApplyInstrumentCellBackground(u8 colorIndex, int backgroundColor) const;
	// Routes ENTER on an instrument's table-pointer field (eipos 2..5) into
	// the corresponding ImGui table view — gototable() when the pointer is
	// non-zero, InsertTableRow() / AllocateSpeedtableEntry() when it's zero.
	// Returns true if consumed. Callable from any focused view (legacy
	// text-mode included) so the user doesn't drop into the legacy editor.
	bool HandleInstrumentTablePointerEnter(bool isShift, bool isAlt, bool isControl, bool isSuper);

	CGT2FontAtlas *fontAtlas;

private:
	void InsertTableRow();   // wavetable / pulsetable / filtertable only
	void DeleteTableRow();
	void AllocateSpeedtableEntry();
	void RenderTablePalette();
	bool CreateWavetableRowWithLeft(unsigned char value);
	void SetWavetableLeft(unsigned char value);
	void SetWavetableRight(unsigned char value);
	// One-cell undo writes to the focused filter-table row, used by the
	// right-click context menu's passband / resonance / routing editor.
	void SetFiltertableLeft(unsigned char value);
	void SetFiltertableRight(unsigned char value);
	void SetFirstwave(unsigned char value);
	bool tableContextHasClickedRow;
	bool tableContextCanEdit;
};

#endif
