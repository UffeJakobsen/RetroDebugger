#ifndef _CVIEWC64UTERMINAL_H_
#define _CVIEWC64UTERMINAL_H_

#include "CGuiViewTerminal.h"

class CDebugInterfaceC64U;
class C64UTelnetClient;

class CViewC64UTerminal : public CGuiViewTerminal
{
public:
	CViewC64UTerminal(const char *name, float posX, float posY, float posZ,
					  float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface);
	virtual ~CViewC64UTerminal();

	virtual void RenderImGui();

	CDebugInterfaceC64U *debugInterface;
	C64UTelnetClient *telnetClient;

private:
	bool autoConnectAttempted;
};

#endif
