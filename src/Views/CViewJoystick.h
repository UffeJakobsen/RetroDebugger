#ifndef _CViewJoystick_H_
#define _CViewJoystick_H_

#include "SYS_Defs.h"
#include "CGuiView.h"
#include "CJoystickLayout.h"

class CDebugInterface;

#define JOYSTICK_MAX_PORTS 4

class CViewJoystick : public CGuiView
{
public:
	CViewJoystick(const char *name, float posX, float posY, float posZ,
				  float sizeX, float sizeY,
				  CDebugInterface *debugInterface, CJoystickLayout *layout, int maxPorts);
	virtual ~CViewJoystick();

	virtual void RenderImGui();

	CDebugInterface *debugInterface;
	CJoystickLayout *layout;

	int maxPorts;
	int visiblePortConfig; // 0..maxPorts-1 = single port, maxPorts = show all

	bool stickyMode;
	bool buttonSticky[JOYSTICK_MAX_PORTS][JOYSTICK_MAX_BUTTONS];
	bool buttonPressed[JOYSTICK_MAX_PORTS][JOYSTICK_MAX_BUTTONS];

	// Track which button is held down via mouse (for non-sticky release)
	int mouseHeldPort;
	int mouseHeldButton;

private:
	void RenderTopBar();
	void RenderSinglePort(int portIndex, float width, float height);
	void UpdateButtonStates();
	void HandleButtonClick(int portIndex, int buttonIndex);
	void HandleButtonRelease(int portIndex, int buttonIndex);
	void RenderInputSourceCombo(int portIndex);
};

#endif
