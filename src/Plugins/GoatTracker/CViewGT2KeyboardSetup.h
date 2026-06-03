#ifndef _CViewGT2KeyboardSetup_H_
#define _CViewGT2KeyboardSetup_H_

#include "SYS_Defs.h"
#include "CGuiView.h"
#include "CSystemFileDialogCallback.h"

class CViewGT2KeyboardSetup : public CGuiView, public CSystemFileDialogCallback
{
public:
	CViewGT2KeyboardSetup(const char *name, float posX, float posY, float posZ,
						   float sizeX, float sizeY);
	virtual ~CViewGT2KeyboardSetup();

	virtual void RenderImGui();

	// Key capture state
	int captureTarget;     // -1 = not capturing, 0-14 = tbl1 index, 100-116 = tbl2 index
	bool isCapturing;

	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);

	// File dialog callbacks
	void OpenSaveDialog();
	void OpenLoadDialog();
	virtual void SystemDialogFileOpenSelected(CSlrString *path);
	virtual void SystemDialogFileOpenCancelled();
	virtual void SystemDialogFileSaveSelected(CSlrString *path);
	virtual void SystemDialogFileSaveCancelled();

	bool isSaveDialog;

	void SaveLayout(const char *path);
	void LoadLayout(const char *path);

	const char *KeyName(unsigned char keyCode);
};

#endif
