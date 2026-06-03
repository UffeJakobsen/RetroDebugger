#include "C64U6502Decoder.h"
#include <cstring>

// Full 6502/6510 opcode table including undocumented opcodes.
// Cycle counts are the base counts (no page-boundary penalty).
// Sourced from http://www.oxyron.de/html/opcodes02.html and
// cross-referenced with src/Tools/C64Opcodes.h in this codebase.
const C64U6502Decoder::OpcodeInfo C64U6502Decoder::OPCODE_TABLE[256] =
{
	// 0x00-0x0F
	{ 7, 1, "BRK" },  // 00
	{ 6, 2, "ORA" },  // 01 (izx)
	{ 0, 1, "KIL" },  // 02 *illegal halt
	{ 8, 2, "SLO" },  // 03 *illegal (izx)
	{ 3, 2, "NOP" },  // 04 *illegal zp
	{ 3, 2, "ORA" },  // 05 zp
	{ 5, 2, "ASL" },  // 06 zp
	{ 5, 2, "SLO" },  // 07 *illegal zp
	{ 3, 1, "PHP" },  // 08
	{ 2, 2, "ORA" },  // 09 imm
	{ 2, 1, "ASL" },  // 0A acc
	{ 2, 2, "ANC" },  // 0B *illegal imm
	{ 4, 3, "NOP" },  // 0C *illegal abs
	{ 4, 3, "ORA" },  // 0D abs
	{ 6, 3, "ASL" },  // 0E abs
	{ 6, 3, "SLO" },  // 0F *illegal abs

	// 0x10-0x1F
	{ 2, 2, "BPL" },  // 10 rel
	{ 5, 2, "ORA" },  // 11 (izy)
	{ 0, 1, "KIL" },  // 12 *illegal halt
	{ 8, 2, "SLO" },  // 13 *illegal (izy)
	{ 4, 2, "NOP" },  // 14 *illegal zpx
	{ 4, 2, "ORA" },  // 15 zpx
	{ 6, 2, "ASL" },  // 16 zpx
	{ 6, 2, "SLO" },  // 17 *illegal zpx
	{ 2, 1, "CLC" },  // 18
	{ 4, 3, "ORA" },  // 19 aby
	{ 2, 1, "NOP" },  // 1A *illegal imp
	{ 7, 3, "SLO" },  // 1B *illegal aby
	{ 4, 3, "NOP" },  // 1C *illegal abx
	{ 4, 3, "ORA" },  // 1D abx
	{ 7, 3, "ASL" },  // 1E abx
	{ 7, 3, "SLO" },  // 1F *illegal abx

	// 0x20-0x2F
	{ 6, 3, "JSR" },  // 20 abs
	{ 6, 2, "AND" },  // 21 (izx)
	{ 0, 1, "KIL" },  // 22 *illegal halt
	{ 8, 2, "RLA" },  // 23 *illegal (izx)
	{ 3, 2, "BIT" },  // 24 zp
	{ 3, 2, "AND" },  // 25 zp
	{ 5, 2, "ROL" },  // 26 zp
	{ 5, 2, "RLA" },  // 27 *illegal zp
	{ 4, 1, "PLP" },  // 28
	{ 2, 2, "AND" },  // 29 imm
	{ 2, 1, "ROL" },  // 2A acc
	{ 2, 2, "ANC" },  // 2B *illegal imm
	{ 4, 3, "BIT" },  // 2C abs
	{ 4, 3, "AND" },  // 2D abs
	{ 6, 3, "ROL" },  // 2E abs
	{ 6, 3, "RLA" },  // 2F *illegal abs

	// 0x30-0x3F
	{ 2, 2, "BMI" },  // 30 rel
	{ 5, 2, "AND" },  // 31 (izy)
	{ 0, 1, "KIL" },  // 32 *illegal halt
	{ 8, 2, "RLA" },  // 33 *illegal (izy)
	{ 4, 2, "NOP" },  // 34 *illegal zpx
	{ 4, 2, "AND" },  // 35 zpx
	{ 6, 2, "ROL" },  // 36 zpx
	{ 6, 2, "RLA" },  // 37 *illegal zpx
	{ 2, 1, "SEC" },  // 38
	{ 4, 3, "AND" },  // 39 aby
	{ 2, 1, "NOP" },  // 3A *illegal imp
	{ 7, 3, "RLA" },  // 3B *illegal aby
	{ 4, 3, "NOP" },  // 3C *illegal abx
	{ 4, 3, "AND" },  // 3D abx
	{ 7, 3, "ROL" },  // 3E abx
	{ 7, 3, "RLA" },  // 3F *illegal abx

	// 0x40-0x4F
	{ 6, 1, "RTI" },  // 40
	{ 6, 2, "EOR" },  // 41 (izx)
	{ 0, 1, "KIL" },  // 42 *illegal halt
	{ 8, 2, "SRE" },  // 43 *illegal (izx)
	{ 3, 2, "NOP" },  // 44 *illegal zp
	{ 3, 2, "EOR" },  // 45 zp
	{ 5, 2, "LSR" },  // 46 zp
	{ 5, 2, "SRE" },  // 47 *illegal zp
	{ 3, 1, "PHA" },  // 48
	{ 2, 2, "EOR" },  // 49 imm
	{ 2, 1, "LSR" },  // 4A acc
	{ 2, 2, "ALR" },  // 4B *illegal imm
	{ 3, 3, "JMP" },  // 4C abs
	{ 4, 3, "EOR" },  // 4D abs
	{ 6, 3, "LSR" },  // 4E abs
	{ 6, 3, "SRE" },  // 4F *illegal abs

	// 0x50-0x5F
	{ 2, 2, "BVC" },  // 50 rel
	{ 5, 2, "EOR" },  // 51 (izy)
	{ 0, 1, "KIL" },  // 52 *illegal halt
	{ 8, 2, "SRE" },  // 53 *illegal (izy)
	{ 4, 2, "NOP" },  // 54 *illegal zpx
	{ 4, 2, "EOR" },  // 55 zpx
	{ 6, 2, "LSR" },  // 56 zpx
	{ 6, 2, "SRE" },  // 57 *illegal zpx
	{ 2, 1, "CLI" },  // 58
	{ 4, 3, "EOR" },  // 59 aby
	{ 2, 1, "NOP" },  // 5A *illegal imp
	{ 7, 3, "SRE" },  // 5B *illegal aby
	{ 4, 3, "NOP" },  // 5C *illegal abx
	{ 4, 3, "EOR" },  // 5D abx
	{ 7, 3, "LSR" },  // 5E abx
	{ 7, 3, "SRE" },  // 5F *illegal abx

	// 0x60-0x6F
	{ 6, 1, "RTS" },  // 60
	{ 6, 2, "ADC" },  // 61 (izx)
	{ 0, 1, "KIL" },  // 62 *illegal halt
	{ 8, 2, "RRA" },  // 63 *illegal (izx)
	{ 3, 2, "NOP" },  // 64 *illegal zp
	{ 3, 2, "ADC" },  // 65 zp
	{ 5, 2, "ROR" },  // 66 zp
	{ 5, 2, "RRA" },  // 67 *illegal zp
	{ 4, 1, "PLA" },  // 68
	{ 2, 2, "ADC" },  // 69 imm
	{ 2, 1, "ROR" },  // 6A acc
	{ 2, 2, "ARR" },  // 6B *illegal imm
	{ 5, 3, "JMP" },  // 6C ind
	{ 4, 3, "ADC" },  // 6D abs
	{ 6, 3, "ROR" },  // 6E abs
	{ 6, 3, "RRA" },  // 6F *illegal abs

	// 0x70-0x7F
	{ 2, 2, "BVS" },  // 70 rel
	{ 5, 2, "ADC" },  // 71 (izy)
	{ 0, 1, "KIL" },  // 72 *illegal halt
	{ 8, 2, "RRA" },  // 73 *illegal (izy)
	{ 4, 2, "NOP" },  // 74 *illegal zpx
	{ 4, 2, "ADC" },  // 75 zpx
	{ 6, 2, "ROR" },  // 76 zpx
	{ 6, 2, "RRA" },  // 77 *illegal zpx
	{ 2, 1, "SEI" },  // 78
	{ 4, 3, "ADC" },  // 79 aby
	{ 2, 1, "NOP" },  // 7A *illegal imp
	{ 7, 3, "RRA" },  // 7B *illegal aby
	{ 4, 3, "NOP" },  // 7C *illegal abx
	{ 4, 3, "ADC" },  // 7D abx
	{ 7, 3, "ROR" },  // 7E abx
	{ 7, 3, "RRA" },  // 7F *illegal abx

	// 0x80-0x8F
	{ 2, 2, "NOP" },  // 80 *illegal imm
	{ 6, 2, "STA" },  // 81 (izx)
	{ 2, 2, "NOP" },  // 82 *illegal imm
	{ 6, 2, "SAX" },  // 83 *illegal (izx)
	{ 3, 2, "STY" },  // 84 zp
	{ 3, 2, "STA" },  // 85 zp
	{ 3, 2, "STX" },  // 86 zp
	{ 3, 2, "SAX" },  // 87 *illegal zp
	{ 2, 1, "DEY" },  // 88
	{ 2, 2, "NOP" },  // 89 *illegal imm
	{ 2, 1, "TXA" },  // 8A
	{ 2, 2, "XAA" },  // 8B *illegal imm
	{ 4, 3, "STY" },  // 8C abs
	{ 4, 3, "STA" },  // 8D abs
	{ 4, 3, "STX" },  // 8E abs
	{ 4, 3, "SAX" },  // 8F *illegal abs

	// 0x90-0x9F
	{ 2, 2, "BCC" },  // 90 rel
	{ 6, 2, "STA" },  // 91 (izy)
	{ 0, 1, "KIL" },  // 92 *illegal halt
	{ 6, 2, "AHX" },  // 93 *illegal (izy)
	{ 4, 2, "STY" },  // 94 zpx
	{ 4, 2, "STA" },  // 95 zpx
	{ 4, 2, "STX" },  // 96 zpy
	{ 4, 2, "SAX" },  // 97 *illegal zpy
	{ 2, 1, "TYA" },  // 98
	{ 5, 3, "STA" },  // 99 aby
	{ 2, 1, "TXS" },  // 9A
	{ 5, 3, "TAS" },  // 9B *illegal aby
	{ 5, 3, "SHY" },  // 9C *illegal abx
	{ 5, 3, "STA" },  // 9D abx
	{ 5, 3, "SHX" },  // 9E *illegal aby
	{ 5, 3, "AHX" },  // 9F *illegal aby

	// 0xA0-0xAF
	{ 2, 2, "LDY" },  // A0 imm
	{ 6, 2, "LDA" },  // A1 (izx)
	{ 2, 2, "LDX" },  // A2 imm
	{ 6, 2, "LAX" },  // A3 *illegal (izx)
	{ 3, 2, "LDY" },  // A4 zp
	{ 3, 2, "LDA" },  // A5 zp
	{ 3, 2, "LDX" },  // A6 zp
	{ 3, 2, "LAX" },  // A7 *illegal zp
	{ 2, 1, "TAY" },  // A8
	{ 2, 2, "LDA" },  // A9 imm
	{ 2, 1, "TAX" },  // AA
	{ 2, 2, "LAX" },  // AB *illegal imm
	{ 4, 3, "LDY" },  // AC abs
	{ 4, 3, "LDA" },  // AD abs
	{ 4, 3, "LDX" },  // AE abs
	{ 4, 3, "LAX" },  // AF *illegal abs

	// 0xB0-0xBF
	{ 2, 2, "BCS" },  // B0 rel
	{ 5, 2, "LDA" },  // B1 (izy)
	{ 0, 1, "KIL" },  // B2 *illegal halt
	{ 5, 2, "LAX" },  // B3 *illegal (izy)
	{ 4, 2, "LDY" },  // B4 zpx
	{ 4, 2, "LDA" },  // B5 zpx
	{ 4, 2, "LDX" },  // B6 zpy
	{ 4, 2, "LAX" },  // B7 *illegal zpy
	{ 2, 1, "CLV" },  // B8
	{ 4, 3, "LDA" },  // B9 aby
	{ 2, 1, "TSX" },  // BA
	{ 4, 3, "LAS" },  // BB *illegal aby
	{ 4, 3, "LDY" },  // BC abx
	{ 4, 3, "LDA" },  // BD abx
	{ 4, 3, "LDX" },  // BE aby
	{ 4, 3, "LAX" },  // BF *illegal aby

	// 0xC0-0xCF
	{ 2, 2, "CPY" },  // C0 imm
	{ 6, 2, "CMP" },  // C1 (izx)
	{ 2, 2, "NOP" },  // C2 *illegal imm
	{ 8, 2, "DCP" },  // C3 *illegal (izx)
	{ 3, 2, "CPY" },  // C4 zp
	{ 3, 2, "CMP" },  // C5 zp
	{ 5, 2, "DEC" },  // C6 zp
	{ 5, 2, "DCP" },  // C7 *illegal zp
	{ 2, 1, "INY" },  // C8
	{ 2, 2, "CMP" },  // C9 imm
	{ 2, 1, "DEX" },  // CA
	{ 2, 2, "AXS" },  // CB *illegal imm
	{ 4, 3, "CPY" },  // CC abs
	{ 4, 3, "CMP" },  // CD abs
	{ 6, 3, "DEC" },  // CE abs
	{ 6, 3, "DCP" },  // CF *illegal abs

	// 0xD0-0xDF
	{ 2, 2, "BNE" },  // D0 rel
	{ 5, 2, "CMP" },  // D1 (izy)
	{ 0, 1, "KIL" },  // D2 *illegal halt
	{ 8, 2, "DCP" },  // D3 *illegal (izy)
	{ 4, 2, "NOP" },  // D4 *illegal zpx
	{ 4, 2, "CMP" },  // D5 zpx
	{ 6, 2, "DEC" },  // D6 zpx
	{ 6, 2, "DCP" },  // D7 *illegal zpx
	{ 2, 1, "CLD" },  // D8
	{ 4, 3, "CMP" },  // D9 aby
	{ 2, 1, "NOP" },  // DA *illegal imp
	{ 7, 3, "DCP" },  // DB *illegal aby
	{ 4, 3, "NOP" },  // DC *illegal abx
	{ 4, 3, "CMP" },  // DD abx
	{ 7, 3, "DEC" },  // DE abx
	{ 7, 3, "DCP" },  // DF *illegal abx

	// 0xE0-0xEF
	{ 2, 2, "CPX" },  // E0 imm
	{ 6, 2, "SBC" },  // E1 (izx)
	{ 2, 2, "NOP" },  // E2 *illegal imm
	{ 8, 2, "ISC" },  // E3 *illegal (izx)
	{ 3, 2, "CPX" },  // E4 zp
	{ 3, 2, "SBC" },  // E5 zp
	{ 5, 2, "INC" },  // E6 zp
	{ 5, 2, "ISC" },  // E7 *illegal zp
	{ 2, 1, "INX" },  // E8
	{ 2, 2, "SBC" },  // E9 imm
	{ 2, 1, "NOP" },  // EA
	{ 2, 2, "SBC" },  // EB *illegal imm
	{ 4, 3, "CPX" },  // EC abs
	{ 4, 3, "SBC" },  // ED abs
	{ 6, 3, "INC" },  // EE abs
	{ 6, 3, "ISC" },  // EF *illegal abs

	// 0xF0-0xFF
	{ 2, 2, "BEQ" },  // F0 rel
	{ 5, 2, "SBC" },  // F1 (izy)
	{ 0, 1, "KIL" },  // F2 *illegal halt
	{ 8, 2, "ISC" },  // F3 *illegal (izy)
	{ 4, 2, "NOP" },  // F4 *illegal zpx
	{ 4, 2, "SBC" },  // F5 zpx
	{ 6, 2, "INC" },  // F6 zpx
	{ 6, 2, "ISC" },  // F7 *illegal zpx
	{ 2, 1, "SED" },  // F8
	{ 4, 3, "SBC" },  // F9 aby
	{ 2, 1, "NOP" },  // FA *illegal imp
	{ 7, 3, "ISC" },  // FB *illegal aby
	{ 4, 3, "NOP" },  // FC *illegal abx
	{ 4, 3, "SBC" },  // FD abx
	{ 7, 3, "INC" },  // FE abx
	{ 7, 3, "ISC" },  // FF *illegal abx
};

C64U6502Decoder::C64U6502Decoder()
{
	Reset();
	traceMode = 0;
}

void C64U6502Decoder::Reset()
{
	state = WAITING_OPCODE;
	currentPC = 0;
	currentOpcode = 0;
	cycleInInstruction = 0;
	expectedCycles = 0;
	synced = false;
	regA = 0;
	regX = 0;
	regY = 0;
	regSP = 0;
	regP = FLAG_U | FLAG_I;  // unused always 1, I set after reset
	regsValid = false;
	instrDataCount = 0;
	lastReadData = 0;
	memset(instrData, 0, sizeof(instrData));
}

uint16_t C64U6502Decoder::GetCurrentPC() const
{
	return currentPC;
}

bool C64U6502Decoder::IsSynced() const
{
	return synced;
}

uint8_t C64U6502Decoder::GetRegA() const { return regA; }
uint8_t C64U6502Decoder::GetRegX() const { return regX; }
uint8_t C64U6502Decoder::GetRegY() const { return regY; }
uint8_t C64U6502Decoder::GetRegSP() const { return regSP; }
uint8_t C64U6502Decoder::GetRegP() const { return regP; }
bool C64U6502Decoder::AreRegsValid() const { return regsValid; }

void C64U6502Decoder::UpdateNZ(uint8_t value)
{
	regP = (regP & ~(FLAG_N | FLAG_Z)) | (value & FLAG_N) | (value == 0 ? FLAG_Z : 0);
}

void C64U6502Decoder::SetTraceMode(int mode)
{
	traceMode = mode;
}

bool C64U6502Decoder::ShouldProcessEntry(const C64UDebugEntry &entry) const
{
	switch (traceMode)
	{
		case 1:  // 6510 Only — always process
			return true;
		case 2:  // VIC Only — decoder disabled for VIC cycles
			return false;
		case 3:  // 6510 & VIC — process only CPU (PHI2=1) cycles
			return entry.GetPhi2();
		case 4:  // 1541 Only — always process
			return true;
		case 5:  // 6510 & 1541 — process only CPU (PHI2=1) cycles for the 6510 decoder
			return entry.GetPhi2();
		default: // 0 = auto — always process
			return true;
	}
}

const char *C64U6502Decoder::GetMnemonic(uint8_t opcode)
{
	return OPCODE_TABLE[opcode].mnemonic;
}

int C64U6502Decoder::GetCycles(uint8_t opcode)
{
	return OPCODE_TABLE[opcode].cycles;
}

bool C64U6502Decoder::IsVectorFetch(uint16_t address) const
{
	// 6502 interrupt/reset vectors at $FFFA-$FFFF
	// $FFFA/$FFFB = NMI, $FFFC/$FFFD = RESET, $FFFE/$FFFF = IRQ/BRK
	return (address >= 0xFFFA && address <= 0xFFFF);
}

C64UDecoderAnnotation C64U6502Decoder::ProcessEntry(const C64UDebugEntry &entry)
{
	C64UDecoderAnnotation ann;

	if (state == WAITING_OPCODE)
	{
		if (entry.rw)
		{
			// This is a read — treat as opcode fetch
			currentOpcode = entry.data;
			currentPC = entry.address;

			const OpcodeInfo &info = OPCODE_TABLE[currentOpcode];
			expectedCycles = info.cycles;

			// Handle KIL/JAM opcodes (0 cycles) — treat as 1-cycle and stay waiting
			if (expectedCycles == 0)
			{
				expectedCycles = 1;
			}

			cycleInInstruction = 1;
			synced = true;

			// Capture opcode fetch data
			instrDataCount = 1;
			instrData[0] = entry.data;
			lastReadData = 0;

			if (expectedCycles <= 1)
			{
				// Single-cycle: stay in WAITING_OPCODE for next entry
				ExecuteInstruction();
				state = WAITING_OPCODE;
			}
			else
			{
				state = IN_INSTRUCTION;
			}

			// Check for interrupt vector fetch
			if (IsVectorFetch(entry.address))
			{
				ann.type = C64UDecoderAnnotation::INTERRUPT;
				ann.instructionPC = currentPC;
				return ann;
			}

			ann.type = C64UDecoderAnnotation::OPCODE_FETCH;
			ann.instructionPC = currentPC;
			return ann;
		}
		else
		{
			// Write cycle while waiting for opcode — not synced yet
			ann.type = C64UDecoderAnnotation::UNKNOWN;
			ann.instructionPC = 0;
			return ann;
		}
	}

	// state == IN_INSTRUCTION
	cycleInInstruction++;

	// Capture cycle data
	if (instrDataCount < 8)
	{
		instrData[instrDataCount] = entry.data;
		instrDataCount++;
	}
	if (entry.rw)
	{
		lastReadData = entry.data;
	}

	if (cycleInInstruction >= expectedCycles)
	{
		// Last cycle of this instruction — execute register effects
		ExecuteInstruction();
		// Next entry will be a new opcode fetch
		state = WAITING_OPCODE;
	}

	// Classify this cycle based on position within the instruction
	const OpcodeInfo &info = OPCODE_TABLE[currentOpcode];

	if (cycleInInstruction <= info.bytes)
	{
		// Still within the instruction's encoded bytes (operand fetch)
		ann.type = C64UDecoderAnnotation::OPERAND;
	}
	else if (entry.rw)
	{
		// Read cycle after operand bytes — address calculation or data read
		if (cycleInInstruction >= expectedCycles)
		{
			ann.type = C64UDecoderAnnotation::DATA_READ;
		}
		else
		{
			ann.type = C64UDecoderAnnotation::ADDRESS_CALC;
		}
	}
	else
	{
		// Write cycle after operand bytes — data write
		ann.type = C64UDecoderAnnotation::DATA_WRITE;
	}

	ann.instructionPC = currentPC;
	return ann;
}

// ---------------------------------------------------------------------------
// ExecuteInstruction — simulate register effects of the completed instruction.
// lastReadData = data bus on the last read cycle (the operand for load/ALU).
// instrData[n] = data bus for cycle n (0 = opcode fetch).
// ---------------------------------------------------------------------------

void C64U6502Decoder::ExecuteInstruction()
{
	regsValid = true;
	uint8_t op = currentOpcode;
	uint8_t m = lastReadData;  // operand from bus

	switch (op)
	{
		// ====================== LDA ======================
		case 0xA9: case 0xA5: case 0xAD: case 0xB5: case 0xBD:
		case 0xB9: case 0xA1: case 0xB1:
			regA = m;
			UpdateNZ(regA);
			break;

		// ====================== LDX ======================
		case 0xA2: case 0xA6: case 0xAE: case 0xB6: case 0xBE:
			regX = m;
			UpdateNZ(regX);
			break;

		// ====================== LDY ======================
		case 0xA0: case 0xA4: case 0xAC: case 0xB4: case 0xBC:
			regY = m;
			UpdateNZ(regY);
			break;

		// ====================== STA, STX, STY ======================
		// Store instructions: no register changes
		case 0x85: case 0x8D: case 0x95: case 0x9D: case 0x99:
		case 0x81: case 0x91:  // STA
		case 0x86: case 0x8E: case 0x96:  // STX
		case 0x84: case 0x8C: case 0x94:  // STY
			break;

		// ====================== TAX, TAY, TXA, TYA, TSX, TXS ======================
		case 0xAA:  // TAX
			regX = regA;
			UpdateNZ(regX);
			break;
		case 0xA8:  // TAY
			regY = regA;
			UpdateNZ(regY);
			break;
		case 0x8A:  // TXA
			regA = regX;
			UpdateNZ(regA);
			break;
		case 0x98:  // TYA
			regA = regY;
			UpdateNZ(regA);
			break;
		case 0xBA:  // TSX
			regX = regSP;
			UpdateNZ(regX);
			break;
		case 0x9A:  // TXS
			regSP = regX;
			break;

		// ====================== PHA, PHP ======================
		case 0x48:  // PHA
			regSP--;
			break;
		case 0x08:  // PHP
			regSP--;
			break;

		// ====================== PLA ======================
		case 0x68:  // PLA
			regSP++;
			regA = m;  // lastReadData = value pulled from stack
			UpdateNZ(regA);
			break;

		// ====================== PLP ======================
		case 0x28:  // PLP
			regSP++;
			regP = (m | FLAG_U) & ~FLAG_B;  // B is never set in P register, unused always 1
			break;

		// ====================== AND ======================
		case 0x29: case 0x25: case 0x2D: case 0x35: case 0x3D:
		case 0x39: case 0x21: case 0x31:
			regA &= m;
			UpdateNZ(regA);
			break;

		// ====================== ORA ======================
		case 0x09: case 0x05: case 0x0D: case 0x15: case 0x1D:
		case 0x19: case 0x01: case 0x11:
			regA |= m;
			UpdateNZ(regA);
			break;

		// ====================== EOR ======================
		case 0x49: case 0x45: case 0x4D: case 0x55: case 0x5D:
		case 0x59: case 0x41: case 0x51:
			regA ^= m;
			UpdateNZ(regA);
			break;

		// ====================== ADC ======================
		case 0x69: case 0x65: case 0x6D: case 0x75: case 0x7D:
		case 0x79: case 0x61: case 0x71:
		{
			uint16_t sum = (uint16_t)regA + m + (regP & FLAG_C);
			uint8_t result = (uint8_t)sum;
			regP &= ~(FLAG_C | FLAG_V | FLAG_N | FLAG_Z);
			if (sum > 0xFF) regP |= FLAG_C;
			if (((regA ^ result) & (m ^ result) & 0x80)) regP |= FLAG_V;
			regA = result;
			UpdateNZ(regA);
			break;
		}

		// ====================== SBC ======================
		case 0xE9: case 0xE5: case 0xED: case 0xF5: case 0xFD:
		case 0xF9: case 0xE1: case 0xF1:
		case 0xEB:  // *SBC illegal (same as SBC imm)
		{
			uint8_t comp = m ^ 0xFF;
			uint16_t sum = (uint16_t)regA + comp + (regP & FLAG_C);
			uint8_t result = (uint8_t)sum;
			regP &= ~(FLAG_C | FLAG_V | FLAG_N | FLAG_Z);
			if (sum > 0xFF) regP |= FLAG_C;
			if (((regA ^ result) & (comp ^ result) & 0x80)) regP |= FLAG_V;
			regA = result;
			UpdateNZ(regA);
			break;
		}

		// ====================== CMP ======================
		case 0xC9: case 0xC5: case 0xCD: case 0xD5: case 0xDD:
		case 0xD9: case 0xC1: case 0xD1:
		{
			uint16_t r = (uint16_t)regA - m;
			regP &= ~(FLAG_C | FLAG_N | FLAG_Z);
			if (regA >= m) regP |= FLAG_C;
			UpdateNZ((uint8_t)r);
			break;
		}

		// ====================== CPX ======================
		case 0xE0: case 0xE4: case 0xEC:
		{
			uint16_t r = (uint16_t)regX - m;
			regP &= ~(FLAG_C | FLAG_N | FLAG_Z);
			if (regX >= m) regP |= FLAG_C;
			UpdateNZ((uint8_t)r);
			break;
		}

		// ====================== CPY ======================
		case 0xC0: case 0xC4: case 0xCC:
		{
			uint16_t r = (uint16_t)regY - m;
			regP &= ~(FLAG_C | FLAG_N | FLAG_Z);
			if (regY >= m) regP |= FLAG_C;
			UpdateNZ((uint8_t)r);
			break;
		}

		// ====================== BIT ======================
		case 0x24: case 0x2C:
			regP &= ~(FLAG_N | FLAG_V | FLAG_Z);
			regP |= (m & (FLAG_N | FLAG_V));
			if ((regA & m) == 0) regP |= FLAG_Z;
			break;

		// ====================== ASL A ======================
		case 0x0A:
		{
			uint8_t c = (regA >> 7) & 1;
			regA = regA << 1;
			regP = (regP & ~FLAG_C) | c;
			UpdateNZ(regA);
			break;
		}

		// ====================== LSR A ======================
		case 0x4A:
		{
			uint8_t c = regA & 1;
			regA = regA >> 1;
			regP = (regP & ~FLAG_C) | c;
			UpdateNZ(regA);
			break;
		}

		// ====================== ROL A ======================
		case 0x2A:
		{
			uint8_t oldC = regP & FLAG_C;
			regP = (regP & ~FLAG_C) | ((regA >> 7) & 1);
			regA = (regA << 1) | oldC;
			UpdateNZ(regA);
			break;
		}

		// ====================== ROR A ======================
		case 0x6A:
		{
			uint8_t oldC = regP & FLAG_C;
			regP = (regP & ~FLAG_C) | (regA & 1);
			regA = (regA >> 1) | (oldC << 7);
			UpdateNZ(regA);
			break;
		}

		// ====================== ASL memory ======================
		case 0x06: case 0x0E: case 0x16: case 0x1E:
		{
			uint8_t c = (m >> 7) & 1;
			uint8_t result = m << 1;
			regP = (regP & ~FLAG_C) | c;
			UpdateNZ(result);
			break;
		}

		// ====================== LSR memory ======================
		case 0x46: case 0x4E: case 0x56: case 0x5E:
		{
			uint8_t c = m & 1;
			uint8_t result = m >> 1;
			regP = (regP & ~FLAG_C) | c;
			UpdateNZ(result);
			break;
		}

		// ====================== ROL memory ======================
		case 0x26: case 0x2E: case 0x36: case 0x3E:
		{
			uint8_t oldC = regP & FLAG_C;
			regP = (regP & ~FLAG_C) | ((m >> 7) & 1);
			uint8_t result = (m << 1) | oldC;
			UpdateNZ(result);
			break;
		}

		// ====================== ROR memory ======================
		case 0x66: case 0x6E: case 0x76: case 0x7E:
		{
			uint8_t oldC = regP & FLAG_C;
			regP = (regP & ~FLAG_C) | (m & 1);
			uint8_t result = (m >> 1) | (oldC << 7);
			UpdateNZ(result);
			break;
		}

		// ====================== INC memory ======================
		case 0xE6: case 0xEE: case 0xF6: case 0xFE:
			UpdateNZ(m + 1);
			break;

		// ====================== DEC memory ======================
		case 0xC6: case 0xCE: case 0xD6: case 0xDE:
			UpdateNZ(m - 1);
			break;

		// ====================== INX, DEX, INY, DEY ======================
		case 0xE8:  // INX
			regX++;
			UpdateNZ(regX);
			break;
		case 0xCA:  // DEX
			regX--;
			UpdateNZ(regX);
			break;
		case 0xC8:  // INY
			regY++;
			UpdateNZ(regY);
			break;
		case 0x88:  // DEY
			regY--;
			UpdateNZ(regY);
			break;

		// ====================== Flag instructions ======================
		case 0x18: regP &= ~FLAG_C; break;  // CLC
		case 0x38: regP |= FLAG_C; break;   // SEC
		case 0x58: regP &= ~FLAG_I; break;  // CLI
		case 0x78: regP |= FLAG_I; break;   // SEI
		case 0xB8: regP &= ~FLAG_V; break;  // CLV
		case 0xD8: regP &= ~FLAG_D; break;  // CLD
		case 0xF8: regP |= FLAG_D; break;   // SED

		// ====================== Branch instructions ======================
		// No register changes (only PC affected, tracked by opcode fetch)
		case 0x10: case 0x30: case 0x50: case 0x70:  // BPL BMI BVC BVS
		case 0x90: case 0xB0: case 0xD0: case 0xF0:  // BCC BCS BNE BEQ
			break;

		// ====================== JMP ======================
		case 0x4C: case 0x6C:  // JMP abs, JMP (ind)
			break;

		// ====================== JSR ======================
		case 0x20:  // JSR: pushes return address, SP -= 2
			regSP -= 2;
			break;

		// ====================== RTS ======================
		case 0x60:  // RTS: pops return address, SP += 2
			regSP += 2;
			break;

		// ====================== BRK ======================
		case 0x00:  // BRK: push PC+2, push P, set I
			regSP -= 3;
			regP |= FLAG_I;
			break;

		// ====================== RTI ======================
		case 0x40:  // RTI: pull P, pull PC. SP += 3
		{
			regSP += 3;
			// instrData: [0]=opcode, [1]=dummy, [2]=dummy stack, [3]=P, [4]=PCL, [5]=PCH
			if (instrDataCount >= 4)
				regP = (instrData[3] | FLAG_U) & ~FLAG_B;
			break;
		}

		// ====================== NOP ======================
		case 0xEA:  // NOP
		case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA:  // *NOP impl
		case 0x04: case 0x14: case 0x34: case 0x44: case 0x54: case 0x64:  // *NOP zp/zpx
		case 0x74: case 0xD4: case 0xF4:
		case 0x0C: case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:  // *NOP abs/abx
		case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2:  // *NOP imm
			break;

		// ====================== Common illegal opcodes ======================

		// LAX: LDA + LDX (A = X = operand)
		case 0xA3: case 0xA7: case 0xAF: case 0xB3: case 0xB7: case 0xBF:
		case 0xAB:  // *LAX imm (unstable but common)
			regA = m;
			regX = m;
			UpdateNZ(regA);
			break;

		// SAX: store A & X (no register changes)
		case 0x83: case 0x87: case 0x8F: case 0x97:
			break;

		// DCP: DEC + CMP
		case 0xC3: case 0xC7: case 0xCF: case 0xD3: case 0xD7: case 0xDB: case 0xDF:
		{
			uint8_t val = m - 1;
			uint16_t r = (uint16_t)regA - val;
			regP &= ~(FLAG_C | FLAG_N | FLAG_Z);
			if (regA >= val) regP |= FLAG_C;
			UpdateNZ((uint8_t)r);
			break;
		}

		// ISC (ISB): INC + SBC
		case 0xE3: case 0xE7: case 0xEF: case 0xF3: case 0xF7: case 0xFB: case 0xFF:
		{
			uint8_t val = m + 1;
			uint8_t comp = val ^ 0xFF;
			uint16_t sum = (uint16_t)regA + comp + (regP & FLAG_C);
			uint8_t result = (uint8_t)sum;
			regP &= ~(FLAG_C | FLAG_V | FLAG_N | FLAG_Z);
			if (sum > 0xFF) regP |= FLAG_C;
			if (((regA ^ result) & (comp ^ result) & 0x80)) regP |= FLAG_V;
			regA = result;
			UpdateNZ(regA);
			break;
		}

		// SLO: ASL + ORA
		case 0x03: case 0x07: case 0x0F: case 0x13: case 0x17: case 0x1B: case 0x1F:
		{
			uint8_t c = (m >> 7) & 1;
			uint8_t val = m << 1;
			regP = (regP & ~FLAG_C) | c;
			regA |= val;
			UpdateNZ(regA);
			break;
		}

		// RLA: ROL + AND
		case 0x23: case 0x27: case 0x2F: case 0x33: case 0x37: case 0x3B: case 0x3F:
		{
			uint8_t oldC = regP & FLAG_C;
			regP = (regP & ~FLAG_C) | ((m >> 7) & 1);
			uint8_t val = (m << 1) | oldC;
			regA &= val;
			UpdateNZ(regA);
			break;
		}

		// SRE: LSR + EOR
		case 0x43: case 0x47: case 0x4F: case 0x53: case 0x57: case 0x5B: case 0x5F:
		{
			uint8_t c = m & 1;
			uint8_t val = m >> 1;
			regP = (regP & ~FLAG_C) | c;
			regA ^= val;
			UpdateNZ(regA);
			break;
		}

		// RRA: ROR + ADC
		case 0x63: case 0x67: case 0x6F: case 0x73: case 0x77: case 0x7B: case 0x7F:
		{
			uint8_t oldC = regP & FLAG_C;
			regP = (regP & ~FLAG_C) | (m & 1);
			uint8_t val = (m >> 1) | (oldC << 7);
			// ADC with val
			uint16_t sum = (uint16_t)regA + val + (regP & FLAG_C);
			uint8_t result = (uint8_t)sum;
			regP &= ~(FLAG_C | FLAG_V | FLAG_N | FLAG_Z);
			if (sum > 0xFF) regP |= FLAG_C;
			if (((regA ^ result) & (val ^ result) & 0x80)) regP |= FLAG_V;
			regA = result;
			UpdateNZ(regA);
			break;
		}

		// ANC: AND + set C from bit 7
		case 0x0B: case 0x2B:
			regA &= m;
			UpdateNZ(regA);
			regP = (regP & ~FLAG_C) | ((regA >> 7) & 1);
			break;

		// ALR: AND + LSR
		case 0x4B:
			regA &= m;
			regP = (regP & ~FLAG_C) | (regA & 1);
			regA >>= 1;
			UpdateNZ(regA);
			break;

		// ARR: AND + ROR (with special flag handling)
		case 0x6B:
		{
			regA = ((regA & m) >> 1) | ((regP & FLAG_C) << 7);
			UpdateNZ(regA);
			regP &= ~(FLAG_C | FLAG_V);
			regP |= ((regA >> 6) & 1);  // C = bit 6
			regP |= (((regA >> 6) ^ (regA >> 5)) & 1) << 6;  // V = bit 6 XOR bit 5
			break;
		}

		// AXS (SBX): X = (A & X) - operand
		case 0xCB:
		{
			uint16_t r = (uint16_t)(regA & regX) - m;
			regX = (uint8_t)r;
			regP &= ~(FLAG_C | FLAG_N | FLAG_Z);
			if (r < 0x100) regP |= FLAG_C;
			UpdateNZ(regX);
			break;
		}

		// LAS: A = X = SP = (SP & operand)
		case 0xBB:
			regA = regSP & m;
			regX = regA;
			regSP = regA;
			UpdateNZ(regA);
			break;

		// KIL/JAM — CPU halted, no register change
		case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
		case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:
			break;

		// Remaining unstable illegals (AHX, SHX, SHY, TAS, XAA) — don't change tracked regs
		default:
			break;
	}

	// Ensure unused flag is always set
	regP |= FLAG_U;
}
