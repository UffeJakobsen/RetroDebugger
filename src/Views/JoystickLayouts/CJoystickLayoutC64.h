#ifndef _CJoystickLayoutC64_H_
#define _CJoystickLayoutC64_H_

#include "CJoystickLayout.h"

// C64/Atari joystick layout: circular d-pad + FIRE button
// Button indices: 0=Up, 1=Down, 2=Left, 3=Right, 4=FIRE
class CJoystickLayoutC64 : public CJoystickLayout
{
public:
	virtual ~CJoystickLayoutC64() {}

	virtual int GetButtonCount() override;
	virtual uint32 GetButtonAxis(int buttonIndex) override;
	virtual const char* GetButtonLabel(int buttonIndex) override;

	virtual void RenderPort(int portIndex, bool buttonPressed[], bool buttonSticky[],
							bool buttonClicked[], bool buttonReleased[],
							float availWidth, float availHeight) override;

};

#endif
