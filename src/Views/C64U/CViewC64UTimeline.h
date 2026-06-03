#ifndef _CVIEWC64UTIMELINE_H_
#define _CVIEWC64UTIMELINE_H_

#include "CGuiView.h"

class CDebugInterfaceC64U;

class CViewC64UTimeline : public CGuiView
{
public:
	CViewC64UTimeline(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface);

	virtual void Render();
	virtual void RenderImGui();

	CDebugInterfaceC64U *debugInterface;

	float cyclesPerPixel;
};

#endif
