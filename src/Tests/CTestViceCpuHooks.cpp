#include "CTestViceCpuHooks.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include "DebuggerDefs.h"
#include <cstdio>
#include <cstring>

// Test program at $1000:
//   $1000: SEI
//   $1001: LDA #$42       ; 2 cycles
//   $1003: LDX #$07       ; 2 cycles
//   $1005: STA $0400       ; 4 cycles
//   $1008: INX             ; 2 cycles
//   $1009: JMP $1009       ; loop forever

static const u8 testCode[] = {
	0x78,                   // $1000: SEI
	0xA9, 0x42,             // $1001: LDA #$42
	0xA2, 0x07,             // $1003: LDX #$07
	0x8D, 0x00, 0x04,       // $1005: STA $0400
	0xE8,                   // $1008: INX
	0x4C, 0x09, 0x10,       // $1009: JMP $1009
};

static char failureMsg[512];

void CTestViceCpuHooks::Run(ITestCallback *cb)
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
	C64StateCPU cpuState;

	// --- Step 1: Inject code, run to JMP loop, verify execution ---
	di->PauseEmulationBlockedWait();

	for (int i = 0; i < (int)sizeof(testCode); i++)
	{
		di->SetByteToRamC64(0x1000 + i, testCode[i]);
	}

	di->MakeJmpC64(0x1000);
	di->SetDebugMode(DEBUGGER_MODE_RUNNING);

	// Poll until PC reaches $1009 (JMP loop)
	int tries = 0;
	while (tries < 200)
	{
		SYS_Sleep(10);
		di->GetC64CpuState(&cpuState);
		if (cpuState.pc == 0x1009)
			break;
		tries++;
	}

	di->PauseEmulationBlockedWait();
	di->GetC64CpuState(&cpuState);

	if (cpuState.pc != 0x1009)
	{
		sprintf(failureMsg, "Execution: expected PC=$1009, got $%04X", cpuState.pc);
		allPassed = false;
	}

	// Verify STA $0400 wrote $42
	if (allPassed)
	{
		u8 val = di->GetByteFromRamC64(0x0400);
		if (val != 0x42)
		{
			sprintf(failureMsg, "Execution: $0400=$%02X (expected $42)", val);
			allPassed = false;
		}
	}

	// Verify A=$42 and X=$08 (LDX #$07 then INX)
	if (allPassed)
	{
		if (cpuState.a != 0x42)
		{
			sprintf(failureMsg, "After execution: A=$%02X (expected $42)", cpuState.a);
			allPassed = false;
		}
		else if (cpuState.x != 0x08)
		{
			sprintf(failureMsg, "After execution: X=$%02X (expected $08 from LDX #$07 + INX)", cpuState.x);
			allPassed = false;
		}
	}

	if (allPassed)
		StepCompleted(1, true, "Code execution and PC tracking verified");
	else
		StepCompleted(1, false, failureMsg);

	// --- Step 2: Single-instruction stepping from a known paused state ---
	if (allPassed)
	{
		// We're paused at $1009 (JMP $1009). Step should execute JMP and land back at $1009.
		u16 pcBefore = cpuState.pc;  // $1009

		di->SetDebugMode(DEBUGGER_MODE_RUN_ONE_INSTRUCTION);
		SYS_Sleep(50);
		di->PauseEmulationBlockedWait();
		di->GetC64CpuState(&cpuState);

		// After stepping JMP $1009, PC should still be $1009
		if (cpuState.pc != 0x1009)
		{
			sprintf(failureMsg, "Single step JMP: expected PC=$1009, got $%04X", cpuState.pc);
			allPassed = false;
		}

		// Do multiple steps — PC should stay at $1009 since it's an infinite JMP loop
		if (allPassed)
		{
			for (int i = 0; i < 3; i++)
			{
				di->SetDebugMode(DEBUGGER_MODE_RUN_ONE_INSTRUCTION);
				SYS_Sleep(50);
				di->PauseEmulationBlockedWait();
			}
			di->GetC64CpuState(&cpuState);

			if (cpuState.pc != 0x1009)
			{
				sprintf(failureMsg, "After 3 steps in JMP loop: PC=$%04X (expected $1009)", cpuState.pc);
				allPassed = false;
			}
		}

		if (allPassed)
			StepCompleted(2, true, "Single-instruction stepping verified (JMP loop stable)");
		else
			StepCompleted(2, false, failureMsg);
	}

	// --- Step 3: Register read from known execution state ---
	if (allPassed)
	{
		di->PauseEmulationBlockedWait();
		di->GetC64CpuState(&cpuState);

		// Verify all register values from execution in step 1
		// A=$42 from LDA #$42, X=$08 from LDX #$07 + INX
		if (cpuState.a != 0x42)
		{
			sprintf(failureMsg, "Register A: $%02X (expected $42)", cpuState.a);
			allPassed = false;
		}
		else if (cpuState.x != 0x08)
		{
			sprintf(failureMsg, "Register X: $%02X (expected $08)", cpuState.x);
			allPassed = false;
		}

		// Verify processor flags: I flag should be set (SEI at $1000)
		if (allPassed)
		{
			if (!(cpuState.processorFlags & 0x04))
			{
				sprintf(failureMsg, "Processor flags: $%02X (expected I flag set after SEI)", cpuState.processorFlags);
				allPassed = false;
			}
		}

		// Verify instruction cycle and lastValidPC fields are populated
		if (allPassed)
		{
			// lastValidPC should be in our code range ($1000-$1009)
			if (cpuState.lastValidPC < 0x1000 || cpuState.lastValidPC > 0x100C)
			{
				sprintf(failureMsg, "lastValidPC=$%04X (expected $1000-$100C)", cpuState.lastValidPC);
				allPassed = false;
			}
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "Register read: A=$%02X X=$%02X flags=$%02X SP=$%02X lastPC=$%04X",
					cpuState.a, cpuState.x, cpuState.processorFlags, cpuState.sp, cpuState.lastValidPC);
			StepCompleted(3, true, msg);
		}
		else
		{
			StepCompleted(3, false, failureMsg);
		}
	}

	// --- Step 4: Cycle counter advancement ---
	if (allPassed)
	{
		di->MakeJmpC64(0x1000);
		di->ResetMainCpuDebugCycleCounter();

		u64 counterBefore = di->GetMainCpuDebugCycleCounter();

		di->SetDebugMode(DEBUGGER_MODE_RUNNING);
		SYS_Sleep(100);
		di->PauseEmulationBlockedWait();

		u64 counterAfter = di->GetMainCpuDebugCycleCounter();

		if (counterAfter <= counterBefore)
		{
			sprintf(failureMsg, "Cycle counter did not advance: before=%llu, after=%llu", counterBefore, counterAfter);
			allPassed = false;
		}

		u64 mainCounter = di->GetMainCpuCycleCounter();
		if (mainCounter == 0)
		{
			sprintf(failureMsg, "Main CPU cycle counter is zero");
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "Cycle counters advancing (debug: %llu, main: %llu)", counterAfter - counterBefore, mainCounter);
			StepCompleted(4, true, msg);
		}
		else
		{
			StepCompleted(4, false, failureMsg);
		}
	}

	// --- Step 5: GetCpuPC matches state struct ---
	if (allPassed)
	{
		di->PauseEmulationBlockedWait();
		di->GetC64CpuState(&cpuState);
		int pc = di->GetCpuPC();

		if ((u16)pc != cpuState.pc)
		{
			sprintf(failureMsg, "GetCpuPC()=$%04X but state.pc=$%04X", pc, cpuState.pc);
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "GetCpuPC()=$%04X matches state struct", pc);
			StepCompleted(5, true, msg);
		}
		else
		{
			StepCompleted(5, false, failureMsg);
		}
	}

	// Restore emulator state
	if (!wasRunning)
		viewC64->StopEmulationThread(di);

	if (allPassed)
		TestCompleted(true, "CPU hooks verified: execution, stepping, registers, cycles, PC");
	else
		TestCompleted(false, failureMsg);
#endif
}

void CTestViceCpuHooks::Cancel()
{
	isRunning = false;
}
