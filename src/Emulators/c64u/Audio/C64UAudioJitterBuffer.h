#pragma once

#include <mutex>

class C64UAudioJitterBuffer
{
public:
	C64UAudioJitterBuffer(int capacitySamples = 48000);
	~C64UAudioJitterBuffer();

	// Called by UDP receiver thread -- writes interleaved LR float samples
	void Write(const float *interleavedLR, int numStereoSamples);

	// Called by SDL audio callback thread -- reads separated L/R channels
	int Read(float *outL, float *outR, int numSamples);

	// Called by render thread for visualization -- reads without consuming
	int PeekRecentSamples(float *outL, float *outR, int numSamples) const;

	void Reset();

	int GetFillLevel() const;
	float GetFillLevelMs(float sampleRate) const;
	int GetUnderflowCount() const;
	int GetOverflowCount() const;
	int GetCapacity() const;

private:
	float *bufferL;
	float *bufferR;
	int capacity;
	int writePos;
	int readPos;
	int fillLevel;
	int underflowCount;
	int overflowCount;
	mutable std::mutex bufferMutex;
};
