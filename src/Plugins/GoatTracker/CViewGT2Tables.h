#ifndef _CViewGT2Tables_H_
#define _CViewGT2Tables_H_

#include "SYS_Defs.h"
#include "CGuiView.h"
#include <vector>

class CGT2FontAtlas;

class CViewGT2Tables : public CGuiView
{
public:
	CViewGT2Tables(const char *name, float posX, float posY, float posZ,
				   float sizeX, float sizeY, CGT2FontAtlas *fontAtlas);
	virtual ~CViewGT2Tables();

	virtual void RenderImGui();
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);

	bool CanUndoTableEdit() const;
	bool CanRedoTableEdit() const;
	bool UndoTableEdit();
	bool RedoTableEdit();
	void ClearTableUndoHistory();
	void BeginTableUndoStep();
	bool CommitTableUndoStep();
	void CancelTableUndoStep();
	u8 ApplyTableCellBackground(u8 colorIndex, int backgroundColor) const;

	CGT2FontAtlas *fontAtlas;

private:
	struct TableUndoSnapshot
	{
		TableUndoSnapshot();

		std::vector<u8> leftTableData;
		std::vector<u8> rightTableData;
		std::vector<u8> patternData;
		std::vector<u8> instrumentData;
		std::vector<int> tableViews;
		int tableNum;
		int tablePos;
		int tableColumn;
		int tableLock;
		int tableMarkNum;
		int tableMarkStart;
		int tableMarkEnd;
	};

	TableUndoSnapshot CaptureTableUndoSnapshot() const;
	void RestoreTableUndoSnapshot(const TableUndoSnapshot &snapshot);
	bool TableUndoSnapshotsHaveSameData(const TableUndoSnapshot &a, const TableUndoSnapshot &b) const;
	void PushTableUndoSnapshot(const TableUndoSnapshot &snapshot);
	bool CommitTableUndoSnapshotIfChanged(const TableUndoSnapshot &before);

	std::vector<TableUndoSnapshot> tableUndoStack;
	std::vector<TableUndoSnapshot> tableRedoStack;
	TableUndoSnapshot pendingTableUndoSnapshot;
	bool pendingTableUndoSnapshotActive;
};

#endif
