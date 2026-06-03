#ifndef _C64U6502DECODER_H_
#define _C64U6502DECODER_H_

#include "C64UDebugEntry.h"
#include "C64UDecoderAnnotation.h"
#include <cstdint>

class C64U6502Decoder
{
public:
	C64U6502Decoder();

	// Process one trace entry, return annotation
	C64UDecoderAnnotation ProcessEntry(const C64UDebugEntry &entry);

	// Reset state (on packet loss or mode switch)
	void Reset();

	uint16_t GetCurrentPC() const;
	bool IsSynced() const;

	// CPU register access (traced from bus data)
	uint8_t GetRegA() const;
	uint8_t GetRegX() const;
	uint8_t GetRegY() const;
	uint8_t GetRegSP() const;
	uint8_t GetRegP() const;
	bool AreRegsValid() const;

	// Mode filtering
	void SetTraceMode(int traceMode);
	bool ShouldProcessEntry(const C64UDebugEntry &entry) const;

	// Static opcode info accessors
	static const char *GetMnemonic(uint8_t opcode);
	static int GetCycles(uint8_t opcode);

	// Flag bit definitions
	static const uint8_t FLAG_C = 0x01;
	static const uint8_t FLAG_Z = 0x02;
	static const uint8_t FLAG_I = 0x04;
	static const uint8_t FLAG_D = 0x08;
	static const uint8_t FLAG_B = 0x10;
	static const uint8_t FLAG_U = 0x20;  // unused, always 1
	static const uint8_t FLAG_V = 0x40;
	static const uint8_t FLAG_N = 0x80;

private:
	enum State { WAITING_OPCODE, IN_INSTRUCTION };
	State state;
	uint16_t currentPC;
	uint8_t currentOpcode;
	int cycleInInstruction;
	int expectedCycles;
	bool synced;
	int traceMode;  // 0=auto, 1=6510, 2=VIC, 3=6510+VIC, 4=1541, 5=6510+1541

	// CPU register state (reconstructed from bus trace)
	uint8_t regA, regX, regY, regSP, regP;
	bool regsValid;

	// Per-cycle data capture for current instruction
	uint8_t instrData[8];   // data bus value per cycle (index 0 = opcode fetch)
	int instrDataCount;
	uint8_t lastReadData;   // last data bus value read during instruction (after opcode fetch)

	void ExecuteInstruction();
	void UpdateNZ(uint8_t value);
	bool IsVectorFetch(uint16_t address) const;

	struct OpcodeInfo
	{
		int cycles;
		int bytes;
		const char *mnemonic;
	};
	static const OpcodeInfo OPCODE_TABLE[256];
};

#endif
