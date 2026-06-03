#ifndef _CJoystickLayout_H_
#define _CJoystickLayout_H_

#include "SYS_Defs.h"

#define JOYSTICK_MAX_BUTTONS 8

class CJoystickLayout
{
public:
	virtual ~CJoystickLayout() {}

	virtual int GetButtonCount() = 0;
	virtual uint32 GetButtonAxis(int buttonIndex) = 0;
	virtual const char* GetButtonLabel(int buttonIndex) = 0;

	// Render one port's buttons. buttonClicked[]/buttonReleased[] are OUTPUT arrays
	// filled by RenderPort to report which buttons were clicked/released this frame.
	virtual void RenderPort(int portIndex, bool buttonPressed[], bool buttonSticky[],
							bool buttonClicked[], bool buttonReleased[],
							float availWidth, float availHeight) = 0;
};

#endif
