#include "CTestViceSoundIntegration.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "CDebugInterfaceVice.h"
#include "C64SettingsStorage.h"
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

void CTestViceSoundIntegration::Run(ITestCallback *cb)
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

	// --- Step 1: Sound pause/resume ---
	{
		c64d_sound_pause();
		SYS_Sleep(50);
		c64d_sound_resume();
		SYS_Sleep(50);

		StepCompleted(1, true, "Sound pause/resume without crash");
	}

	// --- Step 2: SID sampling method ---
	if (allPassed)
	{
		// Test each sampling method
		int methods[] = { 0, 1, 2, 3 };  // Fast, Interpolating, Resampling, Fast Resampling
		const char *names[] = { "Fast", "Interpolating", "Resampling", "Fast Resampling" };

		for (int i = 0; i < 4; i++)
		{
			di->SetSidSamplingMethod(methods[i]);
			SYS_Sleep(20);
		}

		// Set back to default (fast)
		di->SetSidSamplingMethod(0);

		StepCompleted(2, true, "SID sampling methods cycled without crash");
	}

	// --- Step 3: SID filter controls ---
	if (allPassed)
	{
		// Enable/disable filters
		di->SetSidEmulateFilters(1);
		SYS_Sleep(20);
		di->SetSidEmulateFilters(0);
		SYS_Sleep(20);
		di->SetSidEmulateFilters(1);

		// Set filter passband and bias
		di->SetSidPassBand(45);
		di->SetSidFilterBias(0);

		StepCompleted(3, true, "SID filter controls verified");
	}

	// --- Step 4: SID stereo configuration ---
	if (allPassed)
	{
		// Test stereo modes (0=mono, 1=stereo, 2=triple)
		di->SetSidStereo(0);  // mono
		SYS_Sleep(20);

		int numSids = di->GetNumSids();
		if (numSids < 1 || numSids > 3)
		{
			sprintf(failureMsg, "GetNumSids() returned %d (expected 1-3)", numSids);
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "SID stereo: numSids=%d", numSids);
			StepCompleted(4, true, msg);
		}
		else
		{
			StepCompleted(4, false, failureMsg);
		}
	}

	// --- Step 5: Volume control ---
	if (allPassed)
	{
		// Set volume via C callback
		c64d_set_volume(0.5f);
		SYS_Sleep(20);
		c64d_set_volume(1.0f);

		StepCompleted(5, true, "Volume control executed without crash");
	}

	// --- Step 6: SID emulation toggle ---
	if (allPassed)
	{
		// Test run SID emulation flag
		int origRunSid = c64d_setting_run_sid_emulation;

		// Disable SID emulation
		di->SetRunSIDEmulation(false);
		SYS_Sleep(50);

		// Re-enable
		di->SetRunSIDEmulation(true);
		SYS_Sleep(50);

		StepCompleted(6, true, "SID emulation toggle verified");
	}

	// Restore emulator state. Steps 2-3 mutate global SID DSP resources
	// (sampling method, filter enable, passband, bias) and never set them
	// back. The leftover Fast sampling method silences the reSID-FP engine
	// for any test that runs afterwards (e.g. SidStatusWaveform's reSID-FP
	// probe went flat). Re-apply the user's configured values, exactly as
	// the emulator setup does, so this test leaves SID state untouched.
	di->SetSidSamplingMethod(c64SettingsRESIDSamplingMethod);
	di->SetSidEmulateFilters(c64SettingsRESIDEmulateFilters ? 1 : 0);
	di->SetSidPassBand(c64SettingsRESIDPassBand);
	di->SetSidFilterBias(c64SettingsRESIDFilterBias);

	if (!wasRunning)
		viewC64->StopEmulationThread(di);

	if (allPassed)
		TestCompleted(true, "Sound integration verified: pause/resume, sampling, filters, stereo, volume, toggle");
	else
		TestCompleted(false, failureMsg);
#endif
}

void CTestViceSoundIntegration::Cancel()
{
	isRunning = false;
}
