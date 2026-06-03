#include "C64UTraceBuffer.h"

#include <cstring>

C64UTraceBuffer::C64UTraceBuffer(int capacity)
	: capacity(capacity), writeIndex(0)
{
	buffer = new uint32_t[capacity];
	memset(buffer, 0, capacity * sizeof(uint32_t));
}

C64UTraceBuffer::~C64UTraceBuffer()
{
	delete[] buffer;
}

void C64UTraceBuffer::Append(const uint32_t *entries, int count)
{
	for (int i = 0; i < count; i++)
	{
		uint64_t idx = writeIndex.load(std::memory_order_relaxed);
		buffer[idx % capacity] = entries[i];
		writeIndex.store(idx + 1, std::memory_order_release);
	}
}

uint32_t C64UTraceBuffer::GetRawEntry(uint64_t index) const
{
	return buffer[index % capacity];
}

C64UDebugEntry C64UTraceBuffer::GetEntry(uint64_t index) const
{
	return C64UDebugEntry::Decode(buffer[index % capacity]);
}

uint64_t C64UTraceBuffer::GetWriteIndex() const
{
	return writeIndex.load();
}

uint64_t C64UTraceBuffer::GetEntryCount() const
{
	return writeIndex.load(std::memory_order_acquire);
}

// Must only be called when no writer thread is active
void C64UTraceBuffer::Reset()
{
	writeIndex.store(0, std::memory_order_release);
}

int C64UTraceBuffer::GetCapacity() const
{
	return capacity;
}
