#include "CTestGT2Oscilloscope.h"
#include "CGT2VoiceWaveforms.h"
#include "CWaveformData.h"
#include "C64DebuggerPluginGoatTracker.h"
#include "CViewGT2Oscilloscope.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include "gsid.h"
	extern unsigned residdelay;
	extern unsigned adparam;
}

static int CountAdjacentChanges(CWaveformData *waveform, int minDelta)
{
	if (waveform == nullptr || waveform->waveformDataRender == nullptr)
		return 0;

	int changes = 0;
	for (int i = 1; i < waveform->waveformDataLength; i++)
	{
		int delta = std::abs((int)waveform->waveformDataRender[i]
			- (int)waveform->waveformDataRender[i - 1]);
		if (delta >= minDelta)
			changes++;
	}
	return changes;
}

static int SampleRange(CWaveformData *waveform)
{
	if (waveform == nullptr || waveform->waveformDataRender == nullptr
		|| waveform->waveformDataLength <= 0)
		return 0;

	int minSample = waveform->waveformDataRender[0];
	int maxSample = waveform->waveformDataRender[0];
	for (int i = 1; i < waveform->waveformDataLength; i++)
	{
		int sample = waveform->waveformDataRender[i];
		minSample = std::min(minSample, sample);
		maxSample = std::max(maxSample, sample);
	}
	return maxSample - minSample;
}

void CTestGT2Oscilloscope::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;

	int step = 0;
	const int sampleWindow = 1024;
	const int minVisibleChanges = 128;

	unsigned char savedSidReg[NUMSIDREGS];
	memcpy(savedSidReg, sidreg, sizeof(savedSidReg));
	unsigned savedResidDelay = residdelay;
	unsigned savedAdParam = adparam;
	float savedVoiceVolume[3] = {
		gt2_voice_volume[0], gt2_voice_volume[1], gt2_voice_volume[2]
	};
	unsigned char savedVoiceMute[3] = {
		gt2_voice_mute[0], gt2_voice_mute[1], gt2_voice_mute[2]
	};
	float savedMasterVolume = gt2_master_volume;

	bool hadWaveforms = gt2VoiceWaveform[0] != nullptr
		|| gt2VoiceWaveform[1] != nullptr
		|| gt2VoiceWaveform[2] != nullptr
		|| gt2MixWaveform != nullptr;

	auto restoreGlobals = [&]() {
		memcpy(sidreg, savedSidReg, sizeof(savedSidReg));
		residdelay = savedResidDelay;
		adparam = savedAdParam;
		for (int i = 0; i < 3; i++)
		{
			gt2_voice_volume[i] = savedVoiceVolume[i];
			gt2_voice_mute[i] = savedVoiceMute[i];
		}
		gt2_master_volume = savedMasterVolume;
		if (!hadWaveforms)
			GT2_VoiceWaveforms_Destroy();
	};

	step++;
	GT2_VoiceWaveforms_Create(sampleWindow);
	bool buffersReady = gt2VoiceWaveform[0] != nullptr && gt2MixWaveform != nullptr
		&& gt2VoiceWaveform[0]->waveformDataLength == sampleWindow
		&& gt2MixWaveform->waveformDataLength == sampleWindow;
	StepCompleted(step, buffersReady, buffersReady
		? "GT2 waveform buffers ready"
		: "GT2 waveform buffers were not allocated with the expected size");
	if (!buffersReady)
	{
		restoreGlobals();
		TestCompleted(false, "GT2 oscilloscope buffer setup failed");
		return;
	}

	step++;
	std::vector<short> audio(sampleWindow, 0);
	residdelay = 0;
	adparam = 0x0f00;
	for (int i = 0; i < 3; i++)
	{
		gt2_voice_volume[i] = 1.0f;
		gt2_voice_mute[i] = 0;
	}
	gt2_master_volume = 1.0f;

	sid_init(44100, 1, 0, 0, 0, 0);
	memset(sidreg, 0, NUMSIDREGS);
	sidreg[0x00] = 0xff;
	sidreg[0x01] = 0xff;
	sidreg[0x05] = 0x00;
	sidreg[0x06] = 0xf0;
	sidreg[0x04] = 0x21; // saw + gate
	sidreg[0x18] = 0x0f;

	int produced = 0;
	for (int pass = 0; pass < 8; pass++)
		produced = sid_fillbuffer(audio.data(), (int)audio.size());
	produced = sid_fillbuffer(audio.data(), (int)audio.size());
	GT2_VoiceWaveforms_UpdatePerFrame();

	int voice0Changes = CountAdjacentChanges(gt2VoiceWaveform[0], 8);
	int voice0Range = SampleRange(gt2VoiceWaveform[0]);
	int mixChanges = CountAdjacentChanges(gt2MixWaveform, 8);
	int mixRange = SampleRange(gt2MixWaveform);

	bool producedSamples = produced == sampleWindow;
	char msg[256];
	snprintf(msg, sizeof(msg),
		"produced=%d voice0 changes=%d range=%d mix changes=%d range=%d",
		produced, voice0Changes, voice0Range, mixChanges, mixRange);
	StepCompleted(step, producedSamples, msg);
	if (!producedSamples)
	{
		restoreGlobals();
		TestCompleted(false, "GT2 SID did not produce the expected audio sample count");
		return;
	}

	step++;
	bool mixMoves = mixChanges > minVisibleChanges && mixRange > 256;
	StepCompleted(step, mixMoves, mixMoves
		? "GT2 mix waveform contains per-sample movement"
		: "GT2 mix waveform did not contain enough variation for the regression");
	if (!mixMoves)
	{
		restoreGlobals();
		TestCompleted(false, "GT2 oscilloscope regression setup did not create moving mix audio");
		return;
	}

	step++;
	bool voiceMoves = voice0Changes > minVisibleChanges && voice0Range > 512;
	snprintf(msg, sizeof(msg),
		"voice0 changes=%d range=%d; expected more than %d adjacent changes",
		voice0Changes, voice0Range, minVisibleChanges);
	StepCompleted(step, voiceMoves, msg);
	if (!voiceMoves)
	{
		restoreGlobals();
		TestCompleted(false, "GT2 oscilloscope voice waveform stayed chunk-flat while mix moved");
		return;
	}

	// --- Regression: render side must NOT depend on the plugin DoFrame chain ---
	// Real-world bug 2026-05-24: GT2 audio runs on its own thread independent
	// of VICE. The plugin's DoFrame (which calls GT2_VoiceWaveforms_UpdatePerFrame
	// → CopySampleData) only fires while VICE refreshes the screen. So the
	// producer side (waveformData, written by the audio thread) fills with
	// real samples but the render side (waveformDataRender, what the
	// oscilloscope draws) stays at zero. Mixer VU works because it reads
	// gt2_voice_level directly; the scope was flat.
	//
	// Fix: CViewGT2Oscilloscope::RenderImGui pulls the snapshot itself via
	// EnsureWaveformsSnapshotForRender(). This step proves that snapshot
	// transfers producer-side samples into render-side arrays, WITHOUT
	// calling GT2_VoiceWaveforms_UpdatePerFrame() manually.
	step++;
	bool snapshotEndToEnd = false;
	char snapshotDetailMsg[256];
	const char *snapshotMsg = snapshotDetailMsg;
	// Plugin is not available in this headless test fixture (the GT2 data
	// tests run only when chardata != NULL, see CTestGT2Patterns). We test
	// the view's snapshot path by constructing a standalone CViewGT2Oscilloscope
	// directly — same code path RenderImGui uses, no plugin dependency.
	CViewGT2Oscilloscope *standaloneView = new CViewGT2Oscilloscope(
		"CTestGT2Oscilloscope::view", 0, 0, 0, 100, 100);
	{
		// Wipe render side so we can prove the snapshot is what fills it.
		for (int i = 0; i < 3; i++)
		{
			if (gt2VoiceWaveform[i] && gt2VoiceWaveform[i]->waveformDataRender)
			{
				memset(gt2VoiceWaveform[i]->waveformDataRender, 0,
					gt2VoiceWaveform[i]->waveformDataLength * sizeof(short));
			}
		}
		if (gt2MixWaveform && gt2MixWaveform->waveformDataRender)
		{
			memset(gt2MixWaveform->waveformDataRender, 0,
				gt2MixWaveform->waveformDataLength * sizeof(short));
		}

		// Drive more samples through the producer side (simulates the
		// AudioQueue thread populating waveformData via the reSID callback)
		// without calling UpdatePerFrame.
		(void)sid_fillbuffer(audio.data(), (int)audio.size());

		// Producer side = waveformData (what the audio thread writes via the
		// reSID callback). Render side = waveformDataRender (what
		// CopySampleData fills, what the oscilloscope reads).
		int producerMaxBeforeSnapshot = 0;
		if (gt2VoiceWaveform[0] && gt2VoiceWaveform[0]->waveformData)
		{
			for (int i = 0; i < gt2VoiceWaveform[0]->waveformDataLength; i++)
			{
				int s = std::abs((int)gt2VoiceWaveform[0]->waveformData[i]);
				if (s > producerMaxBeforeSnapshot) producerMaxBeforeSnapshot = s;
			}
		}
		int renderMaxBeforeSnapshot = 0;
		if (gt2VoiceWaveform[0] && gt2VoiceWaveform[0]->waveformDataRender)
		{
			for (int i = 0; i < gt2VoiceWaveform[0]->waveformDataLength; i++)
			{
				int s = std::abs((int)gt2VoiceWaveform[0]->waveformDataRender[i]);
				if (s > renderMaxBeforeSnapshot) renderMaxBeforeSnapshot = s;
			}
		}

		// View's RenderImGui calls this every paint. The test invokes it
		// directly so the production code path is exercised without needing
		// an ImGui frame.
		standaloneView->EnsureWaveformsSnapshotForRender();

		int renderMaxAfterSnapshot = 0;
		int renderNonZeroAfterSnapshot = 0;
		if (gt2VoiceWaveform[0] && gt2VoiceWaveform[0]->waveformDataRender)
		{
			for (int i = 0; i < gt2VoiceWaveform[0]->waveformDataLength; i++)
			{
				short s = gt2VoiceWaveform[0]->waveformDataRender[i];
				int a = std::abs((int)s);
				if (a > renderMaxAfterSnapshot) renderMaxAfterSnapshot = a;
				if (s != 0) renderNonZeroAfterSnapshot++;
			}
		}

		bool producerHadData = producerMaxBeforeSnapshot > 0;
		bool renderWasEmptyBeforeSnapshot = renderMaxBeforeSnapshot == 0;
		bool renderHasDataAfterSnapshot = renderMaxAfterSnapshot > 0 && renderNonZeroAfterSnapshot > 64;

		snapshotEndToEnd = producerHadData && renderWasEmptyBeforeSnapshot && renderHasDataAfterSnapshot;
		snprintf(snapshotDetailMsg, sizeof(snapshotDetailMsg),
			"producerBefore=%d renderBefore=%d renderAfterMax=%d renderAfterNz=%d",
			producerMaxBeforeSnapshot, renderMaxBeforeSnapshot, renderMaxAfterSnapshot, renderNonZeroAfterSnapshot);
		snapshotMsg = snapshotEndToEnd
			? "GT2 oscilloscope view snapshots producer samples without relying on plugin DoFrame"
			: snapshotDetailMsg;
	}
	StepCompleted(step, snapshotEndToEnd, snapshotMsg);
	delete standaloneView;

	restoreGlobals();
	TestCompleted(voiceMoves && snapshotEndToEnd, snapshotEndToEnd
		? "GT2 oscilloscope captures per-sample voice movement AND view self-drives the snapshot"
		: "GT2 oscilloscope captured samples but view did not snapshot them into the render array");
}
