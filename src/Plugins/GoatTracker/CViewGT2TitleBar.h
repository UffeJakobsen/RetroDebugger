#ifndef _CViewGT2TitleBar_H_
#define _CViewGT2TitleBar_H_

#include "SYS_Defs.h"
#include "CGuiView.h"

class CGT2FontAtlas;

class CViewGT2TitleBar : public CGuiView
{
public:
	CViewGT2TitleBar(const char *name, float posX, float posY, float posZ,
					 float sizeX, float sizeY, CGT2FontAtlas *fontAtlas);
	virtual ~CViewGT2TitleBar();

	virtual void RenderImGui();
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	static const char *GetDisplayProgramName();
	static u8 GetTitleBarColor();

	CGT2FontAtlas *fontAtlas;
};

#endif
