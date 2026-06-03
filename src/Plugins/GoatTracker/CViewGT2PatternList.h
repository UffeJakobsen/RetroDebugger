#ifndef _CViewGT2PatternList_H_
#define _CViewGT2PatternList_H_

#include "SYS_Defs.h"
#include "CGuiView.h"

class CGT2FontAtlas;

// Vertical clone of the GT2 Order List. Each row is one song order position.
// The editable left column is the "block index" = pattern number / channel
// count (the Renoise / Bulk-Pattern-Number convention: position i, channel c
// -> pattern i*numChannels + c). The real per-channel patterns are shown to
// the right, greyed and read-only.
class CViewGT2PatternList : public CGuiView
{
public:
	CViewGT2PatternList(const char *name, float posX, float posY, float posZ,
						float sizeX, float sizeY, CGT2FontAtlas *fontAtlas);
	virtual ~CViewGT2PatternList();

	virtual void RenderImGui();
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);

	// Context-menu / Ctrl+X-C-V order-list operations. Public for the GT2 tests.
	void CopySelection();        // copy the selected rows (or cursor row)
	void CutSelection();         // copy + delete the selected rows
	void PasteRows();            // insert the copied rows at the cursor
	void DuplicateSelection();   // deep-duplicate the selection into fresh patterns
	void RemoveUnusedPatterns(); // drop unused blocks, compact + renumber
	void SortPatterns();         // renumber blocks to match the order-list sequence

	CGT2FontAtlas *fontAtlas;

private:
	int scrollPos;   // first visible order position
	int editCol;     // 0 = high nibble, 1 = low nibble of the block field

	int selAnchor;     // mouse row-selection anchor; -1 = no selection
	int selEnd;        // mouse row-selection drag-end row
	bool selecting;    // a mouse drag-select is in progress
	int dragScrollPos; // scrollPos frozen for the duration of a drag-select

	int MaxOrderLen() const;
	void MoveCursor(int delta);
	void SyncPatternEditorToCursor();
	void ClearSelection();
	bool CanEditBlock() const;
	void WriteBlock(int block);
	void EditBlockNibble(int hexValue);
	void AdjustBlock(int delta);
	void SelectionRange(int *lo, int *hi) const;
	void DeleteRowRange(int lo, int hi);
	void RenumberBlocks(const int *newBlockOfOld);
};

#endif
