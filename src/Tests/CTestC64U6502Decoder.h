#pragma once

#include "CTest.h"
#include "../Emulators/c64u/Trace/C64UDebugEntry.h"
#include "../Emulators/c64u/Trace/C64U6502Decoder.h"

#include <cstdio>

class CTestC64U6502Decoder : public CTest
{
public:
	virtual const char *GetName() override { return "C64U6502Decoder"; }

	// Helper to construct a raw trace entry uint32
	static uint32_t MakeEntry(uint16_t addr, uint8_t data, bool read, bool phi2)
	{
		uint32_t raw = addr;
		raw |= ((uint32_t)data << 16);
		if (read)  raw |= (1u << 24);   // R/W# = 1 for read
		if (phi2)  raw |= (1u << 31);   // PHI2
		return raw;
	}

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

		C64U6502Decoder decoder;

		// ------------------------------------------------------------------
		// Step 1: NOP at $0810
		// NOP = 0xEA, 2 cycles, 1 byte
		// Cycle 1: opcode fetch (read 0xEA from $0810)
		// Cycle 2: dead cycle (read from $0811, thrown away)
		// ------------------------------------------------------------------
		{
			C64UDebugEntry e1 = C64UDebugEntry::Decode(MakeEntry(0x0810, 0xEA, true, true));
			C64UDecoderAnnotation a1 = decoder.ProcessEntry(e1);
			if (a1.type != C64UDecoderAnnotation::OPCODE_FETCH)
			{
				TestCompleted(false, "Step1: NOP cycle 1 should be OPCODE_FETCH");
				return;
			}
			if (a1.instructionPC != 0x0810)
			{
				TestCompleted(false, "Step1: NOP cycle 1 PC should be 0x0810");
				return;
			}

			C64UDebugEntry e2 = C64UDebugEntry::Decode(MakeEntry(0x0811, 0xEA, true, true));
			C64UDecoderAnnotation a2 = decoder.ProcessEntry(e2);
			// Cycle 2 of NOP: past the 1-byte operand, read cycle, last cycle -> DATA_READ
			// (NOP has 1 byte, so cycle 2 is past operand bytes; it's a read on the last cycle)
			if (a2.type != C64UDecoderAnnotation::DATA_READ)
			{
				char msg[128];
				snprintf(msg, sizeof(msg), "Step1: NOP cycle 2 should be DATA_READ, got %d", (int)a2.type);
				TestCompleted(false, msg);
				return;
			}
			if (a2.instructionPC != 0x0810)
			{
				TestCompleted(false, "Step1: NOP cycle 2 PC should still be 0x0810");
				return;
			}

			if (decoder.GetCurrentPC() != 0x0810)
			{
				TestCompleted(false, "Step1: GetCurrentPC should be 0x0810");
				return;
			}
			if (!decoder.IsSynced())
			{
				TestCompleted(false, "Step1: IsSynced should be true");
				return;
			}
		}
		StepCompleted(1, true, "NOP at $0810: 2-cycle decode correct");

		// ------------------------------------------------------------------
		// Step 2: LDA $0400 (absolute) at $0810
		// LDA abs = 0xAD, 4 cycles, 3 bytes
		// Cycle 1: opcode fetch (read 0xAD from $0810)
		// Cycle 2: operand low (read 0x00 from $0811)
		// Cycle 3: operand high (read 0x04 from $0812)
		// Cycle 4: data read (read 0x42 from $0400)
		// ------------------------------------------------------------------
		{
			decoder.Reset();
			decoder.SetTraceMode(1);  // 6510 only

			C64UDebugEntry e1 = C64UDebugEntry::Decode(MakeEntry(0x0810, 0xAD, true, true));
			C64UDecoderAnnotation a1 = decoder.ProcessEntry(e1);
			if (a1.type != C64UDecoderAnnotation::OPCODE_FETCH || a1.instructionPC != 0x0810)
			{
				TestCompleted(false, "Step2: LDA abs cycle 1 should be OPCODE_FETCH at $0810");
				return;
			}

			C64UDebugEntry e2 = C64UDebugEntry::Decode(MakeEntry(0x0811, 0x00, true, true));
			C64UDecoderAnnotation a2 = decoder.ProcessEntry(e2);
			if (a2.type != C64UDecoderAnnotation::OPERAND || a2.instructionPC != 0x0810)
			{
				TestCompleted(false, "Step2: LDA abs cycle 2 should be OPERAND at $0810");
				return;
			}

			C64UDebugEntry e3 = C64UDebugEntry::Decode(MakeEntry(0x0812, 0x04, true, true));
			C64UDecoderAnnotation a3 = decoder.ProcessEntry(e3);
			if (a3.type != C64UDecoderAnnotation::OPERAND || a3.instructionPC != 0x0810)
			{
				TestCompleted(false, "Step2: LDA abs cycle 3 should be OPERAND at $0810");
				return;
			}

			C64UDebugEntry e4 = C64UDebugEntry::Decode(MakeEntry(0x0400, 0x42, true, true));
			C64UDecoderAnnotation a4 = decoder.ProcessEntry(e4);
			if (a4.type != C64UDecoderAnnotation::DATA_READ || a4.instructionPC != 0x0810)
			{
				TestCompleted(false, "Step2: LDA abs cycle 4 should be DATA_READ at $0810");
				return;
			}
		}
		StepCompleted(2, true, "LDA $0400 at $0810: 4-cycle decode correct");

		// ------------------------------------------------------------------
		// Step 3: STA $D020 (absolute) at $0813
		// STA abs = 0x8D, 4 cycles, 3 bytes
		// Cycle 1: opcode fetch (read 0x8D from $0813)
		// Cycle 2: operand low (read 0x20 from $0814)
		// Cycle 3: operand high (read 0xD0 from $0815)
		// Cycle 4: data write (write 0x0E to $D020)
		// ------------------------------------------------------------------
		{
			C64UDebugEntry e1 = C64UDebugEntry::Decode(MakeEntry(0x0813, 0x8D, true, true));
			C64UDecoderAnnotation a1 = decoder.ProcessEntry(e1);
			if (a1.type != C64UDecoderAnnotation::OPCODE_FETCH || a1.instructionPC != 0x0813)
			{
				TestCompleted(false, "Step3: STA abs cycle 1 should be OPCODE_FETCH at $0813");
				return;
			}

			C64UDebugEntry e2 = C64UDebugEntry::Decode(MakeEntry(0x0814, 0x20, true, true));
			C64UDecoderAnnotation a2 = decoder.ProcessEntry(e2);
			if (a2.type != C64UDecoderAnnotation::OPERAND)
			{
				TestCompleted(false, "Step3: STA abs cycle 2 should be OPERAND");
				return;
			}

			C64UDebugEntry e3 = C64UDebugEntry::Decode(MakeEntry(0x0815, 0xD0, true, true));
			C64UDecoderAnnotation a3 = decoder.ProcessEntry(e3);
			if (a3.type != C64UDecoderAnnotation::OPERAND)
			{
				TestCompleted(false, "Step3: STA abs cycle 3 should be OPERAND");
				return;
			}

			C64UDebugEntry e4 = C64UDebugEntry::Decode(MakeEntry(0xD020, 0x0E, false, true));
			C64UDecoderAnnotation a4 = decoder.ProcessEntry(e4);
			if (a4.type != C64UDecoderAnnotation::DATA_WRITE || a4.instructionPC != 0x0813)
			{
				TestCompleted(false, "Step3: STA abs cycle 4 should be DATA_WRITE at $0813");
				return;
			}
		}
		StepCompleted(3, true, "STA $D020 at $0813: 4-cycle write decode correct");

		// ------------------------------------------------------------------
		// Step 4: JMP $0810 at $0816
		// JMP abs = 0x4C, 3 cycles, 3 bytes
		// Cycle 1: opcode fetch (read 0x4C from $0816)
		// Cycle 2: operand low (read 0x10 from $0817)
		// Cycle 3: operand high (read 0x08 from $0818)
		// After this, the decoder should be ready for the next opcode fetch
		// ------------------------------------------------------------------
		{
			C64UDebugEntry e1 = C64UDebugEntry::Decode(MakeEntry(0x0816, 0x4C, true, true));
			C64UDecoderAnnotation a1 = decoder.ProcessEntry(e1);
			if (a1.type != C64UDecoderAnnotation::OPCODE_FETCH || a1.instructionPC != 0x0816)
			{
				TestCompleted(false, "Step4: JMP abs cycle 1 should be OPCODE_FETCH at $0816");
				return;
			}

			C64UDebugEntry e2 = C64UDebugEntry::Decode(MakeEntry(0x0817, 0x10, true, true));
			C64UDecoderAnnotation a2 = decoder.ProcessEntry(e2);
			if (a2.type != C64UDecoderAnnotation::OPERAND)
			{
				TestCompleted(false, "Step4: JMP abs cycle 2 should be OPERAND");
				return;
			}

			C64UDebugEntry e3 = C64UDebugEntry::Decode(MakeEntry(0x0818, 0x08, true, true));
			C64UDecoderAnnotation a3 = decoder.ProcessEntry(e3);
			if (a3.type != C64UDecoderAnnotation::OPERAND)
			{
				TestCompleted(false, "Step4: JMP abs cycle 3 should be OPERAND");
				return;
			}

			// Now the decoder should be back to WAITING_OPCODE.
			// Feed the next opcode fetch at $0810 (the JMP target) to prove it.
			C64UDebugEntry eNext = C64UDebugEntry::Decode(MakeEntry(0x0810, 0xEA, true, true));
			C64UDecoderAnnotation aNext = decoder.ProcessEntry(eNext);
			if (aNext.type != C64UDecoderAnnotation::OPCODE_FETCH || aNext.instructionPC != 0x0810)
			{
				TestCompleted(false, "Step4: After JMP, next fetch should be OPCODE_FETCH at $0810");
				return;
			}
		}
		StepCompleted(4, true, "JMP $0810 at $0816: 3-cycle decode, next fetch at target");

		// ------------------------------------------------------------------
		// Step 5: Interrupt detection via IRQ vector fetch
		// Simulate reading from $FFFE (IRQ vector low byte)
		// ------------------------------------------------------------------
		{
			decoder.Reset();

			C64UDebugEntry e1 = C64UDebugEntry::Decode(MakeEntry(0xFFFE, 0x48, true, true));
			C64UDecoderAnnotation a1 = decoder.ProcessEntry(e1);
			if (a1.type != C64UDecoderAnnotation::INTERRUPT)
			{
				TestCompleted(false, "Step5: Read from $FFFE should be INTERRUPT");
				return;
			}
			if (a1.instructionPC != 0xFFFE)
			{
				TestCompleted(false, "Step5: INTERRUPT PC should be $FFFE");
				return;
			}

			// Also test NMI vector ($FFFA)
			decoder.Reset();
			C64UDebugEntry e2 = C64UDebugEntry::Decode(MakeEntry(0xFFFA, 0x00, true, true));
			C64UDecoderAnnotation a2 = decoder.ProcessEntry(e2);
			if (a2.type != C64UDecoderAnnotation::INTERRUPT)
			{
				TestCompleted(false, "Step5: Read from $FFFA should be INTERRUPT");
				return;
			}

			// Also test RESET vector ($FFFC)
			decoder.Reset();
			C64UDebugEntry e3 = C64UDebugEntry::Decode(MakeEntry(0xFFFC, 0x00, true, true));
			C64UDecoderAnnotation a3 = decoder.ProcessEntry(e3);
			if (a3.type != C64UDecoderAnnotation::INTERRUPT)
			{
				TestCompleted(false, "Step5: Read from $FFFC should be INTERRUPT");
				return;
			}
		}
		StepCompleted(5, true, "Interrupt detection: IRQ/NMI/RESET vector fetches annotated correctly");

		// ------------------------------------------------------------------
		// Step 6: Packet loss recovery via Reset()
		// Process some entries, Reset(), then feed new entries.
		// Verify IsSynced() is false after Reset, true after first opcode fetch.
		// ------------------------------------------------------------------
		{
			decoder.Reset();
			decoder.SetTraceMode(1);

			// Process a partial instruction
			C64UDebugEntry e1 = C64UDebugEntry::Decode(MakeEntry(0x0810, 0xAD, true, true));
			decoder.ProcessEntry(e1);  // opcode fetch for LDA abs

			if (!decoder.IsSynced())
			{
				TestCompleted(false, "Step6: Should be synced after opcode fetch");
				return;
			}

			// Simulate packet loss
			decoder.Reset();

			if (decoder.IsSynced())
			{
				TestCompleted(false, "Step6: IsSynced should be false after Reset");
				return;
			}
			if (decoder.GetCurrentPC() != 0)
			{
				TestCompleted(false, "Step6: GetCurrentPC should be 0 after Reset");
				return;
			}

			// Feed a new opcode fetch — should re-sync
			C64UDebugEntry e2 = C64UDebugEntry::Decode(MakeEntry(0x0900, 0xEA, true, true));
			C64UDecoderAnnotation a2 = decoder.ProcessEntry(e2);
			if (!decoder.IsSynced())
			{
				TestCompleted(false, "Step6: Should be synced after new opcode fetch post-Reset");
				return;
			}
			if (a2.type != C64UDecoderAnnotation::OPCODE_FETCH)
			{
				TestCompleted(false, "Step6: First entry after Reset should be OPCODE_FETCH");
				return;
			}
			if (a2.instructionPC != 0x0900)
			{
				TestCompleted(false, "Step6: PC after re-sync should be 0x0900");
				return;
			}
		}
		StepCompleted(6, true, "Packet loss recovery: Reset clears state, re-syncs on next fetch");

		// ------------------------------------------------------------------
		// Step 7: Mode filtering (6510 & VIC, traceMode=3)
		// Only PHI2=1 entries should be processed
		// ------------------------------------------------------------------
		{
			C64U6502Decoder modeDecoder;
			modeDecoder.SetTraceMode(3);

			// PHI2=1 (CPU cycle) — should process
			C64UDebugEntry cpuEntry = C64UDebugEntry::Decode(MakeEntry(0x0810, 0xEA, true, true));
			if (!modeDecoder.ShouldProcessEntry(cpuEntry))
			{
				TestCompleted(false, "Step7: traceMode=3, PHI2=1 entry should be processed");
				return;
			}

			// PHI2=0 (VIC cycle) — should NOT process
			C64UDebugEntry vicEntry = C64UDebugEntry::Decode(MakeEntry(0x0810, 0xEA, true, false));
			if (modeDecoder.ShouldProcessEntry(vicEntry))
			{
				TestCompleted(false, "Step7: traceMode=3, PHI2=0 entry should NOT be processed");
				return;
			}

			// Also test traceMode=5 (6510 & 1541) — same PHI2 filtering
			modeDecoder.SetTraceMode(5);
			if (!modeDecoder.ShouldProcessEntry(cpuEntry))
			{
				TestCompleted(false, "Step7: traceMode=5, PHI2=1 entry should be processed");
				return;
			}
			if (modeDecoder.ShouldProcessEntry(vicEntry))
			{
				TestCompleted(false, "Step7: traceMode=5, PHI2=0 entry should NOT be processed");
				return;
			}
		}
		StepCompleted(7, true, "Mode filtering: traceMode=3 and 5 filter by PHI2");

		// ------------------------------------------------------------------
		// Step 8: VIC Only mode (traceMode=2)
		// Decoder should reject all entries
		// ------------------------------------------------------------------
		{
			C64U6502Decoder vicDecoder;
			vicDecoder.SetTraceMode(2);

			C64UDebugEntry e1 = C64UDebugEntry::Decode(MakeEntry(0x0810, 0xEA, true, true));
			if (vicDecoder.ShouldProcessEntry(e1))
			{
				TestCompleted(false, "Step8: traceMode=2 (VIC Only), CPU entry should NOT be processed");
				return;
			}

			C64UDebugEntry e2 = C64UDebugEntry::Decode(MakeEntry(0x0810, 0xEA, true, false));
			if (vicDecoder.ShouldProcessEntry(e2))
			{
				TestCompleted(false, "Step8: traceMode=2 (VIC Only), VIC entry should NOT be processed by 6502 decoder");
				return;
			}
		}
		StepCompleted(8, true, "VIC Only mode: 6502 decoder rejects all entries");

		// ------------------------------------------------------------------
		// Step 9: Undocumented opcode 0x04 (NOP zp)
		// Should not crash and should produce valid annotations
		// 0x04 = NOP zp, 3 cycles, 2 bytes
		// ------------------------------------------------------------------
		{
			C64U6502Decoder illegalDecoder;

			C64UDebugEntry e1 = C64UDebugEntry::Decode(MakeEntry(0x0800, 0x04, true, true));
			C64UDecoderAnnotation a1 = illegalDecoder.ProcessEntry(e1);
			if (a1.type != C64UDecoderAnnotation::OPCODE_FETCH)
			{
				TestCompleted(false, "Step9: Undocumented 0x04 cycle 1 should be OPCODE_FETCH");
				return;
			}
			if (a1.instructionPC != 0x0800)
			{
				TestCompleted(false, "Step9: Undocumented 0x04 PC should be 0x0800");
				return;
			}

			// Cycle 2: operand (zp address)
			C64UDebugEntry e2 = C64UDebugEntry::Decode(MakeEntry(0x0801, 0x42, true, true));
			C64UDecoderAnnotation a2 = illegalDecoder.ProcessEntry(e2);
			if (a2.type != C64UDecoderAnnotation::OPERAND)
			{
				TestCompleted(false, "Step9: Undocumented 0x04 cycle 2 should be OPERAND");
				return;
			}

			// Cycle 3: dead read from zp address (read, last cycle)
			C64UDebugEntry e3 = C64UDebugEntry::Decode(MakeEntry(0x0042, 0x00, true, true));
			C64UDecoderAnnotation a3 = illegalDecoder.ProcessEntry(e3);
			if (a3.type != C64UDecoderAnnotation::DATA_READ)
			{
				char msg[128];
				snprintf(msg, sizeof(msg), "Step9: Undocumented 0x04 cycle 3 should be DATA_READ, got %d", (int)a3.type);
				TestCompleted(false, msg);
				return;
			}

			if (!illegalDecoder.IsSynced())
			{
				TestCompleted(false, "Step9: Decoder should be synced after undocumented opcode");
				return;
			}

			// Verify decoder is ready for the next instruction
			C64UDebugEntry eNext = C64UDebugEntry::Decode(MakeEntry(0x0802, 0xEA, true, true));
			C64UDecoderAnnotation aNext = illegalDecoder.ProcessEntry(eNext);
			if (aNext.type != C64UDecoderAnnotation::OPCODE_FETCH)
			{
				TestCompleted(false, "Step9: After undocumented opcode, next should be OPCODE_FETCH");
				return;
			}
		}
		StepCompleted(9, true, "Undocumented opcode 0x04 (NOP zp): decoded without crash");

		TestCompleted(true, "All 6502 decoder tests passed (9 steps)");
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
