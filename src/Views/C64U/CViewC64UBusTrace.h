#ifndef _CVIEWC64UBUSTRACE_H_
#define _CVIEWC64UBUSTRACE_H_

#include "CGuiView.h"

class CDebugInterfaceC64U;

class CViewC64UBusTrace : public CGuiView
{
public:
	CViewC64UBusTrace(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface);

	virtual void Render();
	virtual void RenderImGui();

	CDebugInterfaceC64U *debugInterface;

	bool autoScroll;
};

#endif
