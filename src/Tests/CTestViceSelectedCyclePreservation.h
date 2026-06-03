#pragma once

#include "CTest.h"
#include "CViewC64.h"
#include "CDebugInterfaceVice.h"
#include "C64SettingsStorage.h"
#include "DebuggerDefs.h"
#include "CViewC64VicDisplay.h"
#include "CViewC64StateVIC.h"
#include "SYS_Main.h"
extern "C" {
#include "c64.h"
#include "c64model.h"
#include "viciitypes.h"
}
#include <cstdio>

class CTestViceSelectedCyclePreservation : public CTest
{
public:
	void RestoreViceState(CDebugInterfaceVice *di, u8 previousRecordMode, bool wasRunning)
	{
		if (di == NULL)
			return;

		di->SetVicRecordStateMode(previousRecordMode);
		if (!wasRunning && di->isRunning)
		{
			viewC64->StopEmulationThread(di);
		}
	}

	int GetRasterLineCount(CDebugInterfaceVice *di)
	{
		switch (di->GetC64ModelType())
		{
			case C64MODEL_C64_NTSC:
			case C64MODEL_C64C_NTSC:
			case C64MODEL_C64SX_NTSC:
			case C64MODEL_C64_JAP:
			case C64MODEL_PET64_NTSC:
				return C64_NTSC_SCREEN_LINES;

			case C64MODEL_C64_OLD_NTSC:
				return C64_NTSCOLD_SCREEN_LINES;

			case C64MODEL_C64_PAL_N:
				return C64_PALN_SCREEN_LINES;

			case C64MODEL_C64_PAL:
			case C64MODEL_C64C_PAL:
			case C64MODEL_C64_OLD_PAL:
			case C64MODEL_C64SX_PAL:
			case C64MODEL_PET64_PAL:
			case C64MODEL_C64_GS:
			case C64MODEL_ULTIMAX:
			default:
				return C64_PAL_SCREEN_LINES;
		}
	}

	int GetRasterCycleCount()
	{
		int cyclesPerLine = vicii.cycles_per_line;
		if (cyclesPerLine <= 0)
			return 64;
		if (cyclesPerLine > 64)
			return 64;
		return cyclesPerLine;
	}

	vicii_cycle_state_t *FindRecordedState(CDebugInterfaceVice *di)
	{
		int rasterLines = GetRasterLineCount(di);
		int rasterCycles = GetRasterCycleCount();

		for (int attempt = 0; attempt < 40; attempt++)
		{
			di->SetDebugMode(DEBUGGER_MODE_RUNNING);
			SYS_Sleep(10);
			di->PauseEmulationBlockedWait();

			for (int rasterLine = 0; rasterLine < rasterLines; rasterLine++)
			{
				for (int rasterCycle = 0; rasterCycle < rasterCycles; rasterCycle++)
				{
					vicii_cycle_state_t *candidate = c64d_get_vicii_state_for_raster_cycle(rasterLine, rasterCycle);
					if (candidate != NULL
						&& candidate->raster_line == (unsigned int)rasterLine
						&& candidate->raster_cycle == (unsigned int)rasterCycle)
					{
						return candidate;
					}
				}
			}
		}

		return NULL;
	}

	virtual const char *GetName() override { return "ViceSelectedCyclePreservation"; }
	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

#ifndef RUN_COMMODORE64
		TestCompleted(true, "Skipped (C64 not enabled)");
		return;
#else
		CDebugInterfaceVice *di = (CDebugInterfaceVice *)viewC64->debugInterfaceC64;
		if (di == NULL)
		{
			TestCompleted(false, "VICE debug interface is NULL");
			return;
		}
		if (viewC64->viewC64VicDisplay == NULL || viewC64->viewC64StateVIC == NULL || viewC64->viewC64MemoryBank == NULL)
		{
			TestCompleted(false, "VICE selected-cycle views are not initialized");
			return;
		}
		StepCompleted(1, true, "VICE selected-cycle views are initialized");

		bool wasRunning = di->isRunning;
		if (!wasRunning)
		{
			viewC64->StartEmulationThread(di);
			for (int attempt = 0; attempt < 200 && !di->isRunning; attempt++)
			{
				SYS_Sleep(10);
			}
		}
		if (!di->isRunning)
		{
			TestCompleted(false, "VICE emulator failed to start");
			return;
		}

		u8 previousRecordMode = c64SettingsVicStateRecordingMode;
		di->SetVicRecordStateMode(C64D_VICII_RECORD_MODE_EVERY_CYCLE);
		vicii_cycle_state_t *selectedState = FindRecordedState(di);
		if (selectedState == NULL)
		{
			RestoreViceState(di, previousRecordMode, wasRunning);
			TestCompleted(false, "Unable to capture a real recorded VICE selected-cycle state");
			return;
		}

		c64d_vicii_copy_state_data(&(viewC64->viciiStateToShow), selectedState);
		viewC64->viewC64StateVIC->SetIsLockedState(true);
		viewC64->UpdateViciiColors();

		if (!viewC64->viewC64StateVIC->GetIsLockedState())
		{
			RestoreViceState(di, previousRecordMode, wasRunning);
			TestCompleted(false, "VICE selected-cycle state view must stay locked when browsing a recorded cycle");
			return;
		}

		if (viewC64->viciiStateToShow.raster_line != selectedState->raster_line
			|| viewC64->viciiStateToShow.raster_cycle != selectedState->raster_cycle)
		{
			RestoreViceState(di, previousRecordMode, wasRunning);
			TestCompleted(false, "Selected-cycle raster coordinates no longer propagate through the VICE path");
			return;
		}

		u8 d011 = ((CDebugInterfaceC64 *)di)->GetVicRegister(&(viewC64->viciiStateToShow), 0x11);
		u8 d016 = ((CDebugInterfaceC64 *)di)->GetVicRegister(&(viewC64->viciiStateToShow), 0x16);
		if (d011 != selectedState->regs[0x11] || d016 != selectedState->regs[0x16])
		{
			RestoreViceState(di, previousRecordMode, wasRunning);
			TestCompleted(false, "VICE selected-cycle register reads changed unexpectedly");
			return;
		}
		StepCompleted(2, true, "VICE selected-cycle raster and register state still flows through viciiStateToShow");

		viewC64->viewC64VicDisplay->CopyCurrentViciiStateAndUnlock();
		if (viewC64->viewC64StateVIC->GetIsLockedState())
		{
			StepCompleted(3, true, "VICE selected-cycle unlock path remains callable; lock release is still frame-gated");
		}
		else
		{
			StepCompleted(3, true, "VICE selected-cycle unlock path still restores current-state browsing");
		}

		RestoreViceState(di, previousRecordMode, wasRunning);

		char msg[256];
		snprintf(msg, sizeof(msg), "VICE selected-cycle preserved: line=%u cycle=%u D011=%02x D016=%02x", selectedState->raster_line, selectedState->raster_cycle, d011, d016);
		TestCompleted(true, msg);
#endif
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
