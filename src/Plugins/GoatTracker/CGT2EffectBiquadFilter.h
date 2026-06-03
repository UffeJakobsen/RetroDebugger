#ifndef _CGT2EffectBiquadFilter_H_
#define _CGT2EffectBiquadFilter_H_

#include "IGT2AudioEffect.h"

enum GT2BiquadFilterType
{
	GT2_BIQUAD_LOWPASS  = 0,
	GT2_BIQUAD_HIGHPASS = 1,
	GT2_BIQUAD_BANDPASS = 2
};

class CGT2EffectBiquadFilter : public IGT2AudioEffect
{
public:
	CGT2EffectBiquadFilter();
	virtual ~CGT2EffectBiquadFilter() {}

	virtual const char* GetName() override;
	virtual void Process(float *bufferL, float *bufferR, int numSamples) override;
	virtual void RenderParamsImGui() override;
	virtual void Reset() override;

	GT2BiquadFilterType filterType;
	float cutoffHz;
	float Q;
	float sampleRate;

private:
	void RecalcCoefficients();

	// Biquad coefficients
	float b0, b1, b2;
	float a1, a2;

	// Delay buffers: x1/x2 = input history, y1/y2 = output history
	float xL1, xL2, yL1, yL2;  // left channel
	float xR1, xR2, yR1, yR2;  // right channel

	// Track last-computed params to avoid redundant recalculation
	GT2BiquadFilterType lastFilterType;
	float lastCutoffHz;
	float lastQ;
};

#endif
