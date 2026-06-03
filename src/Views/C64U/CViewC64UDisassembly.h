#ifndef _CVIEWC64UDISASSEMBLY_H_
#define _CVIEWC64UDISASSEMBLY_H_

#include "CGuiView.h"

class CDebugInterfaceC64U;
class CViewDisassembly;
class CViewDataDump;

class CViewC64UDisassembly : public CGuiView
{
public:
	CViewC64UDisassembly(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface);
	virtual ~CViewC64UDisassembly();

	virtual void Render();
	virtual void RenderImGui();
	virtual void DoLogic();

	CDebugInterfaceC64U *debugInterface;
	CViewDisassembly *viewDisassembly;
};

#endif
