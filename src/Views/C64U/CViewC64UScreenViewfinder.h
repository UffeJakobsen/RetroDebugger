#ifndef _CVIEWC64USCREENVIEWFINDER_H_
#define _CVIEWC64USCREENVIEWFINDER_H_

#include "CGuiView.h"

class CViewC64UScreen;
class CDebugInterfaceC64U;

class CViewC64UScreenViewfinder : public CGuiView
{
public:
	CViewC64UScreenViewfinder(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CViewC64UScreen *viewScreen, CDebugInterfaceC64U *debugInterface);

	virtual bool DoScrollWheel(float deltaX, float deltaY);
	virtual void Render();
	virtual void RenderImGui();

	CViewC64UScreen *viewScreen;
	CDebugInterfaceC64U *debugInterface;
	float zoom;
};

#endif
