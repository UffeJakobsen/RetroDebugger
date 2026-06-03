#ifndef _CVIEWC64UCONNECTIONSTATUS_H_
#define _CVIEWC64UCONNECTIONSTATUS_H_

#include "CGuiView.h"

class CDebugInterfaceC64U;

class CViewC64UConnectionStatus : public CGuiView
{
public:
	CViewC64UConnectionStatus(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface);

	virtual void Render();
	virtual void RenderImGui();

	CDebugInterfaceC64U *debugInterface;
};

#endif
