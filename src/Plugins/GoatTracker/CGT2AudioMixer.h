#ifndef _CGT2AudioMixer_H_
#define _CGT2AudioMixer_H_

#include "IGT2AudioEffect.h"
#include <vector>

#define GT2_MIXER_MAX_CHANNELS 12

struct GT2MixerChannel
{
	float volume;   // 0.0-1.0
	float pan;      // -1.0 to 1.0
	bool mute;
	bool solo;
	std::vector<IGT2AudioEffect*> effects;
	float peekL, peekR;  // VU meter levels
};

class CGT2AudioMixer
{
public:
	CGT2AudioMixer(int maxChannels);
	~CGT2AudioMixer();

	int numActiveChannels;
	GT2MixerChannel channels[GT2_MIXER_MAX_CHANNELS];
	GT2MixerChannel masterBus;
	float masterVolume;

	// Process per-voice buffers through channel effects + mix to stereo output
	void ProcessFrame(float **perVoiceL, float **perVoiceR, int numVoices, int numSamples,
					  float *outputL, float *outputR);
	bool HasSolo(int numChannels = -1) const;
	bool IsChannelEffectivelyMuted(int channelIndex, int numChannels = -1) const;
	void ApplyVoiceMutes(unsigned char *voiceMute, int numVoices) const;
};

#endif
