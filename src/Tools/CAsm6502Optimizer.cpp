#include "CAsm6502Optimizer.h"
#include "CDebuggerApi.h"
#include "SYS_Main.h"

CAsm6502Optimizer::CAsm6502Optimizer(CDebuggerApi *a, u16 startA, u16 endA)
	: api(a), startAddr(startA), endAddr(endA)
{
}

void CAsm6502Optimizer::RunAllPasses()
{
	PassRemoveTaxTxaRedundancy();
	PassReplaceLdaLdxWithLax();
}

u8 CAsm6502Optimizer::PeekByte(u16 addr) const
{
	if (api == NULL) return 0;
	return api->GetByteFromRam(addr);
}

void CAsm6502Optimizer::PokeByte(u16 addr, u8 v)
{
	if (api == NULL) return;
	api->SetByteToRam(addr, v);
}

// Conservative 6502 instruction-length table. Returns the BYTE size
// of the instruction starting with `opcode`. Indexed by opcode value.
//
// Length 1 = implied / accumulator (TAX, NOP, ASL A, ...)
// Length 2 = immediate / zero page / relative branches
// Length 3 = absolute / abs,X / abs,Y / indirect
//
// For unknown / NMOS-illegal opcodes we default to length 1 and the
// optimizer just steps past them — peephole patterns require exact
// length match anyway, so a wrong guess only means we miss an
// optimization, not corrupt code.
int CAsm6502Optimizer::InstructionLength(u8 op) const
{
	// Top 2 bits of opcode usually encode the addressing mode "block";
	// instead of decoding cleanly we use a small lookup — easier to
	// read and verify against a 6502 opcode chart.
	switch (op)
	{
		// 1-byte: implied, accumulator
		case 0x00: case 0x08: case 0x0A: case 0x18: case 0x1A:
		case 0x28: case 0x2A: case 0x38: case 0x3A:
		case 0x40: case 0x48: case 0x4A: case 0x58: case 0x5A:
		case 0x60: case 0x68: case 0x6A: case 0x78: case 0x7A:
		case 0x88: case 0x8A: case 0x98: case 0x9A:
		case 0xA8: case 0xAA: case 0xB8: case 0xBA:
		case 0xC8: case 0xCA: case 0xD8: case 0xDA:
		case 0xE8: case 0xEA: case 0xF8: case 0xFA:
			return 1;

		// 3-byte: absolute / abs,X / abs,Y / indirect
		case 0x0C: case 0x0D: case 0x0E: case 0x0F:
		case 0x19: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
		case 0x20: case 0x2C: case 0x2D: case 0x2E: case 0x2F:
		case 0x39: case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
		case 0x4C: case 0x4D: case 0x4E: case 0x4F:
		case 0x59: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
		case 0x6C: case 0x6D: case 0x6E: case 0x6F:
		case 0x79: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
		case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F:
		case 0x99: case 0x9B: case 0x9C: case 0x9D: case 0x9E: case 0x9F:
		case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF:
		case 0xB9: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
		case 0xCB: case 0xCC: case 0xCD: case 0xCE: case 0xCF:
		case 0xD9: case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF:
		case 0xEC: case 0xED: case 0xEE: case 0xEF:
		case 0xF9: case 0xFB: case 0xFC: case 0xFD: case 0xFE: case 0xFF:
			return 3;

		// Default 2-byte: immediate, zp, zp,X/Y, (zp,X), (zp),Y, branches.
		default:
			return 2;
	}
}

void CAsm6502Optimizer::PassRemoveTaxTxaRedundancy()
{
	// Detect a back-to-back register-transfer pair where the SECOND
	// instruction "undoes" the first's side effect on the source
	// register, making the second a true no-op:
	//
	//   TAX (AA): X := A      then TXA (8A): A := X = A    — A unchanged
	//   TXA (8A): A := X      then TAX (AA): X := A = X    — X unchanged
	//   TAY (A8): Y := A      then TYA (98): A := Y = A    — A unchanged
	//   TYA (98): A := Y      then TAY (A8): Y := A = Y    — Y unchanged
	//
	// In each case the SECOND instruction is dead — removing it leaves
	// the first's register update intact. We NOP only the second byte
	// (the redundant one) and keep the first. Removing both would
	// erase the first's side effect, which downstream code may rely
	// on (e.g. our body does TAX to save phase in X, then later TXA
	// to restore A from X — only the FIRST TXA right after TAX is
	// redundant; the LATER TXA depends on TAX having run).
	for (u16 a = startAddr; a + 1 < endAddr; a++)
	{
		const u8 op0 = PeekByte(a);
		const u8 op1 = PeekByte((u16)(a + 1));
		const bool taxTxa = (op0 == 0xAA && op1 == 0x8A) ||
		                    (op0 == 0x8A && op1 == 0xAA);
		const bool tayTya = (op0 == 0xA8 && op1 == 0x98) ||
		                    (op0 == 0x98 && op1 == 0xA8);
		if (taxTxa)
		{
			PokeByte((u16)(a + 1), 0xEA);  // NOP the redundant second op only
			numTaxTxaPairs++;
			a++;   // skip past the NOP we just placed
			continue;
		}
		if (tayTya)
		{
			PokeByte((u16)(a + 1), 0xEA);
			numTayTyaPairs++;
			a++;
			continue;
		}
	}
}

void CAsm6502Optimizer::PassReplaceLdaLdxWithLax()
{
	// Step instruction-by-instruction (not byte-by-byte) — peephole
	// requires aligned LDA + LDX with matching operand bytes. We use
	// InstructionLength() to advance past whatever's at `a`.
	u16 a = startAddr;
	while (a + 5 < endAddr)
	{
		const u8 op0 = PeekByte(a);
		const int len0 = InstructionLength(op0);
		const u8 op1 = PeekByte((u16)(a + len0));
		const int len1 = InstructionLength(op1);

		// LDA abs (AD) + LDX abs (AE), or swapped order.
		// Operand bytes at +1, +2 of each instruction.
		auto absMatch = [&](u8 firstOpcode, u8 secondOpcode, u8 lax) -> bool
		{
			if (op0 != firstOpcode || op1 != secondOpcode) return false;
			if (len0 != 3 || len1 != 3) return false;
			const u8 lo0 = PeekByte((u16)(a + 1));
			const u8 hi0 = PeekByte((u16)(a + 2));
			const u8 lo1 = PeekByte((u16)(a + 4));
			const u8 hi1 = PeekByte((u16)(a + 5));
			if (lo0 != lo1 || hi0 != hi1) return false;
			// Replace in place: LAX abs at a, NOP NOP NOP at a+3.
			PokeByte(a,             lax);
			PokeByte((u16)(a + 3),  0xEA);
			PokeByte((u16)(a + 4),  0xEA);
			PokeByte((u16)(a + 5),  0xEA);
			numLaxSubs++;
			return true;
		};

		auto zpMatch = [&](u8 firstOpcode, u8 secondOpcode, u8 lax) -> bool
		{
			if (op0 != firstOpcode || op1 != secondOpcode) return false;
			if (len0 != 2 || len1 != 2) return false;
			const u8 zp0 = PeekByte((u16)(a + 1));
			const u8 zp1 = PeekByte((u16)(a + 3));
			if (zp0 != zp1) return false;
			PokeByte(a,             lax);
			PokeByte((u16)(a + 2),  0xEA);
			PokeByte((u16)(a + 3),  0xEA);
			numLaxSubs++;
			return true;
		};

		// LDA abs (AD) + LDX abs (AE)  →  LAX abs (AF)
		if (absMatch(0xAD, 0xAE, 0xAF)) { a += 6; continue; }
		// LDX abs (AE) + LDA abs (AD)  →  LAX abs (AF), keep order
		if (absMatch(0xAE, 0xAD, 0xAF)) { a += 6; continue; }

		// LDA zp (A5) + LDX zp (A6)  →  LAX zp (A7)
		if (zpMatch(0xA5, 0xA6, 0xA7)) { a += 4; continue; }
		// LDX zp (A6) + LDA zp (A5)  →  LAX zp (A7)
		if (zpMatch(0xA6, 0xA5, 0xA7)) { a += 4; continue; }

		// LDA abs,Y (B9) + LDX abs,Y (BE)  →  LAX abs,Y (BF)
		if (absMatch(0xB9, 0xBE, 0xBF)) { a += 6; continue; }
		if (absMatch(0xBE, 0xB9, 0xBF)) { a += 6; continue; }

		// No pattern match — step past the first instruction.
		a = (u16)(a + (len0 > 0 ? len0 : 1));
	}
}
