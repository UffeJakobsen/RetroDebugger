#include "CGT2EffectBiquadFilter.h"
#include "imgui.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

CGT2EffectBiquadFilter::CGT2EffectBiquadFilter()
{
	filterType = GT2_BIQUAD_LOWPASS;
	cutoffHz   = 1000.0f;
	Q          = 0.707f;
	sampleRate = 44100.0f;

	b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
	a1 = 0.0f; a2 = 0.0f;

	xL1 = xL2 = yL1 = yL2 = 0.0f;
	xR1 = xR2 = yR1 = yR2 = 0.0f;

	// Force first-frame recalculation
	lastFilterType = (GT2BiquadFilterType)-1;
	lastCutoffHz   = -1.0f;
	lastQ          = -1.0f;

	RecalcCoefficients();
}

const char* CGT2EffectBiquadFilter::GetName()
{
	return "Biquad Filter";
}

// Robert Bristow-Johnson Audio EQ Cookbook coefficients.
// https://www.w3.org/TR/audio-eq-cookbook/
void CGT2EffectBiquadFilter::RecalcCoefficients()
{
	float w0    = 2.0f * (float)M_PI * cutoffHz / sampleRate;
	float cosW0 = cosf(w0);
	float sinW0 = sinf(w0);
	float alpha = sinW0 / (2.0f * Q);

	float b0_raw, b1_raw, b2_raw, a0_raw, a1_raw, a2_raw;

	switch (filterType)
	{
		case GT2_BIQUAD_HIGHPASS:
			b0_raw =  (1.0f + cosW0) / 2.0f;
			b1_raw = -(1.0f + cosW0);
			b2_raw =  (1.0f + cosW0) / 2.0f;
			a0_raw =   1.0f + alpha;
			a1_raw =  -2.0f * cosW0;
			a2_raw =   1.0f - alpha;
			break;

		case GT2_BIQUAD_BANDPASS:
			// BPF with constant 0 dB peak gain
			b0_raw =  alpha;
			b1_raw =  0.0f;
			b2_raw = -alpha;
			a0_raw =  1.0f + alpha;
			a1_raw = -2.0f * cosW0;
			a2_raw =  1.0f - alpha;
			break;

		case GT2_BIQUAD_LOWPASS:
		default:
			b0_raw =  (1.0f - cosW0) / 2.0f;
			b1_raw =   1.0f - cosW0;
			b2_raw =  (1.0f - cosW0) / 2.0f;
			a0_raw =   1.0f + alpha;
			a1_raw =  -2.0f * cosW0;
			a2_raw =   1.0f - alpha;
			break;
	}

	// Normalise by a0
	b0 = b0_raw / a0_raw;
	b1 = b1_raw / a0_raw;
	b2 = b2_raw / a0_raw;
	a1 = a1_raw / a0_raw;
	a2 = a2_raw / a0_raw;

	lastFilterType = filterType;
	lastCutoffHz   = cutoffHz;
	lastQ          = Q;
}

void CGT2EffectBiquadFilter::Process(float *bufferL, float *bufferR, int numSamples)
{
	// Recalculate if parameters changed
	if (filterType != lastFilterType || cutoffHz != lastCutoffHz || Q != lastQ)
		RecalcCoefficients();

	for (int i = 0; i < numSamples; i++)
	{
		// Left channel
		float xL = bufferL[i];
		float yL = b0 * xL + b1 * xL1 + b2 * xL2 - a1 * yL1 - a2 * yL2;
		xL2 = xL1;  xL1 = xL;
		yL2 = yL1;  yL1 = yL;
		bufferL[i] = yL;

		// Right channel
		float xR = bufferR[i];
		float yR = b0 * xR + b1 * xR1 + b2 * xR2 - a1 * yR1 - a2 * yR2;
		xR2 = xR1;  xR1 = xR;
		yR2 = yR1;  yR1 = yR;
		bufferR[i] = yR;
	}
}

void CGT2EffectBiquadFilter::RenderParamsImGui()
{
	static const char* typeNames[] = { "Lowpass", "Highpass", "Bandpass" };
	int typeIndex = (int)filterType;
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Type");
	for (int i = 0; i < 3; i++)
	{
		ImGui::SameLine();
		if (ImGui::RadioButton(typeNames[i], typeIndex == i))
		{
			typeIndex = i;
			filterType = (GT2BiquadFilterType)typeIndex;
		}
	}

	ImGui::SliderFloat("Cutoff Hz##bqcutoff", &cutoffHz, 20.0f, 20000.0f, "%.1f Hz",
					   ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Q##bqq", &Q, 0.1f, 10.0f, "%.3f");
}

void CGT2EffectBiquadFilter::Reset()
{
	xL1 = xL2 = yL1 = yL2 = 0.0f;
	xR1 = xR2 = yR1 = yR2 = 0.0f;
}
