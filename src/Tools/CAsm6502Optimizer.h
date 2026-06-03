#ifndef _CASM6502OPTIMIZER_H_
#define _CASM6502OPTIMIZER_H_

#include "SYS_Defs.h"

class CDebuggerApi;

// Post-emit peephole optimizer for 6502 speedcode in C64 RAM.
//
// The emit pipelines (CFadeEffect::Emit6502Effect, the various
// Remapper emit functions, ...) generate code in straightforward,
// pattern-driven shapes. That makes the emit code easy to read and
// extend, but it also leaves redundant byte-sequences sitting in the
// final image — TAX immediately followed by TXA, LDA + LDX of the
// same address that could be a single LAX, etc. The emit code stays
// as-is; this tool runs as a separate pass and rewrites the emitted
// bytes in place.
//
// All passes preserve byte-level addressing — patterns are NOPped
// out instead of removed, so SMC operand offsets stay valid.
//
// Usage:
//   CAsm6502Optimizer opt(api, codeStartAddr, codeEndAddr);
//   opt.RunAllPasses();
//   LOGM("optimizer: %d TAX/TXA, %d LAX subs", opt.numTaxTxaPairs, opt.numLaxSubs);
//
// or one pass at a time:
//   opt.PassRemoveTaxTxaRedundancy();
//   opt.PassReplaceLdaLdxWithLax();
class CAsm6502Optimizer
{
public:
	CAsm6502Optimizer(CDebuggerApi *api, u16 startAddr, u16 endAddr);

	// Convenience: run every pass below in order.
	void RunAllPasses();

	// Remove pairs of register-transfer instructions whose net effect
	// on the visible registers is zero. Detects:
	//   TAX (AA) immediately followed by TXA (8A)  → NOP NOP
	//   TXA (8A) immediately followed by TAX (AA)  → NOP NOP
	//   TAY (A8) immediately followed by TYA (98)  → NOP NOP
	//   TYA (98) immediately followed by TAY (A8)  → NOP NOP
	// Each removed pair leaves two $EA bytes so subsequent SMC patch
	// sites at later addresses remain valid.
	void PassRemoveTaxTxaRedundancy();

	// Replace LDA <addr> + LDX <addr> reading from the same operand
	// with the NMOS-illegal LAX <addr> single op. Both LDA and LDX of
	// the same memory location load the same byte into A then X
	// respectively; LAX does both in one instruction. The freed bytes
	// are NOPped so addressing doesn't shift.
	//
	// Patterns handled:
	//   LDA abs   (AD lo hi) + LDX abs   (AE lo hi)  → LAX abs   (AF lo hi) + EA EA EA
	//   LDA zp    (A5 zp)    + LDX zp    (A6 zp)     → LAX zp    (A7 zp)    + EA EA
	//   LDA abs,Y (B9 lo hi) + LDX abs,Y (BE lo hi)  → LAX abs,Y (BF lo hi) + EA EA EA
	// (and their LDX-then-LDA inverses).
	//
	// NOTE: LAX is an undocumented NMOS opcode. Some 6510 derivatives
	// (like the 65C02) don't decode it, but the C64's stock 6510 does.
	// The fade-speedup PRG runs on stock C64 hardware, so this is OK.
	void PassReplaceLdaLdxWithLax();

	// Stats — caller can log these after RunAllPasses().
	int numTaxTxaPairs   = 0;
	int numTayTyaPairs   = 0;
	int numLaxSubs       = 0;

private:
	CDebuggerApi *api;
	u16 startAddr;
	u16 endAddr;

	// Disassembly helper — returns the instruction length at addr
	// (1, 2, or 3 bytes). Conservative: anything that looks like a
	// branch / illegal opcode falls back to 1. Used to step through
	// the emitted code byte-by-byte without misaligning patterns.
	int InstructionLength(u8 opcode) const;

	u8 PeekByte(u16 addr) const;
	void PokeByte(u16 addr, u8 v);
};

#endif
