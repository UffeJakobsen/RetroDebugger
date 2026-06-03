#ifndef _CVIEWC64UMEDIASTATUS_H_
#define _CVIEWC64UMEDIASTATUS_H_

#include "CGuiView.h"

#include <cstdint>

class CDebugInterfaceC64U;

class CViewC64UMediaStatus : public CGuiView
{
public:
	CViewC64UMediaStatus(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface);

	virtual void Render();
	virtual void RenderImGui();

	CDebugInterfaceC64U *debugInterface;
	uint64_t lastDriveInfoFetchTime;
};

#endif
