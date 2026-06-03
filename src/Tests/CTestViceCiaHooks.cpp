#include "CTestViceCiaHooks.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include "DebuggerDefs.h"
#include "C64SettingsStorage.h"
#include <cstdio>
#include <cstring>

extern "C"
{
#include "ViceWrapper.h"
};

// Test program that continuously writes to CIA1 ($DC00) and CIA2 ($DD00):
//   $1000: SEI
//   $1001: LDA #$FF
//   $1003: STA $DC02       ; CIA1 DDRA (port A all output)
//   $1006: LDA #$7F
//   $1008: STA $DC00       ; CIA1 Port A
//   $100B: LDA #$03
//   $100D: STA $DD00       ; CIA2 Port A
//   $1010: JMP $1006       ; loop: continuously write to both CIAs

static const u8 testCode[] = {
	0x78,                   // $1000: SEI
	0xA9, 0xFF,             // $1001: LDA #$FF
	0x8D, 0x02, 0xDC,       // $1003: STA $DC02
	0xA9, 0x7F,             // $1006: LDA #$7F
	0x8D, 0x00, 0xDC,       // $1008: STA $DC00
	0xA9, 0x03,             // $100B: LDA #$03
	0x8D, 0x00, 0xDD,       // $100D: STA $DD00
	0x4C, 0x06, 0x10,       // $1010: JMP $1006 (loop writes)
};

static char failureMsg[512];

void CTestViceCiaHooks::Run(ITestCallback *cb)
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

	// --- Step 1: CIA register read/write via API ---
	{
		di->PauseEmulationBlockedWait();

		// CIA1 timer A low byte (register $04)
		u8 origVal = di->GetCiaRegister(0, 0x04);

		// Read CIA2 port A (register $00)
		u8 cia2Val = di->GetCiaRegister(1, 0x00);

		// Both reads should succeed without crash — values depend on emulation state
		StepCompleted(1, true, "CIA register read API works (CIA1/CIA2)");
	}

	// --- Step 2: CIA write tracking in per-cycle state ---
	if (allPassed)
	{
		// Ensure every-cycle recording
		u8 prevMode = c64SettingsVicStateRecordingMode;
		di->SetVicRecordStateMode(C64D_VICII_RECORD_MODE_EVERY_CYCLE);

		di->PauseEmulationBlockedWait();

		// Inject test code
		for (int i = 0; i < (int)sizeof(testCode); i++)
		{
			di->SetByteToRamC64(0x1000 + i, testCode[i]);
		}

		di->MakeJmpC64(0x1000);

		// Run for several frames
		di->SetDebugMode(DEBUGGER_MODE_RUNNING);
		SYS_Sleep(300);
		di->PauseEmulationBlockedWait();

		// Scan per-cycle state for CIA1 and CIA2 write markers (retry once if not found)
		bool foundCia1Write = false;
		bool foundCia2Write = false;

		for (int attempt = 0; attempt < 2 && !(foundCia1Write && foundCia2Write); attempt++)
		{
			if (attempt > 0)
			{
				di->SetDebugMode(DEBUGGER_MODE_RUNNING);
				SYS_Sleep(300);
				di->PauseEmulationBlockedWait();
			}

			for (int rasterLine = 0; rasterLine < 312; rasterLine++)
			{
				for (int rasterCycle = 0; rasterCycle < 63; rasterCycle++)
				{
					vicii_cycle_state_t *state = c64d_get_vicii_state_for_raster_cycle(rasterLine, rasterCycle);
					if (state->cia1RegisterWritten >= 0)
						foundCia1Write = true;
					if (state->cia2RegisterWritten >= 0)
						foundCia2Write = true;

					if (foundCia1Write && foundCia2Write)
						break;
				}
				if (foundCia1Write && foundCia2Write)
					break;
			}
		}

		// Restore recording mode
		di->SetVicRecordStateMode(prevMode);

		if (!foundCia1Write)
		{
			sprintf(failureMsg, "CIA1 write not tracked in per-cycle state (STA $DC00/$DC02)");
			allPassed = false;
		}
		else if (!foundCia2Write)
		{
			sprintf(failureMsg, "CIA2 write not tracked in per-cycle state (STA $DD00)");
			allPassed = false;
		}

		if (allPassed)
			StepCompleted(2, true, "CIA1/CIA2 write tracking in per-cycle state verified");
		else
			StepCompleted(2, false, failureMsg);
	}

	// --- Step 3: CIA timer registers readable ---
	if (allPassed)
	{
		di->PauseEmulationBlockedWait();

		// Read CIA1 Timer A ($DC04/$DC05) and Timer B ($DC06/$DC07)
		u8 timerALo = di->GetCiaRegister(0, 0x04);
		u8 timerAHi = di->GetCiaRegister(0, 0x05);
		u8 timerBLo = di->GetCiaRegister(0, 0x06);
		u8 timerBHi = di->GetCiaRegister(0, 0x07);

		// Timers should be readable (any value is valid)
		char msg[128];
		sprintf(msg, "CIA1 timers: A=$%02X%02X, B=$%02X%02X", timerAHi, timerALo, timerBHi, timerBLo);
		StepCompleted(3, true, msg);
	}

	// Restore emulator state
	if (!wasRunning)
		viewC64->StopEmulationThread(di);

	if (allPassed)
		TestCompleted(true, "CIA hooks verified: register API, cycle-state tracking, timers");
	else
		TestCompleted(false, failureMsg);
#endif
}

void CTestViceCiaHooks::Cancel()
{
	isRunning = false;
}
