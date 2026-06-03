#include "CTestSidStatusWaveform.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "CDebugInterfaceVice.h"
#include "CWaveformData.h"
#include "C64SettingsStorage.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include "DebuggerDefs.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C"
{
#include "ViceWrapper.h"
	int resources_get_int(const char *name, int *value_return);
};

// VICE SidEngine resource values (sid/sid.h): 0=fastsid, 1=reSID, 8=reSID-FP.
#define TEST_SID_ENGINE_RESID 1

// Peak-to-peak span of the live waveform buffer.
static int WaveformRange(CWaveformData *w)
{
	if (w == NULL || w->waveformData == NULL || w->waveformDataLength <= 0)
		return 0;

	int minSample = w->waveformData[0];
	int maxSample = w->waveformData[0];
	for (int i = 1; i < w->waveformDataLength; i++)
	{
		int s = w->waveformData[i];
		minSample = std::min(minSample, s);
		maxSample = std::max(maxSample, s);
	}
	return maxSample - minSample;
}

// Count of adjacent samples that differ by at least minDelta. A flat line
// (the regression) yields ~0; a moving waveform yields thousands.
static int WaveformChanges(CWaveformData *w, int minDelta)
{
	if (w == NULL || w->waveformData == NULL || w->waveformDataLength <= 0)
		return 0;

	int changes = 0;
	for (int i = 1; i < w->waveformDataLength; i++)
	{
		int delta = std::abs((int)w->waveformData[i] - (int)w->waveformData[i - 1]);
		if (delta >= minDelta)
			changes++;
	}
	return changes;
}

static void ClearWaveform(CWaveformData *w)
{
	if (w != NULL && w->waveformData != NULL && w->waveformDataLength > 0)
		memset(w->waveformData, 0, w->waveformDataLength * sizeof(signed short));
}

// Movement in the RENDER buffer (what CViewWaveform actually draws on screen).
static int RenderRange(CWaveformData *w)
{
	if (w == NULL || w->waveformDataRender == NULL || w->waveformDataLength <= 0)
		return 0;
	int mn = w->waveformDataRender[0], mx = w->waveformDataRender[0];
	for (int i = 1; i < w->waveformDataLength; i++)
	{
		int s = w->waveformDataRender[i];
		mn = std::min(mn, s);
		mx = std::max(mx, s);
	}
	return mx - mn;
}

void CTestSidStatusWaveform::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;

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

	int savedSidModel = c64SettingsSIDEngineModel;
	int savedRunSid = c64d_setting_run_sid_emulation;

	auto restore = [&]()
	{
		di->SetSIDReceiveChannelsData(0, false);
		c64SettingsSIDEngineModel = savedSidModel;
		di->SetSidType(savedSidModel);
		c64d_setting_run_sid_emulation = savedRunSid;
		SYS_Sleep(300);
		if (!wasRunning)
			viewC64->StopEmulationThread(di);
	};

	char msg[256];

	c64d_setting_run_sid_emulation = 1;
	di->SetEmulationMaximumSpeed(100); // normal speed => Sound resource enabled, not warp
	c64d_sound_resume();

	// Measures voice-1 + mix waveform movement for the engine selected by sidTypeIndex.
	// Returns true if the SID Status oscilloscope receives a moving signal.
	auto probeEngine = [&](int sidTypeIndex, int &outEngine, int &outVoiceChanges, int &outMixChanges,
		int &outVoiceRenderRange, int &outMixRenderRange) -> bool
	{
		// Align the saved-engine global too: CDebugInterfaceVice re-applies
		// c64SettingsSIDEngineModel on model/reset events and would otherwise revert
		// the switch back to the user's saved engine.
		c64SettingsSIDEngineModel = sidTypeIndex;
		di->SetSidTypeAsync(sidTypeIndex); // synchronous engine/model switch (no CPU trap)
		SYS_Sleep(800);                    // let the SID engine reopen

		outEngine = -1;
		resources_get_int("SidEngine", &outEngine);

		di->SetSIDReceiveChannelsData(0, true);

		ClearWaveform(di->sidChannelWaveform[0][0]);
		ClearWaveform(di->sidChannelWaveform[0][1]);
		ClearWaveform(di->sidChannelWaveform[0][2]);
		ClearWaveform(di->sidMixWaveform[0]);

		int voiceChanges = 0, voiceRange = 0, mixChanges = 0, mixRange = 0;
		for (int attempt = 0; attempt < 20; attempt++)
		{
			// Voice 1: mid frequency, max sustain, full volume, sawtooth + gate.
			di->SetByteC64(0xD400, 0x00);
			di->SetByteC64(0xD401, 0x20);
			di->SetByteC64(0xD405, 0x00);
			di->SetByteC64(0xD406, 0xF0);
			di->SetByteC64(0xD418, 0x0F);
			di->SetByteC64(0xD404, 0x21);

			SYS_Sleep(100);

			voiceChanges = WaveformChanges(di->sidChannelWaveform[0][0], 4);
			voiceRange = WaveformRange(di->sidChannelWaveform[0][0]);
			mixChanges = WaveformChanges(di->sidMixWaveform[0], 4);
			mixRange = WaveformRange(di->sidMixWaveform[0]);

			if (voiceChanges > 256 && voiceRange > 256 && mixChanges > 256 && mixRange > 256)
				break;
		}

		// Now exercise the REAL on-screen path: copy live -> render buffer + trigger,
		// exactly like CViewC64::Update -> UpdateWaveforms() does each frame.
		di->UpdateWaveforms();
		int voiceRenderRange = RenderRange(di->sidChannelWaveform[0][0]);
		int mixRenderRange = RenderRange(di->sidMixWaveform[0]);

		outVoiceChanges = voiceChanges;
		outMixChanges = mixChanges;
		outVoiceRenderRange = voiceRenderRange;
		outMixRenderRange = mixRenderRange;
		// The view is flat unless the RENDER buffer moves too.
		return voiceChanges > 256 && voiceRange > 256 && mixChanges > 256 && mixRange > 256
			&& voiceRenderRange > 256 && mixRenderRange > 256;
	};

	// Engine index -> sid.h engine (see SetSidTypeAsync): 0 = reSID/6581, 3 = fastsid/6581,
	// 5 = reSID-FP. The fix targets the plain reSID path (sid/resid.cpp).
	struct { int index; const char *name; } engines[] = {
		{ 0, "reSID" },
		{ 3, "fastSID" },
		{ 5, "reSID-FP" },
	};

	bool allOk = true;
	for (int e = 0; e < 3; e++)
	{
		int liveEngine = -1, voiceChanges = 0, mixChanges = 0, voiceRender = 0, mixRender = 0;
		bool moves = probeEngine(engines[e].index, liveEngine, voiceChanges, mixChanges, voiceRender, mixRender);
		snprintf(msg, sizeof(msg),
			"%s (SidEngine=%d): live voiceCh=%d mixCh=%d  render voiceRange=%d mixRange=%d -> %s",
			engines[e].name, liveEngine, voiceChanges, mixChanges, voiceRender, mixRender,
			moves ? "MOVES" : "FLAT");
		StepCompleted(e + 1, moves, msg);
		if (!moves)
			allOk = false;
	}

	restore();

	if (!allOk)
	{
		TestCompleted(false, "SID Status oscilloscope is flat for at least one SID engine");
		return;
	}

	TestCompleted(true, "SID Status oscilloscope receives moving waveform data on all SID engines");
#endif
}
