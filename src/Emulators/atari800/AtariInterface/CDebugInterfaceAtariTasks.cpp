#include "CDebugInterfaceAtariTasks.h"
#include "CDebugInterfaceAtari.h"
#include "CSnapshotsManager.h"
#include "CByteBuffer.h"
#include "DebuggerDefs.h"
#include "SYS_Main.h"
#include "SYS_KeyCodes.h"

extern "C" {
#include "input.h"
#include "akey.h"
}

extern int key_control;

// --- Joystick ---

CDebugInterfaceAtariTaskJoystickEvent::CDebugInterfaceAtariTaskJoystickEvent(
	CDebugInterfaceAtari *debugInterface, u8 buttonState, int port, u32 axis)
{
	this->debugInterface = debugInterface;
	this->buttonState = buttonState;
	this->port = port;
	this->axis = axis;
}

void CDebugInterfaceAtariTaskJoystickEvent::ExecuteTask()
{
	// Record to input events buffer (with dedup — replace existing event for same port)
	if (debugInterface->snapshotsManager->isStoreInputEventsEnabled)
	{
		CByteBuffer *buf = debugInterface->GetInputEventsBufferForCurrentCycle();
		buf->Rewind();

		bool found = false;
		while (buf->IsEof() == false)
		{
			u8 eventType = buf->GetU8();
			if (eventType == DEBUGGER_EVENT_TYPE_JOYSTICK)
			{
				int bufPort = buf->GetI32();
				if (bufPort == port)
				{
					int len = buf->length;
					buf->PutU32(axis);
					buf->PutU8(buttonState);
					buf->length = len;
					found = true;
				}
				else
				{
					buf->GetU32();
					buf->GetU8();
				}
			}
			else if (eventType == DEBUGGER_EVENT_TYPE_KEYBOARD)
			{
				buf->GetU32();
				buf->GetU8();
			}
			else break;
		}

		if (!found)
		{
			buf->PutU8(DEBUGGER_EVENT_TYPE_JOYSTICK);
			buf->PutI32(port);
			buf->PutU32(axis);
			buf->PutU8(buttonState);
		}
	}

	// Apply to joystick state (same conflict resolution as old JoystickDown/Up)
	if (buttonState == DEBUGGER_EVENT_BUTTON_DOWN)
	{
		if (axis == JOYPAD_N && (debugInterface->joystickState[port] & JOYPAD_S))
			debugInterface->joystickState[port] &= ~JOYPAD_S;
		if (axis == JOYPAD_S && (debugInterface->joystickState[port] & JOYPAD_N))
			debugInterface->joystickState[port] &= ~JOYPAD_N;
		if (axis == JOYPAD_E && (debugInterface->joystickState[port] & JOYPAD_W))
			debugInterface->joystickState[port] &= ~JOYPAD_W;
		if (axis == JOYPAD_W && (debugInterface->joystickState[port] & JOYPAD_E))
			debugInterface->joystickState[port] &= ~JOYPAD_E;
		debugInterface->joystickState[port] |= axis;
	}
	else
	{
		debugInterface->joystickState[port] &= ~axis;
	}
}

// --- Keyboard ---

CDebugInterfaceAtariTaskKeyboardEvent::CDebugInterfaceAtariTaskKeyboardEvent(
	CDebugInterfaceAtari *debugInterface, u8 buttonState, u32 mtKeyCode)
{
	this->debugInterface = debugInterface;
	this->buttonState = buttonState;
	this->mtKeyCode = mtKeyCode;
}

void CDebugInterfaceAtariTaskKeyboardEvent::ExecuteTask()
{
	// Record to input events buffer
	if (debugInterface->snapshotsManager->isStoreInputEventsEnabled)
	{
		CByteBuffer *buf = debugInterface->GetInputEventsBufferForCurrentCycle();
		buf->PutU8(DEBUGGER_EVENT_TYPE_KEYBOARD);
		buf->PutU32(mtKeyCode);
		buf->PutU8(buttonState);
	}

	// Apply keyboard state directly (same as KeyboardDown/Up without plugin loop)
	if (buttonState == DEBUGGER_EVENT_BUTTON_DOWN)
	{
		int shiftctrl = 0;
		if (mtKeyCode == MTKEY_LSHIFT || mtKeyCode == MTKEY_RSHIFT)
			INPUT_key_shift = 1;
		if (mtKeyCode == MTKEY_LCONTROL || mtKeyCode == MTKEY_RCONTROL)
			key_control = 1;
		if (mtKeyCode == MTKEY_F2) { INPUT_key_consol &= ~INPUT_CONSOL_OPTION; }
		else if (mtKeyCode == MTKEY_F3) { INPUT_key_consol &= ~INPUT_CONSOL_SELECT; }
		else if (mtKeyCode == MTKEY_F4) { INPUT_key_consol &= ~INPUT_CONSOL_START; }
		else
		{
			if (INPUT_key_shift) shiftctrl ^= AKEY_SHFT;
			INPUT_key_code = debugInterface->MapMTKeyToAKey(mtKeyCode, shiftctrl, key_control);
		}
	}
	else
	{
		if (mtKeyCode == MTKEY_F2) { INPUT_key_consol |= INPUT_CONSOL_OPTION; }
		else if (mtKeyCode == MTKEY_F3) { INPUT_key_consol |= INPUT_CONSOL_SELECT; }
		else if (mtKeyCode == MTKEY_F4) { INPUT_key_consol |= INPUT_CONSOL_START; }
		else { INPUT_key_code = AKEY_NONE; }
		if (mtKeyCode == MTKEY_LSHIFT || mtKeyCode == MTKEY_RSHIFT)
			INPUT_key_shift = 0;
		if (mtKeyCode == MTKEY_LCONTROL || mtKeyCode == MTKEY_RCONTROL)
			key_control = 0;
	}
}
