#ifndef _CJoystickLayoutNes_H_
#define _CJoystickLayoutNes_H_

#include "CJoystickLayout.h"

// NES gamepad layout: d-pad (left) + SELECT/START (center) + B/A buttons (right)
// Button indices: 0=Up, 1=Down, 2=Left, 3=Right, 4=SELECT, 5=START, 6=B, 7=A
class CJoystickLayoutNes : public CJoystickLayout
{
public:
	virtual ~CJoystickLayoutNes() {}

	virtual int GetButtonCount() override;
	virtual uint32 GetButtonAxis(int buttonIndex) override;
	virtual const char* GetButtonLabel(int buttonIndex) override;

	virtual void RenderPort(int portIndex, bool buttonPressed[], bool buttonSticky[],
							bool buttonClicked[], bool buttonReleased[],
							float availWidth, float availHeight) override;

};

#endif
