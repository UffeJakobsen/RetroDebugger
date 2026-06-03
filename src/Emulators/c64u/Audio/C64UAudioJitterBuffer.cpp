#include "C64UAudioJitterBuffer.h"
#include <cstring>
#include <algorithm>

C64UAudioJitterBuffer::C64UAudioJitterBuffer(int capacitySamples)
: capacity(capacitySamples)
, writePos(0)
, readPos(0)
, fillLevel(0)
, underflowCount(0)
, overflowCount(0)
{
	bufferL = new float[capacity];
	bufferR = new float[capacity];
	memset(bufferL, 0, capacity * sizeof(float));
	memset(bufferR, 0, capacity * sizeof(float));
}

C64UAudioJitterBuffer::~C64UAudioJitterBuffer()
{
	delete[] bufferL;
	delete[] bufferR;
}

void C64UAudioJitterBuffer::Write(const float *interleavedLR, int numStereoSamples)
{
	std::lock_guard<std::mutex> lock(bufferMutex);

	for (int i = 0; i < numStereoSamples; i++)
	{
		bufferL[writePos] = interleavedLR[i * 2];
		bufferR[writePos] = interleavedLR[i * 2 + 1];
		writePos = (writePos + 1) % capacity;

		if (fillLevel < capacity)
		{
			fillLevel++;
		}
		else
		{
			// Overflow: drop oldest sample by advancing readPos
			readPos = (readPos + 1) % capacity;
			overflowCount++;
		}
	}
}

int C64UAudioJitterBuffer::Read(float *outL, float *outR, int numSamples)
{
	std::lock_guard<std::mutex> lock(bufferMutex);

	int available = std::min(numSamples, fillLevel);

	for (int i = 0; i < available; i++)
	{
		outL[i] = bufferL[readPos];
		outR[i] = bufferR[readPos];
		readPos = (readPos + 1) % capacity;
	}

	fillLevel -= available;

	// Fill remainder with silence
	if (available < numSamples)
	{
		memset(outL + available, 0, (numSamples - available) * sizeof(float));
		memset(outR + available, 0, (numSamples - available) * sizeof(float));
		underflowCount++;
	}

	return available;
}

int C64UAudioJitterBuffer::PeekRecentSamples(float *outL, float *outR, int numSamples) const
{
	std::lock_guard<std::mutex> lock(bufferMutex);

	int available = std::min(numSamples, fillLevel);

	// Read the last 'available' samples before writePos (looking backward)
	for (int i = 0; i < available; i++)
	{
		int idx = (writePos - available + i + capacity) % capacity;
		outL[i] = bufferL[idx];
		outR[i] = bufferR[idx];
	}

	return available;
}

void C64UAudioJitterBuffer::Reset()
{
	std::lock_guard<std::mutex> lock(bufferMutex);
	writePos = 0;
	readPos = 0;
	fillLevel = 0;
}

int C64UAudioJitterBuffer::GetFillLevel() const
{
	std::lock_guard<std::mutex> lock(bufferMutex);
	return fillLevel;
}

float C64UAudioJitterBuffer::GetFillLevelMs(float sampleRate) const
{
	std::lock_guard<std::mutex> lock(bufferMutex);
	if (sampleRate <= 0.0f)
		return 0.0f;
	return (float)fillLevel / sampleRate * 1000.0f;
}

int C64UAudioJitterBuffer::GetUnderflowCount() const
{
	std::lock_guard<std::mutex> lock(bufferMutex);
	return underflowCount;
}

int C64UAudioJitterBuffer::GetOverflowCount() const
{
	std::lock_guard<std::mutex> lock(bufferMutex);
	return overflowCount;
}

int C64UAudioJitterBuffer::GetCapacity() const
{
	return capacity;
}
