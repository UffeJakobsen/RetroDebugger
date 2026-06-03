#ifndef _CViewInputEventsTable_H_
#define _CViewInputEventsTable_H_

#include "SYS_Defs.h"
#include "CGuiView.h"

class CDebugInterface;
class CSnapshotsManager;

class CViewInputEventsTable : public CGuiView
{
public:
	CViewInputEventsTable(const char *name, float posX, float posY, float posZ,
						  float sizeX, float sizeY, CDebugInterface *debugInterface);
	virtual ~CViewInputEventsTable();

	virtual void RenderImGui();

	CDebugInterface *debugInterface;
	CSnapshotsManager *snapshotsManager;

	int selectedRow;
};

#endif
