#include "CTestAtariInputReplay.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceAtari.h"
#include "CSnapshotsManager.h"
#include "DebuggerDefs.h"
#include "SYS_Main.h"
#include "CByteBuffer.h"
#include <cstring>
#include <cstdio>

void CTestAtariInputReplay::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;
	char msg[512];

#ifndef RUN_ATARI
	TestCompleted(true, "Skipped (Atari not enabled)");
	return;
#else
	CDebugInterfaceAtari *di = (CDebugInterfaceAtari *)viewC64->debugInterfaceAtari;
	if (!di)
	{
		TestCompleted(false, "Atari debug interface is NULL");
		return;
	}

	bool wasRunning = di->isRunning;
	if (!wasRunning)
	{
		viewC64->StartEmulationThread(di);
		SYS_Sleep(1000);
	}

	if (!di->isRunning)
	{
		TestCompleted(false, "Atari emulator failed to start");
		return;
	}

	if (di->GetDebugMode() != DEBUGGER_MODE_RUNNING)
	{
		di->SetDebugMode(DEBUGGER_MODE_RUNNING);
		SYS_Sleep(100);
	}

	CSnapshotsManager *sm = di->snapshotsManager;

	// --- Step 1: Verify GetJoystickState baseline ---
	SYS_Sleep(200);
	uint32 stateBeforePort0 = di->GetJoystickState(0);

	snprintf(msg, sizeof(msg), "Baseline joystick state: port0=$%02X", stateBeforePort0);
	StepCompleted(1, true, msg);

	// --- Step 2: Test task-based joystick injection ---
	// Press UP via task queue, verify GetJoystickState reflects it
	di->JoystickDown(0, JOYPAD_N);
	SYS_Sleep(200);

	uint32 stateAfterTask = di->GetJoystickState(0);
	di->JoystickUp(0, JOYPAD_N);
	SYS_Sleep(50);

	bool taskWorked = (stateAfterTask & JOYPAD_N) != 0;
	snprintf(msg, sizeof(msg), "Task injection: GetJoystickState(0)=$%02X %s",
			 stateAfterTask, taskWorked ? "(N set)" : "(N NOT set)");
	StepCompleted(2, taskWorked, msg);

	if (!taskWorked)
	{
		if (!wasRunning) viewC64->StopEmulationThread(di);
		TestCompleted(false, "Atari task injection failed");
		return;
	}

	// --- Step 3: Test map replay path ---
	u64 baseCycle = di->GetMainCpuCycleCounter();
	u64 eventCycle = baseCycle + 500;  // small offset — will be hit within ms

	CStoredInputEvent *ev = sm->GetNewInputEventSnapshot(0, eventCycle);
	sm->inputEventsByCycle[eventCycle] = ev;
	ev->byteBuffer->PutU8(DEBUGGER_EVENT_TYPE_JOYSTICK);
	ev->byteBuffer->PutI32(0);
	ev->byteBuffer->PutU32(JOYPAD_S);  // DOWN (different from Step 2's UP)
	ev->byteBuffer->PutU8(DEBUGGER_EVENT_BUTTON_DOWN);

	sm->isReplayInputEventsEnabled = true;
	SYS_Sleep(200);

	uint32 stateAfterReplay = di->GetJoystickState(0);

	sm->isReplayInputEventsEnabled = false;
	for (auto &pair : sm->inputEventsByCycle)
		sm->inputEventsToReuse.push_back(pair.second);
	sm->inputEventsByCycle.clear();

	di->JoystickUp(0, JOYPAD_S);
	SYS_Sleep(50);

	bool replayWorked = (stateAfterReplay & JOYPAD_S) != 0;
	snprintf(msg, sizeof(msg), "Map replay: GetJoystickState(0)=$%02X %s",
			 stateAfterReplay, replayWorked ? "(S set)" : "(S NOT set)");
	StepCompleted(3, replayWorked, msg);

	if (!wasRunning) viewC64->StopEmulationThread(di);

	if (taskWorked && replayWorked)
		TestCompleted(true, "Atari input replay: task injection + map replay both verified");
	else if (taskWorked)
		TestCompleted(false, "Atari input replay: task works but map replay failed");
	else
		TestCompleted(false, "Atari input replay: task injection failed");
#endif
}
