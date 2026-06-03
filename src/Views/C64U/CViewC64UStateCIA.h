#ifndef _CVIEWC64USTATECIAS_H_
#define _CVIEWC64USTATECIAS_H_

#include "CGuiView.h"

class CDebugInterfaceC64U;

class CViewC64UStateCIA : public CGuiView
{
public:
	CViewC64UStateCIA(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface);

	virtual void Render();
	virtual void RenderImGui();

	CDebugInterfaceC64U *debugInterface;
};

#endif
