#ifndef _CAUDIOCHANNELC64U_H_
#define _CAUDIOCHANNELC64U_H_

#include "CAudioChannel.h"

class CDebugInterfaceC64U;
class C64UAudioJitterBuffer;

class CAudioChannelC64U : public CAudioChannel
{
public:
	CAudioChannelC64U(CDebugInterfaceC64U *debugInterface);

	virtual void Start() override;
	virtual void FillBuffer(int *mixBuffer, u32 numSamples);

	CDebugInterfaceC64U *debugInterface;
	C64UAudioJitterBuffer *jitterBuffer;

	float resamplePos;
	float resampleStep;   // sourceRate / outputRate (~47983/44100)
	float prevSampleL;
	float prevSampleR;

	// Pre-buffering: wait for jitter buffer to fill before starting playback
	bool prebufferingComplete;
	int prebufferThreshold;  // samples needed before playback starts
};

#endif
