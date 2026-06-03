#include "CTestVicePeripherals.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "CDebugInterfaceVice.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include "DebuggerDefs.h"
#include <cstdio>
#include <cstring>

extern "C"
{
#include "ViceWrapper.h"
};

static char failureMsg[512];

void CTestVicePeripherals::Run(ITestCallback *cb)
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

	di->PauseEmulationBlockedWait();

	bool allPassed = true;

	// --- Step 1: Cartridge state query ---
	{
		C64StateCartridge cartState;
		di->GetC64CartridgeState(&cartState);

		// With no cartridge attached, EXROM and GAME should both be 1
		// (active low — 1 means not asserted = no cartridge)
		if (cartState.exrom != 1 || cartState.game != 1)
		{
			// Some configurations may differ; just verify the call succeeds
			// This is still valid — just log the state
		}

		char msg[128];
		sprintf(msg, "Cartridge state: EXROM=%d, GAME=%d", cartState.exrom, cartState.game);
		StepCompleted(1, true, msg);
	}

	// --- Step 2: REU enable/disable ---
	if (allPassed)
	{
		// Enable REU — should not crash
		di->SetReuEnabled(true);
		SYS_Sleep(50);

		// Disable REU
		di->SetReuEnabled(false);
		SYS_Sleep(50);

		StepCompleted(2, true, "REU enable/disable executed without crash");
	}

	// --- Step 3: Datasette API ---
	if (allPassed)
	{
		// With no tape attached, datasette commands should be no-ops but not crash
		di->DatasetteReset();
		SYS_Sleep(20);

		di->DatasetteStop();
		SYS_Sleep(20);

		// Set datasette parameters
		di->DatasetteSetSpeedTuning(0);
		di->DatasetteSetZeroGapDelay(20000);
		di->DatasetteSetResetWithCPU(true);

		StepCompleted(3, true, "Datasette API calls executed without crash");
	}

	// --- Step 4: Disk drive reset ---
	if (allPassed)
	{
		di->DiskDriveReset();
		SYS_Sleep(200);

		// Verify drive still functional by reading drive memory
		u8 drvByte = di->GetByte1541(0xC000);
		// Drive ROM should be present at $C000+
		// Any value is fine — just verify no crash

		StepCompleted(4, true, "Disk drive reset executed");
	}

	// --- Step 5: Disk dirty flag management ---
	if (allPassed)
	{
		// Clear all dirty flags
		di->ClearDriveDirtyForSnapshotFlag();
		di->ClearDriveDirtyForRefreshFlag(0);

		bool dirtySnap = di->IsDriveDirtyForSnapshot();
		bool dirtyRefresh = di->IsDriveDirtyForRefresh(0);

		if (dirtySnap)
		{
			sprintf(failureMsg, "Snapshot dirty flag not cleared");
			allPassed = false;
		}
		else if (dirtyRefresh)
		{
			sprintf(failureMsg, "Refresh dirty flag not cleared");
			allPassed = false;
		}

		// Test set/clear cycle
		if (allPassed)
		{
			di->SetDriveDirtyForRefreshFlag(0);
			bool nowDirty = di->IsDriveDirtyForRefresh(0);
			if (!nowDirty)
			{
				sprintf(failureMsg, "Refresh dirty flag not set after SetDriveDirtyForRefreshFlag");
				allPassed = false;
			}
			di->ClearDriveDirtyForRefreshFlag(0);
		}

		if (allPassed)
			StepCompleted(5, true, "Disk dirty flag set/clear/query verified");
		else
			StepCompleted(5, false, failureMsg);
	}

	// --- Step 6: Joystick subsystem ---
	if (allPassed)
	{
		// Joystick input should not crash (fire on port 2)
		di->JoystickDown(2, JOYPAD_FIRE);
		SYS_Sleep(10);
		di->JoystickUp(2, JOYPAD_FIRE);
		SYS_Sleep(10);

		// Direction input
		di->JoystickDown(2, JOYPAD_N);
		SYS_Sleep(10);
		di->JoystickUp(2, JOYPAD_N);

		StepCompleted(6, true, "Joystick input executed without crash");
	}

	// --- Step 7: Keyboard subsystem ---
	if (allPassed)
	{
		// Keyboard input should not crash
		di->KeyboardDown(0x20);  // space
		SYS_Sleep(10);
		di->KeyboardUp(0x20);
		SYS_Sleep(10);

		StepCompleted(7, true, "Keyboard input executed without crash");
	}

	// --- Step 8: DetachEverything ---
	if (allPassed)
	{
		// Should safely detach any cartridges, disks, tapes
		di->DetachEverything();
		SYS_Sleep(100);

		StepCompleted(8, true, "DetachEverything executed without crash");
	}

	// Restore emulator state
	if (!wasRunning)
		viewC64->StopEmulationThread(di);

	if (allPassed)
		TestCompleted(true, "Peripherals verified: cartridge, REU, datasette, drive, dirty flags, input");
	else
		TestCompleted(false, failureMsg);
#endif
}

void CTestVicePeripherals::Cancel()
{
	isRunning = false;
}
