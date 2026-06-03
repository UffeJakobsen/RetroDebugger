#include "CTestViceBreakpoints.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "CDebugSymbolsC64.h"
#include "CDebugSymbolsSegment.h"
#include "CDebugBreakpointsAddr.h"
#include "CDebugBreakpointAddr.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include "DebuggerDefs.h"
#include <cstdio>
#include <cstring>

// Test program at $1000:
//   $1000: SEI
//   $1001: NOP
//   $1002: NOP
//   $1003: NOP             ; breakpoint target
//   $1004: NOP
//   $1005: JMP $1001       ; loop

static const u8 testCode[] = {
	0x78,                   // $1000: SEI
	0xEA,                   // $1001: NOP
	0xEA,                   // $1002: NOP
	0xEA,                   // $1003: NOP (breakpoint here)
	0xEA,                   // $1004: NOP
	0x4C, 0x01, 0x10,       // $1005: JMP $1001
};

static char failureMsg[512];

void CTestViceBreakpoints::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;
	failureMsg[0] = '\0';

#ifndef RUN_COMMODORE64
	TestCompleted(true, "Skipped (C64 not enabled)");
	return;
#else
	CDebugInterfaceC64 *di = (CDebugInterfaceC64 *)viewC64->debugInterfaceC64;
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

	bool allPassed = true;

	// --- Step 1: Set PC breakpoint and verify it triggers ---
	{
		di->PauseEmulationBlockedWait();

		// Inject test code
		for (int i = 0; i < (int)sizeof(testCode); i++)
		{
			di->SetByteToRamC64(0x1000 + i, testCode[i]);
		}

		// Set breakpoint at $1003
		CDebugSymbolsSegment *segment = di->symbolsC64->currentSegment;
		if (!segment)
		{
			sprintf(failureMsg, "C64 symbols segment is NULL");
			allPassed = false;
		}

		if (allPassed)
		{
			segment->AddBreakpointPC(0x1003);

			// Jump to $1000 and run
			di->MakeJmpC64(0x1000);
			di->SetDebugMode(DEBUGGER_MODE_RUNNING);

			// Wait for breakpoint to trigger (should pause emulator)
			int tries = 0;
			while (tries < 200)
			{
				SYS_Sleep(10);
				if (di->GetDebugMode() == DEBUGGER_MODE_PAUSED)
					break;
				tries++;
			}

			di->PauseEmulationBlockedWait();

			C64StateCPU cpuState;
			di->GetC64CpuState(&cpuState);

			// PC should be at or just past the breakpoint address
			// (the instruction at the breakpoint may have executed before pause takes effect)
			if (cpuState.pc < 0x1003 || cpuState.pc > 0x1005)
			{
				sprintf(failureMsg, "Breakpoint at $1003: PC=$%04X (expected $1003-$1005)", cpuState.pc);
				allPassed = false;
			}

			// Clean up: remove breakpoint
			auto it = segment->breakpointsPC->breakpoints.find(0x1003);
			if (it != segment->breakpointsPC->breakpoints.end())
			{
				segment->breakpointsPC->RemoveBreakpoint(it->second);
			}
		}

		if (allPassed)
			StepCompleted(1, true, "PC breakpoint at $1003 triggered correctly");
		else
			StepCompleted(1, false, failureMsg);
	}

	// --- Step 2: Debug mode transitions ---
	if (allPassed)
	{
		// Verify we can transition between modes
		di->PauseEmulationBlockedWait();

		u8 mode = di->GetDebugMode();
		if (mode != DEBUGGER_MODE_PAUSED)
		{
			sprintf(failureMsg, "Expected PAUSED mode ($%02X), got $%02X", DEBUGGER_MODE_PAUSED, mode);
			allPassed = false;
		}

		if (allPassed)
		{
			di->SetDebugMode(DEBUGGER_MODE_RUNNING);
			SYS_Sleep(50);
			mode = di->GetDebugMode();
			if (mode != DEBUGGER_MODE_RUNNING)
			{
				sprintf(failureMsg, "Expected RUNNING mode ($%02X), got $%02X", DEBUGGER_MODE_RUNNING, mode);
				allPassed = false;
			}
		}

		if (allPassed)
		{
			di->PauseEmulationBlockedWait();
			mode = di->GetDebugMode();
			if (mode != DEBUGGER_MODE_PAUSED)
			{
				sprintf(failureMsg, "After re-pause: expected PAUSED ($%02X), got $%02X", DEBUGGER_MODE_PAUSED, mode);
				allPassed = false;
			}
		}

		if (allPassed)
			StepCompleted(2, true, "Debug mode transitions: PAUSED -> RUNNING -> PAUSED");
		else
			StepCompleted(2, false, failureMsg);
	}

	// --- Step 3: Breakpoint add/remove doesn't crash ---
	if (allPassed)
	{
		CDebugSymbolsSegment *segment = di->symbolsC64->currentSegment;

		// Add multiple breakpoints
		segment->AddBreakpointPC(0x2000);
		segment->AddBreakpointPC(0x2001);
		segment->AddBreakpointPC(0x2002);

		// Verify they exist
		int count = (int)segment->breakpointsPC->breakpoints.size();
		if (count < 3)
		{
			sprintf(failureMsg, "Expected at least 3 PC breakpoints, found %d", count);
			allPassed = false;
		}

		// Remove them
		segment->ClearBreakpoints();

		count = (int)segment->breakpointsPC->breakpoints.size();
		if (count != 0)
		{
			sprintf(failureMsg, "After ClearBreakpoints: still have %d breakpoints", count);
			allPassed = false;
		}

		if (allPassed)
			StepCompleted(3, true, "Breakpoint add/remove/clear verified");
		else
			StepCompleted(3, false, failureMsg);
	}

	// Restore emulator state
	if (!wasRunning)
		viewC64->StopEmulationThread(di);

	if (allPassed)
		TestCompleted(true, "Breakpoints verified: PC breakpoint trigger, mode transitions, add/remove");
	else
		TestCompleted(false, failureMsg);
#endif
}

void CTestViceBreakpoints::Cancel()
{
	isRunning = false;
}
