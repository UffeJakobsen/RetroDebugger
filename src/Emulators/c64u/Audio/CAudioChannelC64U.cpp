#include "SYS_Types.h"
#include "C64SettingsStorage.h"
#include "CViewC64.h"
#include "DebuggerDefs.h"

#include "CAudioChannelC64U.h"
#include "C64UAudioJitterBuffer.h"
#include "../CDebugInterfaceC64U.h"

#include <cstring>
#include <algorithm>

// The C64 Ultimate audio source runs at ~47983 Hz.
// The SDL audio output runs at 44100 Hz.
static const float C64U_SOURCE_RATE = 47983.0f;
static const float SDL_OUTPUT_RATE = 44100.0f;

CAudioChannelC64U::CAudioChannelC64U(CDebugInterfaceC64U *debugInterface)
: CAudioChannel("C64U")
{
	this->debugInterface = debugInterface;
	this->jitterBuffer = NULL;
	this->bypass = true;

	resamplePos = 0.0f;
	resampleStep = C64U_SOURCE_RATE / SDL_OUTPUT_RATE;
	prevSampleL = 0.0f;
	prevSampleR = 0.0f;
	prebufferingComplete = false;
	prebufferThreshold = 4800;  // ~100ms at 47983 Hz
}

void CAudioChannelC64U::Start()
{
	prebufferingComplete = false;
	resamplePos = 0.0f;
	resampleStep = C64U_SOURCE_RATE / SDL_OUTPUT_RATE;
	prevSampleL = 0.0f;
	prevSampleR = 0.0f;
	CAudioChannel::Start();
}

// FillBuffer is called by the SDL audio mixer.
// numSamples = number of stereo sample pairs to produce.
// mixBuffer layout: interleaved i16 L, R pairs (4 bytes per "sample").
void CAudioChannelC64U::FillBuffer(int *mixBuffer, u32 numSamples)
{
	if (jitterBuffer == NULL)
	{
		memset(mixBuffer, 0, numSamples * 4);
		return;
	}

	// Pre-buffering: output silence until jitter buffer has enough samples
	if (!prebufferingComplete)
	{
		if (jitterBuffer->GetFillLevel() < prebufferThreshold)
		{
			memset(mixBuffer, 0, numSamples * 4);
			return;
		}
		prebufferingComplete = true;
	}

	// Adaptive rate correction: adjust resampleStep to compensate for clock
	// drift between the hardware and local SDL clock. If the jitter buffer
	// is draining (fill below target), we're consuming too fast — slow down.
	// If it's filling up, we're consuming too slow — speed up.
	// Correction is capped at ±2% to avoid audible pitch shift.
	{
		float fill   = (float)jitterBuffer->GetFillLevel();
		float target = (float)prebufferThreshold;
		if (target > 0.0f)
		{
			float error = (fill - target) / target;   // negative = draining, positive = filling
			float corr  = error * 0.05f;
			if (corr >  0.02f) corr =  0.02f;
			if (corr < -0.02f) corr = -0.02f;
			resampleStep = (C64U_SOURCE_RATE / SDL_OUTPUT_RATE) * (1.0f + corr);
		}
	}

	// Calculate how many source samples we need from the jitter buffer
	int sourceSamplesNeeded = (int)(numSamples * resampleStep) + 2;
	if (sourceSamplesNeeded < 4)
		sourceSamplesNeeded = 4;

	// Allocate temp buffers on stack (reasonable size for audio -- max ~2048 samples)
	float *srcL = (float *)alloca(sourceSamplesNeeded * sizeof(float));
	float *srcR = (float *)alloca(sourceSamplesNeeded * sizeof(float));

	int actualRead = jitterBuffer->Read(srcL, srcR, sourceSamplesNeeded);

	// If nothing was read, output silence and re-enter prebuffering
	if (actualRead == 0)
	{
		memset(mixBuffer, 0, numSamples * 4);
		prebufferingComplete = false;
		resamplePos = 0.0f;
		return;
	}

	// Linear interpolation resampling
	i16 *outPtr = (i16 *)mixBuffer;

	for (u32 i = 0; i < numSamples; i++)
	{
		int idx = (int)resamplePos;
		float frac = resamplePos - (float)idx;

		float sL, sR;

		if (idx + 1 < actualRead)
		{
			sL = srcL[idx] + frac * (srcL[idx + 1] - srcL[idx]);
			sR = srcR[idx] + frac * (srcR[idx + 1] - srcR[idx]);
		}
		else if (idx < actualRead)
		{
			sL = srcL[idx];
			sR = srcR[idx];
		}
		else
		{
			// Past the end of available data — hold last sample instead of silence
			sL = prevSampleL;
			sR = prevSampleR;
		}

		// Clamp to [-1.0, 1.0] then convert to i16
		if (sL > 1.0f) sL = 1.0f;
		if (sL < -1.0f) sL = -1.0f;
		if (sR > 1.0f) sR = 1.0f;
		if (sR < -1.0f) sR = -1.0f;

		*outPtr++ = (i16)(sL * 32767.0f);
		*outPtr++ = (i16)(sR * 32767.0f);

		resamplePos += resampleStep;
	}

	// Track last samples for potential crossfade
	if (actualRead > 0)
	{
		prevSampleL = srcL[actualRead - 1];
		prevSampleR = srcR[actualRead - 1];
	}

	// Adjust resamplePos: subtract the consumed source samples.
	// Keep the fractional remainder for smooth interpolation continuity.
	resamplePos -= (float)actualRead;
	if (resamplePos < 0.0f)
		resamplePos = 0.0f;

	// Mute if paused and user has enabled mute-on-pause
	if (c64SettingsMuteSIDOnPause)
	{
		if (debugInterface->GetDebugMode() != DEBUGGER_MODE_RUNNING)
		{
			memset(mixBuffer, 0, numSamples * 4);
		}
	}
}
