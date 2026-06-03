#include "CTestViceInstructionStepping.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "CDebugInterfaceVice.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include "DebuggerDefs.h"
#include "C64SettingsStorage.h"
#include <cstdio>
#include <cstring>

static char failureMsg[1024];

void CTestViceInstructionStepping::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;
	failureMsg[0] = '\0';

#ifndef RUN_COMMODORE64
	TestCompleted(true, "Skipped (C64 not enabled)");
	return;
#else
	CDebugInterfaceVice *di = (CDebugInterfaceVice *)viewC64->debugInterfaceC64;
	if (!di)
	{
		TestCompleted(false, "C64 debug interface is NULL");
		return;
	}

	bool wasRunning = di->isRunning;
	if (!wasRunning)
	{
		viewC64->StartEmulationThread(di);
		SYS_Sleep(2000);
	}

	if (!di->isRunning)
	{
		TestCompleted(false, "C64 emulator failed to start");
		return;
	}

	// Inject test code at $1000:
	// $1000: SEI          (78)
	// $1001: INC $D020    (EE 20 D0)
	// $1004: INC $D021    (EE 21 D0)
	// $1007: JMP $1001    (4C 01 10)
	di->PauseEmulationBlockedWait();

	u8 code[] = { 0x78, 0xEE, 0x20, 0xD0, 0xEE, 0x21, 0xD0, 0x4C, 0x01, 0x10 };
	for (int i = 0; i < (int)sizeof(code); i++)
	{
		di->SetByteToRamC64(0x1000 + i, code[i]);
	}

	// Use MakeJmpC64 (CPU trap) to redirect PC to $1000, then run briefly
	di->MakeJmpC64(0x1000);
	di->SetDebugMode(DEBUGGER_MODE_RUNNING);

	// Poll until PC reaches our code
	C64StateCPU cpuState;
	int tries = 0;
	while (tries < 200)
	{
		SYS_Sleep(10);
		di->GetC64CpuState(&cpuState);
		if (cpuState.pc >= 0x1000 && cpuState.pc <= 0x1007)
			break;
		tries++;
	}

	// Pause and verify we're in our loop
	di->PauseEmulationBlockedWait();
	di->GetC64CpuState(&cpuState);

	if (cpuState.pc < 0x1000 || cpuState.pc > 0x1007)
	{
		snprintf(failureMsg, sizeof(failureMsg), "PC=$%04X, expected $1000-$1007", cpuState.pc);
		TestCompleted(false, failureMsg);
		return;
	}

	// Step through 20 instructions (INC $D020, INC $D021, JMP, loop)
	// verifying stepping works without crashes or hangs
	for (int step = 0; step < 20; step++)
	{
		di->SetDebugMode(DEBUGGER_MODE_RUN_ONE_INSTRUCTION);
		SYS_Sleep(50);
		di->PauseEmulationBlockedWait();
	}

	// Verify PC is still in the loop
	di->GetC64CpuState(&cpuState);
	if (cpuState.pc != 0x1001 && cpuState.pc != 0x1004 && cpuState.pc != 0x1007)
	{
		snprintf(failureMsg, sizeof(failureMsg), "PC=$%04X after stepping, expected $1001/$1004/$1007", cpuState.pc);
		TestCompleted(false, failureMsg);
		return;
	}

	// Resume normal execution
	di->SetDebugMode(DEBUGGER_MODE_RUNNING);
	SYS_Sleep(200);

	if (!wasRunning)
	{
		di->PauseEmulationBlockedWait();
		viewC64->StopEmulationThread(di);
	}

	TestCompleted(true, "Instruction stepping verified: code injection, PC control, 20 steps");
#endif
}

void CTestViceInstructionStepping::Cancel()
{
	isRunning = false;
}
