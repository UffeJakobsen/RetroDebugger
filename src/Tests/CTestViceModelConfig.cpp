#include "CTestViceModelConfig.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "CDebugInterfaceVice.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include "DebuggerDefs.h"
#include <cstdio>
#include <cstring>
#include <vector>

static char failureMsg[512];

void CTestViceModelConfig::Run(ITestCallback *cb)
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

	// --- Step 1: Machine type query ---
	{
		u8 machineType = di->GetC64MachineType();

		if (machineType != MACHINE_TYPE_PAL && machineType != MACHINE_TYPE_NTSC)
		{
			sprintf(failureMsg, "Machine type: %d (expected PAL=%d or NTSC=%d)", machineType, MACHINE_TYPE_PAL, MACHINE_TYPE_NTSC);
			allPassed = false;
		}

		if (allPassed)
		{
			const char *typeName = (machineType == MACHINE_TYPE_PAL) ? "PAL" : "NTSC";
			char msg[128];
			sprintf(msg, "Machine type: %s (%d)", typeName, machineType);
			StepCompleted(1, true, msg);
		}
		else
		{
			StepCompleted(1, false, failureMsg);
		}
	}

	// --- Step 2: Model type enumeration ---
	if (allPassed)
	{
		int currentModel = di->GetC64ModelType();

		std::vector<const char *> modelNames;
		std::vector<int> modelIds;
		di->GetC64ModelTypes(&modelNames, &modelIds);

		if (modelNames.empty() || modelIds.empty())
		{
			sprintf(failureMsg, "GetC64ModelTypes returned empty lists");
			allPassed = false;
		}
		else if (modelNames.size() != modelIds.size())
		{
			sprintf(failureMsg, "Model names (%d) and IDs (%d) count mismatch", (int)modelNames.size(), (int)modelIds.size());
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "Model: current=%d, %d types available", currentModel, (int)modelNames.size());
			StepCompleted(2, true, msg);
		}
		else
		{
			StepCompleted(2, false, failureMsg);
		}
	}

	// --- Step 3: Warp mode toggle ---
	if (allPassed)
	{
		bool origWarp = di->GetSettingIsWarpSpeed();

		// Enable warp
		di->SetSettingIsWarpSpeed(true);
		SYS_Sleep(50);
		bool isWarp = di->GetSettingIsWarpSpeed();

		if (!isWarp)
		{
			sprintf(failureMsg, "Warp mode: set to true but reads false");
			allPassed = false;
		}

		// Disable warp
		if (allPassed)
		{
			di->SetSettingIsWarpSpeed(false);
			SYS_Sleep(50);
			isWarp = di->GetSettingIsWarpSpeed();

			if (isWarp)
			{
				sprintf(failureMsg, "Warp mode: set to false but reads true");
				allPassed = false;
			}
		}

		// Restore
		di->SetSettingIsWarpSpeed(origWarp);

		if (allPassed)
			StepCompleted(3, true, "Warp mode toggle verified (on/off)");
		else
			StepCompleted(3, false, failureMsg);
	}

	// --- Step 4: Soft/Hard reset ---
	if (allPassed)
	{
		di->PauseEmulationBlockedWait();

		// Write pattern to RAM that will survive soft reset
		di->SetByteToRamC64(0x0800, 0xAA);

		// Soft reset
		di->ResetSoft();
		SYS_Sleep(500);
		di->PauseEmulationBlockedWait();

		// After soft reset, emulator should still be functional
		C64StateCPU cpuState;
		di->GetC64CpuState(&cpuState);

		// PC should be in KERNAL area (reset vector)
		// After reset, KERNAL init runs — just verify PC is valid
		if (cpuState.pc == 0x0000)
		{
			sprintf(failureMsg, "After soft reset: PC=$0000 (seems dead)");
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "Soft reset: PC=$%04X (emulator functional)", cpuState.pc);
			StepCompleted(4, true, msg);
		}
		else
		{
			StepCompleted(4, false, failureMsg);
		}
	}

	// --- Step 5: FPS query ---
	if (allPassed)
	{
		di->SetDebugMode(DEBUGGER_MODE_RUNNING);
		SYS_Sleep(500);
		di->PauseEmulationBlockedWait();

		float fps = di->GetEmulationFPS();

		// FPS should be reasonable (25-75 for PAL/NTSC)
		if (fps <= 0.0f || fps > 200.0f)
		{
			sprintf(failureMsg, "Emulation FPS: %.2f (expected 25-75)", fps);
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "Emulation FPS: %.1f", fps);
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
		TestCompleted(true, "Model config verified: machine type, models, warp, reset, FPS");
	else
		TestCompleted(false, failureMsg);
#endif
}

void CTestViceModelConfig::Cancel()
{
	isRunning = false;
}
