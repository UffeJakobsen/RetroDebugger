#ifndef _CViewGT2OrderList_H_
#define _CViewGT2OrderList_H_

#include "SYS_Defs.h"
#include "CGuiView.h"

class CGT2FontAtlas;

class CViewGT2OrderList : public CGuiView
{
public:
	CViewGT2OrderList(const char *name, float posX, float posY, float posZ,
					  float sizeX, float sizeY, CGT2FontAtlas *fontAtlas);
	virtual ~CViewGT2OrderList();

	virtual void RenderImGui();
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);

	CGT2FontAtlas *fontAtlas;
};

#endif
