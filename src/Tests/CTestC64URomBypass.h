#pragma once

#include "CTest.h"
#include "../Emulators/c64u/State/C64URomBypass.h"

#include <cstdio>

class CTestC64URomBypass : public CTest
{
public:
	virtual const char *GetName() override { return "C64URomBypass"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

		// Step 1: Test OverlapsRom -- ROM regions
		if (!C64URomBypass::OverlapsRom(0xA000, 1))
		{
			TestCompleted(false, "OverlapsRom: $A000+1 should overlap BASIC ROM");
			return;
		}
		if (!C64URomBypass::OverlapsRom(0xBFFF, 1))
		{
			TestCompleted(false, "OverlapsRom: $BFFF+1 should overlap BASIC ROM");
			return;
		}
		if (!C64URomBypass::OverlapsRom(0xE000, 1))
		{
			TestCompleted(false, "OverlapsRom: $E000+1 should overlap KERNAL ROM");
			return;
		}
		if (!C64URomBypass::OverlapsRom(0xFFFF, 1))
		{
			TestCompleted(false, "OverlapsRom: $FFFF+1 should overlap KERNAL ROM");
			return;
		}
		if (!C64URomBypass::OverlapsRom(0x9000, 0x2000))
		{
			TestCompleted(false, "OverlapsRom: $9000+$2000 should span into BASIC ROM");
			return;
		}
		if (!C64URomBypass::OverlapsRom(0xD000, 0x2000))
		{
			TestCompleted(false, "OverlapsRom: $D000+$2000 should span into KERNAL ROM");
			return;
		}
		StepCompleted(1, true, "OverlapsRom correctly detects ROM regions");

		// Step 2: Test OverlapsRom -- non-ROM regions
		if (C64URomBypass::OverlapsRom(0x0000, 0x100))
		{
			TestCompleted(false, "OverlapsRom: $0000+$100 should not overlap ROM");
			return;
		}
		if (C64URomBypass::OverlapsRom(0xC000, 0x1000))
		{
			TestCompleted(false, "OverlapsRom: $C000+$1000 should not overlap ROM (between ROMs)");
			return;
		}
		if (C64URomBypass::OverlapsRom(0xD000, 0x1000))
		{
			TestCompleted(false, "OverlapsRom: $D000+$1000 should not overlap ROM (I/O area)");
			return;
		}
		if (C64URomBypass::OverlapsRom(0x8000, 0x2000))
		{
			TestCompleted(false, "OverlapsRom: $8000+$2000 should not overlap ROM");
			return;
		}
		if (C64URomBypass::OverlapsRom(0x0000, 0))
		{
			TestCompleted(false, "OverlapsRom: zero length should not overlap ROM");
			return;
		}
		StepCompleted(2, true, "OverlapsRom correctly rejects non-ROM regions");

		// Step 3: Test BuildCopyStub basic structure
		auto stub = C64URomBypass::BuildCopyStub(0xE000, 0x4000, 0x2000, 0xFE47);
		if (stub.size() < 30 || stub.size() > 80)
		{
			char msg[128];
			snprintf(msg, sizeof(msg), "BuildCopyStub: unexpected size %d (expected 30-80)", (int)stub.size());
			TestCompleted(false, msg);
			return;
		}

		// First byte must be PHA ($48)
		if (stub[0] != 0x48)
		{
			char msg[128];
			snprintf(msg, sizeof(msg), "BuildCopyStub: first byte=$%02X expected=$48 (PHA)", stub[0]);
			TestCompleted(false, msg);
			return;
		}

		// Last 3 bytes must be JMP originalNmi ($4C, $47, $FE)
		size_t sz = stub.size();
		if (stub[sz - 3] != 0x4C)
		{
			char msg[128];
			snprintf(msg, sizeof(msg), "BuildCopyStub: JMP opcode=$%02X expected=$4C", stub[sz - 3]);
			TestCompleted(false, msg);
			return;
		}
		if (stub[sz - 2] != 0x47 || stub[sz - 1] != 0xFE)
		{
			char msg[128];
			snprintf(msg, sizeof(msg), "BuildCopyStub: JMP target=$%02X%02X expected=$FE47",
					 stub[sz - 1], stub[sz - 2]);
			TestCompleted(false, msg);
			return;
		}
		StepCompleted(3, true, "BuildCopyStub produces correct stub structure");

		// Step 4: Test BuildCopyStub with different parameters
		auto stub2 = C64URomBypass::BuildCopyStub(0xA000, 0x4000, 0x1000, 0xEA31);
		if (stub2.size() < 30)
		{
			TestCompleted(false, "BuildCopyStub: second stub too small");
			return;
		}

		// Verify JMP target matches the new originalNmi
		size_t sz2 = stub2.size();
		if (stub2[sz2 - 3] != 0x4C || stub2[sz2 - 2] != 0x31 || stub2[sz2 - 1] != 0xEA)
		{
			char msg[128];
			snprintf(msg, sizeof(msg), "BuildCopyStub: second stub JMP target=$%02X%02X expected=$EA31",
					 stub2[sz2 - 1], stub2[sz2 - 2]);
			TestCompleted(false, msg);
			return;
		}

		// Both stubs should be the same size (structure is identical, only data differs)
		if (stub.size() != stub2.size())
		{
			char msg[128];
			snprintf(msg, sizeof(msg), "BuildCopyStub: stubs should be same size (%d vs %d)",
					 (int)stub.size(), (int)stub2.size());
			TestCompleted(false, msg);
			return;
		}

		// Verify the stub contains LDA #$34 (ROM bank-out): $A9 $34
		bool foundBankOut = false;
		for (size_t i = 0; i + 1 < stub.size(); i++)
		{
			if (stub[i] == 0xA9 && stub[i + 1] == 0x34)
			{
				foundBankOut = true;
				break;
			}
		}
		if (!foundBankOut)
		{
			TestCompleted(false, "BuildCopyStub: missing LDA #$34 (ROM bank-out)");
			return;
		}

		// Verify the stub contains the completion marker: LDA #$42, STA $02
		bool foundMarker = false;
		for (size_t i = 0; i + 3 < stub.size(); i++)
		{
			if (stub[i] == 0xA9 && stub[i + 1] == 0x42 &&
				stub[i + 2] == 0x85 && stub[i + 3] == 0x02)
			{
				foundMarker = true;
				break;
			}
		}
		if (!foundMarker)
		{
			TestCompleted(false, "BuildCopyStub: missing completion marker (LDA #$42, STA $02)");
			return;
		}
		StepCompleted(4, true, "BuildCopyStub encodes correct 6502 instructions");

		TestCompleted(true, "C64URomBypass static helpers work correctly");
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
