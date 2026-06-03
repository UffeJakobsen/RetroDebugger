#include "CViewC64UScreen.h"

CViewC64UScreen::CViewC64UScreen(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterface *debugInterface)
	: CViewEmulatorScreen(name, posX, posY, posZ, sizeX, sizeY, debugInterface)
{
}

CViewC64UScreen::~CViewC64UScreen()
{
}
