#ifndef _C64UTRACEBUFFER_H_
#define _C64UTRACEBUFFER_H_

#include "C64UDebugEntry.h"

#include <atomic>
#include <cstdint>

class C64UTraceBuffer
{
public:
	C64UTraceBuffer(int capacity = 4 * 1024 * 1024);
	~C64UTraceBuffer();

	void Append(const uint32_t *entries, int count);  // called by receiver thread
	uint32_t GetRawEntry(uint64_t index) const;
	C64UDebugEntry GetEntry(uint64_t index) const;
	uint64_t GetWriteIndex() const;
	uint64_t GetEntryCount() const;
	void Reset();
	int GetCapacity() const;

private:
	uint32_t *buffer;
	int capacity;
	std::atomic<uint64_t> writeIndex;
};

#endif
