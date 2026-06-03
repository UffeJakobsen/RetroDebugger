#ifndef _CDebugInterfaceAtariTasks_H_
#define _CDebugInterfaceAtariTasks_H_

#include "CDebugInterfaceTask.h"
#include "SYS_Defs.h"

class CDebugInterfaceAtari;

class CDebugInterfaceAtariTaskJoystickEvent : public CDebugInterfaceTask
{
public:
	CDebugInterfaceAtariTaskJoystickEvent(CDebugInterfaceAtari *debugInterface, u8 buttonState, int port, u32 axis);
	virtual void ExecuteTask();

	CDebugInterfaceAtari *debugInterface;
	u8 buttonState;
	int port;
	u32 axis;
};

class CDebugInterfaceAtariTaskKeyboardEvent : public CDebugInterfaceTask
{
public:
	CDebugInterfaceAtariTaskKeyboardEvent(CDebugInterfaceAtari *debugInterface, u8 buttonState, u32 mtKeyCode);
	virtual void ExecuteTask();

	CDebugInterfaceAtari *debugInterface;
	u8 buttonState;
	u32 mtKeyCode;
};

#endif
