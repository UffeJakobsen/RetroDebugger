#include "CGT2AudioMixer.h"
#include <cstring>
#include <cmath>
#include <cstdlib>

CGT2AudioMixer::CGT2AudioMixer(int maxChannels)
{
	numActiveChannels = (maxChannels < GT2_MIXER_MAX_CHANNELS)
						? maxChannels : GT2_MIXER_MAX_CHANNELS;

	for (int i = 0; i < GT2_MIXER_MAX_CHANNELS; i++)
	{
		channels[i].volume = 1.0f;
		channels[i].pan    = 0.0f;
		channels[i].mute   = false;
		channels[i].solo   = false;
		channels[i].peekL  = 0.0f;
		channels[i].peekR  = 0.0f;
	}

	masterBus.volume = 1.0f;
	masterBus.pan    = 0.0f;
	masterBus.mute   = false;
	masterBus.solo   = false;
	masterBus.peekL  = 0.0f;
	masterBus.peekR  = 0.0f;

	masterVolume = 1.0f;
}

CGT2AudioMixer::~CGT2AudioMixer()
{
	// Effects are owned externally (by the view or plugin) — do not delete here.
	// Callers must manage IGT2AudioEffect lifetime.
}

static int GT2MixerClampChannelCount(const CGT2AudioMixer *mixer, int numChannels)
{
	int limit = (numChannels < 0) ? mixer->numActiveChannels : numChannels;
	if (limit < 0) limit = 0;
	if (limit > mixer->numActiveChannels) limit = mixer->numActiveChannels;
	if (limit > GT2_MIXER_MAX_CHANNELS) limit = GT2_MIXER_MAX_CHANNELS;
	return limit;
}

bool CGT2AudioMixer::HasSolo(int numChannels) const
{
	int limit = GT2MixerClampChannelCount(this, numChannels);
	for (int c = 0; c < limit; c++)
	{
		if (channels[c].solo) return true;
	}
	return false;
}

bool CGT2AudioMixer::IsChannelEffectivelyMuted(int channelIndex, int numChannels) const
{
	if (channelIndex < 0 || channelIndex >= numActiveChannels || channelIndex >= GT2_MIXER_MAX_CHANNELS)
		return true;

	int limit = GT2MixerClampChannelCount(this, numChannels);
	bool soloApplies = channelIndex < limit && HasSolo(limit);
	return channels[channelIndex].mute || (soloApplies && !channels[channelIndex].solo);
}

void CGT2AudioMixer::ApplyVoiceMutes(unsigned char *voiceMute, int numVoices) const
{
	if (voiceMute == nullptr || numVoices <= 0) return;

	for (int v = 0; v < numVoices; v++)
	{
		voiceMute[v] = IsChannelEffectivelyMuted(v, numVoices) ? 1 : 0;
	}
}

void CGT2AudioMixer::ProcessFrame(float **perVoiceL, float **perVoiceR,
								   int numVoices, int numSamples,
								   float *outputL, float *outputR)
{
	// Zero output buffers
	memset(outputL, 0, numSamples * sizeof(float));
	memset(outputR, 0, numSamples * sizeof(float));

	// Temporary per-channel processing buffers (stack-allocated for small frames)
	// Up to 1024 samples per call is typical; guard with a heap fallback.
	float  stackL[1024];
	float  stackR[1024];
	float *tmpL = (numSamples <= 1024) ? stackL : (float*)malloc(numSamples * sizeof(float));
	float *tmpR = (numSamples <= 1024) ? stackR : (float*)malloc(numSamples * sizeof(float));

	for (int c = 0; c < numActiveChannels; c++)
	{
		GT2MixerChannel &ch = channels[c];

		bool active = !IsChannelEffectivelyMuted(c);

		// Use voice buffer if one is supplied, otherwise treat as silence
		int voiceIdx = c;
		if (!active || voiceIdx >= numVoices || perVoiceL == nullptr || perVoiceR == nullptr)
		{
			ch.peekL = 0.0f;
			ch.peekR = 0.0f;
			continue;
		}

		// Copy voice data into temp buffers
		memcpy(tmpL, perVoiceL[voiceIdx], numSamples * sizeof(float));
		memcpy(tmpR, perVoiceR[voiceIdx], numSamples * sizeof(float));

		// Apply effects chain
		for (IGT2AudioEffect *fx : ch.effects)
			fx->Process(tmpL, tmpR, numSamples);

		// Pan law: constant-power (-3 dB at centre)
		float panAngle = (ch.pan + 1.0f) * 0.25f * 3.14159265f;  // [0, pi/2]
		float gainL = ch.volume * cosf(panAngle);
		float gainR = ch.volume * sinf(panAngle);

		// Accumulate into output + update VU peaks
		float peakL = 0.0f;
		float peakR = 0.0f;
		for (int s = 0; s < numSamples; s++)
		{
			float sl = tmpL[s] * gainL;
			float sr = tmpR[s] * gainR;
			outputL[s] += sl;
			outputR[s] += sr;
			float aL = sl < 0.0f ? -sl : sl;
			float aR = sr < 0.0f ? -sr : sr;
			if (aL > peakL) peakL = aL;
			if (aR > peakR) peakR = aR;
		}
		ch.peekL = peakL;
		ch.peekR = peakR;
	}

	// Apply master bus effects chain
	for (IGT2AudioEffect *fx : masterBus.effects)
		fx->Process(outputL, outputR, numSamples);

	// Apply master volume + update master VU peaks
	float masterPeakL = 0.0f;
	float masterPeakR = 0.0f;
	for (int s = 0; s < numSamples; s++)
	{
		outputL[s] *= masterVolume;
		outputR[s] *= masterVolume;
		float aL = outputL[s] < 0.0f ? -outputL[s] : outputL[s];
		float aR = outputR[s] < 0.0f ? -outputR[s] : outputR[s];
		if (aL > masterPeakL) masterPeakL = aL;
		if (aR > masterPeakR) masterPeakR = aR;
	}
	masterBus.peekL = masterPeakL;
	masterBus.peekR = masterPeakR;

	if (tmpL != stackL) { free(tmpL); free(tmpR); }
}
