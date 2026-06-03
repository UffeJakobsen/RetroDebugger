#include "C64UIecDecoder.h"

C64UIecDecoder::C64UIecDecoder()
{
	Reset();
}

void C64UIecDecoder::Reset()
{
	transitions.clear();
	lastAtn = false;
	lastClock = false;
	lastData = false;
	initialized = false;
}

void C64UIecDecoder::ProcessEntry(const C64UDebugEntry &entry, uint64_t cycleIndex)
{
	bool atn = entry.GetAtn();
	bool clk = entry.GetIecClock();
	bool dat = entry.GetIecData();

	if (!initialized || atn != lastAtn || clk != lastClock || dat != lastData)
	{
		Transition t;
		t.cycle = cycleIndex;
		t.atn = atn;
		t.clock = clk;
		t.data = dat;
		transitions.push_back(t);

		if ((int)transitions.size() > MAX_TRANSITIONS)
		{
			transitions.pop_front();
		}

		lastAtn = atn;
		lastClock = clk;
		lastData = dat;
		initialized = true;
	}
}

const std::deque<C64UIecDecoder::Transition> &C64UIecDecoder::GetTransitions() const
{
	return transitions;
}
