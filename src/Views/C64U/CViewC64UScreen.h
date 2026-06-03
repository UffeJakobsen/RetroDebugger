#ifndef _CVIEWC64USCREEN_H_
#define _CVIEWC64USCREEN_H_

#include "../CViewEmulatorScreen.h"

class CViewC64UScreen : public CViewEmulatorScreen
{
public:
	CViewC64UScreen(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterface *debugInterface);
	virtual ~CViewC64UScreen();
};

#endif
