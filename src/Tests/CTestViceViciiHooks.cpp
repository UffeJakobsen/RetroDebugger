#include "CTestViceViciiHooks.h"
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

extern "C"
{
#include "ViceWrapper.h"
};

static char failureMsg[512];

void CTestViceViciiHooks::Run(ITestCallback *cb)
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

	bool allPassed = true;

	// --- Step 1: Frame counter increments ---
	{
		di->PauseEmulationBlockedWait();
		unsigned int frameBefore = c64d_get_frame_num();

		di->SetDebugMode(DEBUGGER_MODE_RUNNING);
		SYS_Sleep(200);  // ~10 frames at 50Hz
		di->PauseEmulationBlockedWait();

		unsigned int frameAfter = c64d_get_frame_num();

		if (frameAfter <= frameBefore)
		{
			sprintf(failureMsg, "Frame counter did not advance: before=%u, after=%u", frameBefore, frameAfter);
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "Frame counter advanced by %u frames", frameAfter - frameBefore);
			StepCompleted(1, true, msg);
		}
		else
		{
			StepCompleted(1, false, failureMsg);
		}
	}

	// --- Step 2: VIC-II register read/write ---
	if (allPassed)
	{
		di->PauseEmulationBlockedWait();

		// Read $D011 (control register 1) - should have valid value
		u8 d011 = di->GetVicRegister(0x11);

		// Write/read $D020 (border color)
		u8 origBorder = di->GetVicRegister(0x20);
		di->SetVicRegister(0x20, 0x02);
		u8 newBorder = di->GetVicRegister(0x20);

		// Border color is only 4 bits
		if ((newBorder & 0x0F) != 0x02)
		{
			sprintf(failureMsg, "VIC register: wrote $02 to $D020, read $%02X (low nibble $%X)", newBorder, newBorder & 0x0F);
			allPassed = false;
		}

		// Restore
		di->SetVicRegister(0x20, origBorder);

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "VIC register R/W verified ($D011=$%02X, $D020 write OK)", d011);
			StepCompleted(2, true, msg);
		}
		else
		{
			StepCompleted(2, false, failureMsg);
		}
	}

	// --- Step 3: Per-cycle state recording ---
	if (allPassed)
	{
		// Ensure every-cycle recording mode
		u8 prevMode = c64SettingsVicStateRecordingMode;
		di->SetVicRecordStateMode(C64D_VICII_RECORD_MODE_EVERY_CYCLE);

		di->SetDebugMode(DEBUGGER_MODE_RUNNING);
		SYS_Sleep(100);
		di->PauseEmulationBlockedWait();

		// Scan state array for valid raster lines
		int validStates = 0;
		int maxRasterLine = 0;

		for (int rasterLine = 0; rasterLine < 312; rasterLine++)
		{
			for (int rasterCycle = 0; rasterCycle < 63; rasterCycle++)
			{
				vicii_cycle_state_t *state = c64d_get_vicii_state_for_raster_cycle(rasterLine, rasterCycle);
				if (state && state->raster_line == (unsigned int)rasterLine && state->raster_cycle == (unsigned int)rasterCycle)
				{
					validStates++;
					if ((int)state->raster_line > maxRasterLine)
						maxRasterLine = (int)state->raster_line;
				}
			}
		}

		// Restore recording mode
		di->SetVicRecordStateMode(prevMode);

		// PAL has 312 lines x 63 cycles = 19656 total
		if (validStates < 1000)
		{
			sprintf(failureMsg, "Per-cycle recording: only %d valid states (expected >1000)", validStates);
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "Per-cycle state recording: %d valid states, max raster=%d", validStates, maxRasterLine);
			StepCompleted(3, true, msg);
		}
		else
		{
			StepCompleted(3, false, failureMsg);
		}
	}

	// --- Step 4: Border mode get/set ---
	if (allPassed)
	{
		di->PauseEmulationBlockedWait();

		int origBorderMode = di->GetViciiBorderMode();

		// Set to no-borders mode
		di->SetViciiBorderMode(3);  // NO_BORDERS
		int newMode = di->GetViciiBorderMode();

		if (newMode != 3)
		{
			sprintf(failureMsg, "Border mode: set to 3, got %d", newMode);
			allPassed = false;
		}

		// Restore
		di->SetViciiBorderMode(origBorderMode);

		if (allPassed)
		{
			StepCompleted(4, true, "Border mode get/set verified");
		}
		else
		{
			StepCompleted(4, false, failureMsg);
		}
	}

	// --- Step 5: VIC state struct ---
	if (allPassed)
	{
		C64StateVIC vicState;
		di->GetVICState(&vicState);

		// Raster line should be 0-311 for PAL, 0-262 for NTSC
		if (vicState.raster_line < 0 || vicState.raster_line > 320)
		{
			sprintf(failureMsg, "VIC state: raster_line=%d out of range", vicState.raster_line);
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "VIC state: raster_line=%d, raster_cycle=%d", vicState.raster_line, vicState.raster_cycle);
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
		TestCompleted(true, "VIC-II hooks verified: frames, registers, cycle states, border, VIC state");
	else
		TestCompleted(false, failureMsg);
#endif
}

void CTestViceViciiHooks::Cancel()
{
	isRunning = false;
}
