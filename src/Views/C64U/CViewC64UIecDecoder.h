#ifndef _CVIEWC64UIECDECODER_H_
#define _CVIEWC64UIECDECODER_H_

#include "CGuiView.h"

class CDebugInterfaceC64U;

class CViewC64UIecDecoder : public CGuiView
{
public:
	CViewC64UIecDecoder(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface);

	virtual void Render();
	virtual void RenderImGui();

	CDebugInterfaceC64U *debugInterface;
};

#endif
