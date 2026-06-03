#pragma once

#include "CTest.h"
#include "../Emulators/c64u/Trace/C64UDebugEntry.h"
#include "../Emulators/c64u/Trace/C64UTraceBuffer.h"
#include "../Emulators/c64u/Transport/C64UDebugStream.h"

#include <cstring>

class CTestC64UDebugProtocol : public CTest
{
public:
	virtual const char *GetName() override { return "C64UDebugProtocol"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

		// Step 1: C64UDebugEntry::Decode basic
		{
			uint32_t raw = (1u << 31) | (1u << 24) | (0x42u << 16) | 0x0400u;
			C64UDebugEntry e = C64UDebugEntry::Decode(raw);
			if (e.address != 0x0400)
			{
				TestCompleted(false, "Decode: address should be 0x0400");
				return;
			}
			if (e.data != 0x42)
			{
				TestCompleted(false, "Decode: data should be 0x42");
				return;
			}
			if (!e.rw)
			{
				TestCompleted(false, "Decode: rw should be true (read)");
				return;
			}
			if (!e.GetPhi2())
			{
				TestCompleted(false, "Decode: GetPhi2() should be true");
				return;
			}
		}
		StepCompleted(1, true, "C64UDebugEntry::Decode basic fields correct");

		// Step 2: Signal accessors 6510
		{
			// All signal bits set: bits 31-25 = 1111111, bit 24 = 1 (R/W)
			uint32_t raw = 0xFFu << 24;
			C64UDebugEntry e = C64UDebugEntry::Decode(raw);
			if (!e.GetPhi2())
			{
				TestCompleted(false, "6510 signals: GetPhi2() should be true");
				return;
			}
			if (!e.GetGame())
			{
				TestCompleted(false, "6510 signals: GetGame() should be true");
				return;
			}
			if (!e.GetExrom())
			{
				TestCompleted(false, "6510 signals: GetExrom() should be true");
				return;
			}
			if (!e.GetBA())
			{
				TestCompleted(false, "6510 signals: GetBA() should be true");
				return;
			}
			if (!e.GetIrq())
			{
				TestCompleted(false, "6510 signals: GetIrq() should be true");
				return;
			}
			if (!e.GetRom())
			{
				TestCompleted(false, "6510 signals: GetRom() should be true");
				return;
			}
			if (!e.GetNmi())
			{
				TestCompleted(false, "6510 signals: GetNmi() should be true");
				return;
			}
		}
		StepCompleted(2, true, "6510 signal accessors all correct");

		// Step 3: Signal accessors 1541
		{
			// Set bits for 1541 signals: ATN(6), IEC_DATA(5), IEC_CLK(4), SYNC(3), BYTE_READY(2)
			// bit31=0 (no PHI2), bits 30-26 = 11111, bit25=0, bit24=0
			uint32_t raw = (1u << 30) | (1u << 29) | (1u << 28) | (1u << 27) | (1u << 26);
			C64UDebugEntry e = C64UDebugEntry::Decode(raw);
			if (!e.GetAtn())
			{
				TestCompleted(false, "1541 signals: GetAtn() should be true");
				return;
			}
			if (!e.GetIecData())
			{
				TestCompleted(false, "1541 signals: GetIecData() should be true");
				return;
			}
			if (!e.GetIecClock())
			{
				TestCompleted(false, "1541 signals: GetIecClock() should be true");
				return;
			}
			if (!e.GetSync())
			{
				TestCompleted(false, "1541 signals: GetSync() should be true");
				return;
			}
			if (!e.GetByteReady())
			{
				TestCompleted(false, "1541 signals: GetByteReady() should be true");
				return;
			}
			// PHI2 should be false (bit 31 = 0)
			if (e.GetPhi2())
			{
				TestCompleted(false, "1541 signals: GetPhi2() should be false");
				return;
			}
		}
		StepCompleted(3, true, "1541 signal accessors all correct");

		// Step 4: Write detection (R/W# = 0)
		{
			// bit24=0 means write
			uint32_t raw = (1u << 31) | (0x42u << 16) | 0x0400u;  // no bit 24
			C64UDebugEntry e = C64UDebugEntry::Decode(raw);
			if (e.rw)
			{
				TestCompleted(false, "Write detection: rw should be false for write cycle");
				return;
			}
		}
		StepCompleted(4, true, "Write cycle detection correct");

		// Step 5: Trace buffer append/read
		{
			C64UTraceBuffer buf(1024);
			uint32_t entries[500];
			for (int i = 0; i < 500; i++)
			{
				entries[i] = (uint32_t)(0xDEAD0000u | i);
			}
			buf.Append(entries, 500);

			bool allMatch = true;
			for (int i = 0; i < 500; i++)
			{
				uint32_t got = buf.GetRawEntry(i);
				uint32_t expected = (uint32_t)(0xDEAD0000u | i);
				if (got != expected)
				{
					allMatch = false;
					break;
				}
			}
			if (!allMatch)
			{
				TestCompleted(false, "Trace buffer: append/read mismatch");
				return;
			}
		}
		StepCompleted(5, true, "Trace buffer append/read 500 entries correct");

		// Step 6: Trace buffer wrap
		{
			C64UTraceBuffer buf(100);
			uint32_t entries[150];
			for (int i = 0; i < 150; i++)
			{
				entries[i] = (uint32_t)(0xCAFE0000u | i);
			}
			buf.Append(entries, 150);

			// First 50 entries (indices 0-49) should be overwritten by entries 100-149
			// Entries at indices 50-99 should be entries[50]-entries[99]
			bool wrapCorrect = true;
			for (int i = 50; i < 100; i++)
			{
				uint32_t got = buf.GetRawEntry(i);
				uint32_t expected = (uint32_t)(0xCAFE0000u | i);
				if (got != expected)
				{
					wrapCorrect = false;
					break;
				}
			}
			if (!wrapCorrect)
			{
				TestCompleted(false, "Trace buffer wrap: entries 50-99 should survive");
				return;
			}

			// Slots 0-49 should now hold entries 100-149
			bool overwriteCorrect = true;
			for (int i = 100; i < 150; i++)
			{
				uint32_t got = buf.GetRawEntry(i);  // i % 100 = 0..49
				uint32_t expected = (uint32_t)(0xCAFE0000u | i);
				if (got != expected)
				{
					overwriteCorrect = false;
					break;
				}
			}
			if (!overwriteCorrect)
			{
				TestCompleted(false, "Trace buffer wrap: overwritten slots should hold new entries");
				return;
			}
		}
		StepCompleted(6, true, "Trace buffer wrap-around correct");

		// Step 7: Trace buffer count
		{
			C64UTraceBuffer buf(100);
			uint32_t entries[200];
			for (int i = 0; i < 200; i++)
			{
				entries[i] = (uint32_t)i;
			}
			buf.Append(entries, 200);

			if (buf.GetEntryCount() != 200)
			{
				char msg[128];
				snprintf(msg, sizeof(msg), "Trace buffer count: expected 200, got %llu",
						 (unsigned long long)buf.GetEntryCount());
				TestCompleted(false, msg);
				return;
			}
			if (buf.GetWriteIndex() != 200)
			{
				char msg[128];
				snprintf(msg, sizeof(msg), "Trace buffer writeIndex: expected 200, got %llu",
						 (unsigned long long)buf.GetWriteIndex());
				TestCompleted(false, msg);
				return;
			}
		}
		StepCompleted(7, true, "Trace buffer count and writeIndex correct after wrap");

		// Step 8: Packet parsing
		{
			uint8_t packet[1444];
			memset(packet, 0, sizeof(packet));

			// Header: seq = 0x0042
			packet[0] = 0x42;
			packet[1] = 0x00;
			// reserved
			packet[2] = 0x00;
			packet[3] = 0x00;

			// Fill 360 entries with known values
			for (int i = 0; i < 360; i++)
			{
				uint32_t val = (uint32_t)(0xBEEF0000u | i);
				int offset = 4 + i * 4;
				packet[offset + 0] = (uint8_t)(val & 0xFF);
				packet[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
				packet[offset + 2] = (uint8_t)((val >> 16) & 0xFF);
				packet[offset + 3] = (uint8_t)((val >> 24) & 0xFF);
			}

			uint16_t seq = 0;
			uint32_t parsedEntries[360];
			int entryCount = 0;

			if (!C64UDebugStream::ParsePacket(packet, 1444, seq, parsedEntries, 360, entryCount))
			{
				TestCompleted(false, "Packet parsing: ParsePacket returned false for valid packet");
				return;
			}

			if (seq != 0x0042)
			{
				char msg[128];
				snprintf(msg, sizeof(msg), "Packet parsing: seq should be 0x0042, got 0x%04X", seq);
				TestCompleted(false, msg);
				return;
			}

			if (entryCount != 360)
			{
				char msg[128];
				snprintf(msg, sizeof(msg), "Packet parsing: entryCount should be 360, got %d", entryCount);
				TestCompleted(false, msg);
				return;
			}

			bool allMatch = true;
			for (int i = 0; i < 360; i++)
			{
				uint32_t expected = (uint32_t)(0xBEEF0000u | i);
				if (parsedEntries[i] != expected)
				{
					allMatch = false;
					break;
				}
			}
			if (!allMatch)
			{
				TestCompleted(false, "Packet parsing: parsed entries do not match input");
				return;
			}
		}
		StepCompleted(8, true, "Packet parsing extracts 360 entries correctly");

		TestCompleted(true, "All debug stream protocol tests passed");
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
