#include "CDebugInterfaceNesTasks.h"
#include "NesWrapper.h"
#include "CDebugInterfaceNes.h"
#include "CSnapshotsManager.h"
#include "CSlrString.h"

CDebugInterfaceNesTaskJoystickEvent::CDebugInterfaceNesTaskJoystickEvent(CDebugInterfaceNes *debugInterface, u8 buttonState, int port, u32 axis)
{
	this->debugInterface = debugInterface;
	this->port = port;
	this->buttonState = buttonState;
	this->axis = axis;
}

void CDebugInterfaceNesTaskJoystickEvent::ExecuteTask()
{
	if (debugInterface->snapshotsManager->isStoreInputEventsEnabled)
	{
		CByteBuffer *inputEventsBuffer = debugInterface->GetInputEventsBufferForCurrentCycle();
		inputEventsBuffer->Rewind();

		bool found = false;
		while (inputEventsBuffer->IsEof() == false)
		{
			u8 eventType = inputEventsBuffer->GetU8();
			if (eventType == DEBUGGER_EVENT_TYPE_JOYSTICK)
			{
				int bufferEventPort = inputEventsBuffer->GetI32();
				if (bufferEventPort == port)
				{
					// replace existing event for this port
					int len = inputEventsBuffer->length;
					inputEventsBuffer->PutU32(axis);
					inputEventsBuffer->PutU8(buttonState);
					inputEventsBuffer->length = len;
					found = true;
				}
				else
				{
					inputEventsBuffer->GetU32(); // skip axis
					inputEventsBuffer->GetU8();  // skip buttonState
				}
			}
			else if (eventType == DEBUGGER_EVENT_TYPE_KEYBOARD)
			{
				inputEventsBuffer->GetU32(); // skip keyCode
				inputEventsBuffer->GetU8();  // skip buttonState
			}
			else
			{
				LOGError("Unknown event type: %d", eventType);
				break;
			}
		}

		if (found == false)
		{
			inputEventsBuffer->PutU8(DEBUGGER_EVENT_TYPE_JOYSTICK);
			inputEventsBuffer->PutI32(port);
			inputEventsBuffer->PutU32(axis);
			inputEventsBuffer->PutU8(buttonState);
		}
	}
	
	debugInterface->ProcessJoystickEventSynced(port, axis, buttonState);
}

CDebugInterfaceNesTaskInsertCartridge::CDebugInterfaceNesTaskInsertCartridge(CDebugInterfaceNes *debugInterface, CSlrString *pathToCart)
{
	this->debugInterface = debugInterface;
	this->pathToCart = new CSlrString(pathToCart);
}
CDebugInterfaceNesTaskInsertCartridge::~CDebugInterfaceNesTaskInsertCartridge()
{
	delete this->pathToCart;
}

void CDebugInterfaceNesTaskInsertCartridge::ExecuteTask()
{
	char *cPathToCart = this->pathToCart->GetStdASCII();
	bool ret = nesd_insert_cartridge(cPathToCart);

	delete cPathToCart;
	
	debugInterface->ResetEmulationFrameCounter();
	debugInterface->ResetClockCounters();
	debugInterface->ClearHistory();
}

//
CDebugInterfaceNesTaskReset::CDebugInterfaceNesTaskReset(CDebugInterfaceNes *debugInterface)
{
	this->debugInterface = debugInterface;
}

void CDebugInterfaceNesTaskReset::ExecuteTask()
{
	nesd_reset();
}

//
CDebugInterfaceNesTaskResetHard::CDebugInterfaceNesTaskResetHard(CDebugInterfaceNes *debugInterface)
{
	this->debugInterface = debugInterface;
}

void CDebugInterfaceNesTaskResetHard::ExecuteTask()
{
	nesd_reset();
	
	debugInterface->ResetEmulationFrameCounter();
	debugInterface->ResetClockCounters();
	debugInterface->ClearHistory();
}

