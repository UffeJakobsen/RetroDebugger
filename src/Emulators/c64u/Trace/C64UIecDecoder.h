#ifndef _C64UIECDECODER_H_
#define _C64UIECDECODER_H_

#include "C64UDebugEntry.h"

#include <cstdint>
#include <deque>

class C64UIecDecoder
{
public:
	struct Transition
	{
		uint64_t cycle;
		bool atn, clock, data;
	};

	C64UIecDecoder();

	void ProcessEntry(const C64UDebugEntry &entry, uint64_t cycleIndex);
	const std::deque<Transition> &GetTransitions() const;
	void Reset();

private:
	std::deque<Transition> transitions;
	bool lastAtn, lastClock, lastData;
	bool initialized;
	static const int MAX_TRANSITIONS = 10000;
};

#endif
