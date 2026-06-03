#ifndef _C64UDEBUGENTRY_H_
#define _C64UDEBUGENTRY_H_

#include <cstdint>

struct C64UDebugEntry
{
	uint16_t address;
	uint8_t data;
	bool rw;            // true=read, false=write (bit 24)
	uint8_t signals;    // raw bits 31-24

	static C64UDebugEntry Decode(uint32_t raw)
	{
		C64UDebugEntry e;
		e.address = (uint16_t)(raw & 0xFFFF);
		e.data = (uint8_t)((raw >> 16) & 0xFF);
		e.rw = ((raw >> 24) & 1) != 0;
		e.signals = (uint8_t)((raw >> 24) & 0xFF);
		return e;
	}

	// 6510/VIC signal accessors (bits 31-25 of the original uint32)
	bool GetPhi2() const { return (signals >> 7) & 1; }
	bool GetGame() const { return (signals >> 6) & 1; }
	bool GetExrom() const { return (signals >> 5) & 1; }
	bool GetBA() const { return (signals >> 4) & 1; }
	bool GetIrq() const { return (signals >> 3) & 1; }
	bool GetRom() const { return (signals >> 2) & 1; }
	bool GetNmi() const { return (signals >> 1) & 1; }
	// 1541 signal accessors (same bit positions, different meaning)
	bool GetAtn() const { return (signals >> 6) & 1; }
	bool GetIecData() const { return (signals >> 5) & 1; }
	bool GetIecClock() const { return (signals >> 4) & 1; }
	bool GetSync() const { return (signals >> 3) & 1; }
	bool GetByteReady() const { return (signals >> 2) & 1; }
};

#endif
