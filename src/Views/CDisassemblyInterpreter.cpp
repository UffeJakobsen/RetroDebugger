#include "CDisassemblyInterpreter.h"
#include "CDebugDataAdapter.h"
#include <cstring>

CDisassemblyInterpreter::CDisassemblyInterpreter()
{
	instructionBudget = 128;
	ioRangeStart = 0xD000;
	ioRangeEnd = 0xDFFF;
	dataAdapter = NULL;
	memoryLength = 0x10000;
}

CDisassemblyInterpreter::~CDisassemblyInterpreter()
{
}

void CDisassemblyInterpreter::Init(u8 a, u8 x, u8 y, u8 sp, u8 flags, u16 pc)
{
	regA = RegState(a, RegCertainty::Certain);
	regX = RegState(x, RegCertainty::Certain);
	regY = RegState(y, RegCertainty::Certain);
	regSP = RegState(sp, RegCertainty::Certain);
	regFlags = flags;
	flagsKnown = true;
	simPC = pc;

	annotations.clear();
	visitedAddresses.clear();
}

void CDisassemblyInterpreter::SimulateRange(int startAddr, int endAddr, CDebugDataAdapter *adapter, int memLen)
{
	this->dataAdapter = adapter;
	this->memoryLength = memLen;
	remainingBudget = instructionBudget;
	loopDepth = 0;
	jsrDepth = 0;
	debugInstructionsSimulated = 0;
	debugStopReason = 0;
	debugStopPC = simPC;

	while (remainingBudget > 0)
	{
		u16 instrAddr = simPC;

		// Detect if we've been here before (infinite loop protection)
		if (visitedAddresses.count(instrAddr))
		{
			debugStopReason = 1;
			debugStopPC = instrAddr;
			break;
		}
		visitedAddresses.insert(instrAddr);

		u8 op = ReadByte(instrAddr);
		u8 lo = ReadByte((instrAddr + 1) % memoryLength);
		u8 hi = ReadByte((instrAddr + 2) % memoryLength);

		// Compute and store annotation for this address
		AnnotationInfo annotation = ComputeEffectiveAddress(op, lo, hi, instrAddr);
		if (annotation.hasAnnotation)
		{
			annotations[instrAddr] = annotation;
		}

		// Simulate the instruction (updates registers, PC, budget)
		int consumed = SimulateInstruction(op, lo, hi, instrAddr, endAddr);
		debugInstructionsSimulated++;
		if (consumed == 0)
		{
			debugStopReason = 2;
			debugStopPC = instrAddr;
			break;
		}
	}
	if (remainingBudget <= 0)
	{
		debugStopReason = 0;
		debugStopPC = simPC;
	}
}

bool CDisassemblyInterpreter::GetAnnotation(int addr, AnnotationInfo *info)
{
	auto it = annotations.find(addr);
	if (it != annotations.end())
	{
		*info = it->second;
		return true;
	}
	return false;
}

int CDisassemblyInterpreter::GetNumAnnotations()
{
	return (int)annotations.size();
}

bool CDisassemblyInterpreter::GetFallbackAnnotation(int addr, AnnotationInfo *info)
{
	if (!dataAdapter)
		return false;

	u8 op = ReadByte(addr);
	u8 lo = ReadByte((addr + 1) % memoryLength);
	u8 hi = ReadByte((addr + 2) % memoryLength);

	*info = ComputeEffectiveAddress(op, lo, hi, (u16)addr);
	if (!info->hasAnnotation)
		return false;

	// Force everything to Unknown since this is beyond budget
	info->certainty = RegCertainty::Unknown;
	info->displayRegCertainty = RegCertainty::Unknown;
	info->indexRegCertainty = RegCertainty::Unknown;
	return true;
}

u8 CDisassemblyInterpreter::ReadByte(int addr)
{
	uint8 value;
	dataAdapter->AdapterReadByte(addr % memoryLength, &value);
	return value;
}

u16 CDisassemblyInterpreter::ReadWord(int addr)
{
	u8 lo = ReadByte(addr);
	u8 hi = ReadByte((addr + 1) % memoryLength);
	return (u16)(hi << 8) | lo;
}

bool CDisassemblyInterpreter::IsIOAddress(u16 addr)
{
	return addr >= ioRangeStart && addr <= ioRangeEnd;
}

RegCertainty CDisassemblyInterpreter::WorstCertainty(RegCertainty a, RegCertainty b)
{
	if (a == RegCertainty::Unknown || b == RegCertainty::Unknown)
		return RegCertainty::Unknown;
	if (a == RegCertainty::Simulated || b == RegCertainty::Simulated)
		return RegCertainty::Simulated;
	return RegCertainty::Certain;
}

void CDisassemblyInterpreter::UpdateRegAfterLoad(RegState *reg, u16 addr)
{
	if (IsIOAddress(addr))
	{
		reg->value = ReadByte(addr);
		reg->certainty = RegCertainty::Unknown;
	}
	else
	{
		reg->value = ReadByte(addr);
		if (reg->certainty != RegCertainty::Unknown)
			reg->certainty = RegCertainty::Simulated;
	}
}

static AnnotationDisplayReg GetLoadReg(const char *opName)
{
	if (strcmp(opName, "LDA") == 0) return DispRegA;
	if (strcmp(opName, "LDX") == 0) return DispRegX;
	if (strcmp(opName, "LDY") == 0) return DispRegY;
	return DispRegNone;
}

static AnnotationDisplayReg GetStoreReg(const char *opName)
{
	if (strcmp(opName, "STA") == 0) return DispRegA;
	if (strcmp(opName, "STX") == 0) return DispRegX;
	if (strcmp(opName, "STY") == 0) return DispRegY;
	return DispRegNone;
}

static bool IsRMWOp(const char *opName)
{
	return (strcmp(opName, "INC") == 0 || strcmp(opName, "DEC") == 0 ||
			strcmp(opName, "ASL") == 0 || strcmp(opName, "LSR") == 0 ||
			strcmp(opName, "ROL") == 0 || strcmp(opName, "ROR") == 0);
}

AnnotationInfo CDisassemblyInterpreter::ComputeEffectiveAddress(u8 op, u8 lo, u8 hi, u16 instrAddr)
{
	AnnotationInfo info;
	info.hasAnnotation = false;

	OpcodeAddressingMode mode = opcodes[op].addressingMode;
	const char *opName = opcodes[op].name;

	// Skip instructions where the address is already fully visible and no memory/register value is relevant
	// (but keep JMP indirect — its resolved target IS useful to show)
	if (strcmp(opName, "JSR") == 0 ||
		(strcmp(opName, "JMP") == 0 && mode != ADDR_IND))
		return info;

	// Classify instruction
	AnnotationDisplayReg loadReg = GetLoadReg(opName);
	AnnotationDisplayReg storeReg = GetStoreReg(opName);
	bool isStore = (strcmp(opName, "STA") == 0 || strcmp(opName, "STX") == 0 ||
					strcmp(opName, "STY") == 0 || strcmp(opName, "SAX") == 0);
	bool isLoad = (loadReg != DispRegNone);
	bool isRMW = IsRMWOp(opName);
	info.isStore = isStore;
	info.isRMW = isRMW;

	// Helper: get register state by display reg enum
	auto getRegState = [&](AnnotationDisplayReg r) -> RegState {
		switch (r) {
			case DispRegA: return regA;
			case DispRegX: return regX;
			case DispRegY: return regY;
			default: return RegState();
		}
	};

	// Set annotation values based on instruction type
	auto setAnnotationValues = [&](u16 ea, RegCertainty extraCertainty = RegCertainty::Certain) {
		if (isStore && storeReg != DispRegNone)
		{
			info.displayReg = storeReg;
			RegState rs = getRegState(storeReg);
			info.displayRegValue = rs.value;
			info.displayRegCertainty = rs.certainty;
			info.value = rs.value;
			info.certainty = WorstCertainty(extraCertainty, rs.certainty);
		}
		else if (isLoad)
		{
			info.displayReg = loadReg;
			info.displayRegValue = ReadByte(ea);
			info.displayRegCertainty = RegCertainty::Simulated;
			info.value = info.displayRegValue;
			info.certainty = WorstCertainty(extraCertainty, RegCertainty::Simulated);
		}
		else
		{
			// RMW, compare, bit-test, ALU ops: show addr=value
			info.value = ReadByte(ea);
			info.certainty = WorstCertainty(extraCertainty, RegCertainty::Simulated);
		}
	};

	switch (mode)
	{
		case ADDR_ZP:
		{
			u16 ea = lo;
			info.effectiveAddr = ea;
			info.isDirectAddress = true;
			setAnnotationValues(ea);
			info.hasAnnotation = true;
			break;
		}
		case ADDR_ABS:
		{
			u16 ea = ((u16)hi << 8) | lo;
			info.effectiveAddr = ea;
			info.isDirectAddress = true;
			setAnnotationValues(ea);
			info.hasAnnotation = true;
			break;
		}
		case ADDR_ZPX:
		{
			u16 ea = ((u16)lo + regX.value) & 0xFF;
			info.effectiveAddr = ea;
			info.hasIndexReg = true;
			info.indexReg = DispRegX;
			info.indexRegValue = regX.value;
			info.indexRegCertainty = regX.certainty;
			setAnnotationValues(ea, regX.certainty);
			info.hasAnnotation = true;
			break;
		}
		case ADDR_ZPY:
		{
			u16 ea = ((u16)lo + regY.value) & 0xFF;
			info.effectiveAddr = ea;
			info.hasIndexReg = true;
			info.indexReg = DispRegY;
			info.indexRegValue = regY.value;
			info.indexRegCertainty = regY.certainty;
			setAnnotationValues(ea, regY.certainty);
			info.hasAnnotation = true;
			break;
		}
		case ADDR_ABX:
		{
			u16 base = ((u16)hi << 8) | lo;
			u16 ea = base + regX.value;
			info.effectiveAddr = ea;
			info.hasIndexReg = true;
			info.indexReg = DispRegX;
			info.indexRegValue = regX.value;
			info.indexRegCertainty = regX.certainty;
			setAnnotationValues(ea, regX.certainty);
			info.hasAnnotation = true;
			break;
		}
		case ADDR_ABY:
		{
			u16 base = ((u16)hi << 8) | lo;
			u16 ea = base + regY.value;
			info.effectiveAddr = ea;
			info.hasIndexReg = true;
			info.indexReg = DispRegY;
			info.indexRegValue = regY.value;
			info.indexRegCertainty = regY.certainty;
			setAnnotationValues(ea, regY.certainty);
			info.hasAnnotation = true;
			break;
		}
		case ADDR_IZX:
		{
			u8 zpAddr = (lo + regX.value) & 0xFF;
			u16 ptr = (u16)ReadByte(zpAddr) | ((u16)ReadByte((zpAddr + 1) & 0xFF) << 8);
			info.effectiveAddr = ptr;
			info.hasIndexReg = true;
			info.indexReg = DispRegX;
			info.indexRegValue = regX.value;
			info.indexRegCertainty = regX.certainty;
			setAnnotationValues(ptr, regX.certainty);
			info.hasAnnotation = true;
			break;
		}
		case ADDR_IZY:
		{
			u16 ptr = (u16)ReadByte(lo) | ((u16)ReadByte((lo + 1) & 0xFF) << 8);
			u16 ea = ptr + regY.value;
			info.effectiveAddr = ea;
			info.hasIndexReg = true;
			info.indexReg = DispRegY;
			info.indexRegValue = regY.value;
			info.indexRegCertainty = regY.certainty;
			setAnnotationValues(ea, regY.certainty);
			info.hasAnnotation = true;
			break;
		}
		case ADDR_IND:
		{
			// JMP ($xxxx) — indirect jump
			u16 ptrAddr = ((u16)hi << 8) | lo;
			// 6502 bug: wraps within page
			u16 ptrAddrHi = (ptrAddr & 0xFF00) | ((ptrAddr + 1) & 0x00FF);
			u16 ea = (u16)ReadByte(ptrAddr) | ((u16)ReadByte(ptrAddrHi) << 8);
			info.effectiveAddr = ea;
			info.value = ReadByte(ea);
			info.certainty = RegCertainty::Certain;
			info.hasAnnotation = true;
			break;
		}
		default:
			break;
	}

	return info;
}

// 6502 flag bits
#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_N 0x80

static void UpdateNZ(u8 *flags, u8 value)
{
	*flags = (*flags & ~(FLAG_Z | FLAG_N));
	if (value == 0) *flags |= FLAG_Z;
	if (value & 0x80) *flags |= FLAG_N;
}

void CDisassemblyInterpreter::SimulateLoop(u16 branchTarget, u16 returnAddr)
{
	// Prevent infinite recursion (nested loops)
	if (loopDepth >= 2)
	{
		regA.certainty = RegCertainty::Unknown;
		regX.certainty = RegCertainty::Unknown;
		regY.certainty = RegCertainty::Unknown;
		regSP.certainty = RegCertainty::Unknown;
		flagsKnown = false;
		simPC = returnAddr;
		return;
	}
	loopDepth++;

	// Simulate the loop with a separate small budget to determine post-loop register state
	// This does NOT consume the main budget — loops are treated as a fixed cost
	int savedMainBudget = remainingBudget;
	int loopBudget = 64; // small separate budget for loop simulation
	remainingBudget = loopBudget;

	u16 savedPC = simPC;
	simPC = branchTarget;

	// Use a local visited set to detect loop completion
	std::set<int> loopVisited;
	bool loopResolved = false;

	while (remainingBudget > 0)
	{
		u16 loopAddr = simPC;

		if (loopVisited.count(loopAddr))
		{
			// We've gone around the loop again — can't determine final state
			break;
		}
		loopVisited.insert(loopAddr);

		u8 op = ReadByte(loopAddr);
		u8 lo = ReadByte((loopAddr + 1) % memoryLength);
		u8 hi = ReadByte((loopAddr + 2) % memoryLength);

		// In loop simulation, don't enter JSRs — just mark registers unknown and skip
		if (op == 0x20) // JSR
		{
			regA.certainty = RegCertainty::Unknown;
			regX.certainty = RegCertainty::Unknown;
			regY.certainty = RegCertainty::Unknown;
			flagsKnown = false;
			regSP.value -= 2;
			regSP.certainty = RegCertainty::Simulated;
			simPC = (loopAddr + 3) % memoryLength;
			remainingBudget--;
			continue;
		}

		int consumed = SimulateInstruction(op, lo, hi, loopAddr, 0xFFFF);
		if (consumed == 0)
			break;

		// If we've reached the return address, the loop has exited
		if (simPC == returnAddr)
		{
			loopResolved = true;
			break;
		}
	}

	if (!loopResolved)
	{
		// Could not determine post-loop state
		regA.certainty = RegCertainty::Unknown;
		regX.certainty = RegCertainty::Unknown;
		regY.certainty = RegCertainty::Unknown;
		regSP.certainty = RegCertainty::Unknown;
		flagsKnown = false;
		simPC = returnAddr;
	}

	// Restore main budget (loop was a fixed cost, deduct only what was actually used)
	int loopCost = loopBudget - remainingBudget;
	if (loopCost > 8) loopCost = 8; // cap loop cost to preserve main budget
	remainingBudget = savedMainBudget - loopCost;
	loopDepth--;
}

void CDisassemblyInterpreter::SimulateJSR(u16 target, u16 returnAddr, int endAddr)
{
	if (jsrDepth >= jsrDepthLimit)
	{
		// Too deeply nested — mark registers unknown and bail
		regA.certainty = RegCertainty::Unknown;
		regX.certainty = RegCertainty::Unknown;
		regY.certainty = RegCertainty::Unknown;
		regSP.certainty = RegCertainty::Unknown;
		flagsKnown = false;
		return;
	}
	jsrDepth++;

	// JSR gets its own budget — doesn't consume the main simulation budget
	int savedMainBudget = remainingBudget;
	int jsrBudget = 64; // separate budget for subroutine
	remainingBudget = jsrBudget;

	simPC = target;

	std::set<int> jsrVisited;
	bool jsrResolved = false;

	while (remainingBudget > 0)
	{
		u16 instrAddr = simPC;

		if (jsrVisited.count(instrAddr))
			break;
		jsrVisited.insert(instrAddr);

		u8 op = ReadByte(instrAddr);
		u8 lo = ReadByte((instrAddr + 1) % memoryLength);
		u8 hi = ReadByte((instrAddr + 2) % memoryLength);

		// RTS — return from subroutine
		if (op == 0x60) // RTS
		{
			remainingBudget--;
			simPC = returnAddr;
			jsrResolved = true;
			break;
		}

		// RTI — also a return
		if (op == 0x40) // RTI
		{
			remainingBudget--;
			flagsKnown = false;
			simPC = returnAddr;
			jsrResolved = true;
			break;
		}

		int consumed = SimulateInstruction(op, lo, hi, instrAddr, 0xFFFF);
		if (consumed == 0)
			break;
	}

	if (!jsrResolved)
	{
		// Budget exhausted inside JSR — registers become unknown
		regA.certainty = RegCertainty::Unknown;
		regX.certainty = RegCertainty::Unknown;
		regY.certainty = RegCertainty::Unknown;
		regSP.certainty = RegCertainty::Unknown;
		flagsKnown = false;
		simPC = returnAddr;
	}

	// Restore main budget with small fixed cost for the JSR
	int jsrCost = jsrBudget - remainingBudget;
	if (jsrCost > 4) jsrCost = 4; // cap JSR cost to preserve main budget
	remainingBudget = savedMainBudget - jsrCost;
	jsrDepth--;
}

int CDisassemblyInterpreter::SimulateInstruction(u8 op, u8 lo, u8 hi, u16 instrAddr, int endAddr)
{
	remainingBudget--;
	if (remainingBudget <= 0)
	{
		regA.certainty = RegCertainty::Unknown;
		regX.certainty = RegCertainty::Unknown;
		regY.certainty = RegCertainty::Unknown;
		regSP.certainty = RegCertainty::Unknown;
		flagsKnown = false;
		return 0;
	}

	int length = opcodes[op].addressingLength;
	OpcodeAddressingMode mode = opcodes[op].addressingMode;
	const char *name = opcodes[op].name;

	// Helper: compute effective address for memory access instructions
	auto getEA = [&]() -> u16 {
		switch (mode)
		{
			case ADDR_ZP:  return lo;
			case ADDR_ZPX: return ((u16)lo + regX.value) & 0xFF;
			case ADDR_ZPY: return ((u16)lo + regY.value) & 0xFF;
			case ADDR_ABS: return ((u16)hi << 8) | lo;
			case ADDR_ABX: return (((u16)hi << 8) | lo) + regX.value;
			case ADDR_ABY: return (((u16)hi << 8) | lo) + regY.value;
			case ADDR_IZX: {
				u8 zpAddr = (lo + regX.value) & 0xFF;
				return (u16)ReadByte(zpAddr) | ((u16)ReadByte((zpAddr + 1) & 0xFF) << 8);
			}
			case ADDR_IZY: {
				u16 ptr = (u16)ReadByte(lo) | ((u16)ReadByte((lo + 1) & 0xFF) << 8);
				return ptr + regY.value;
			}
			case ADDR_IMM: return instrAddr + 1;
			default: return 0;
		}
	};

	// Determine the value for IMM mode
	auto getImmOrMem = [&]() -> u8 {
		if (mode == ADDR_IMM)
			return lo;
		return ReadByte(getEA());
	};

	// Advance PC by default
	simPC = (instrAddr + length) % memoryLength;

	// Match by opcode name for simplicity (covers all addressing modes)
	// Load instructions
	if (strcmp(name, "LDA") == 0)
	{
		if (mode == ADDR_IMM)
		{
			regA.value = lo;
			if (regA.certainty != RegCertainty::Unknown)
				regA.certainty = RegCertainty::Simulated;
		}
		else
		{
			u16 ea = getEA();
			UpdateRegAfterLoad(&regA, ea);
		}
		if (flagsKnown) UpdateNZ(&regFlags, regA.value);
	}
	else if (strcmp(name, "LDX") == 0)
	{
		if (mode == ADDR_IMM)
		{
			regX.value = lo;
			if (regX.certainty != RegCertainty::Unknown)
				regX.certainty = RegCertainty::Simulated;
		}
		else
		{
			u16 ea = getEA();
			UpdateRegAfterLoad(&regX, ea);
		}
		if (flagsKnown) UpdateNZ(&regFlags, regX.value);
	}
	else if (strcmp(name, "LDY") == 0)
	{
		if (mode == ADDR_IMM)
		{
			regY.value = lo;
			if (regY.certainty != RegCertainty::Unknown)
				regY.certainty = RegCertainty::Simulated;
		}
		else
		{
			u16 ea = getEA();
			UpdateRegAfterLoad(&regY, ea);
		}
		if (flagsKnown) UpdateNZ(&regFlags, regY.value);
	}
	// Store instructions — don't change registers
	else if (strcmp(name, "STA") == 0 || strcmp(name, "STX") == 0 || strcmp(name, "STY") == 0)
	{
		// No register change
	}
	// Transfer instructions
	else if (strcmp(name, "TAX") == 0)
	{
		regX = regA;
		if (flagsKnown) UpdateNZ(&regFlags, regX.value);
	}
	else if (strcmp(name, "TAY") == 0)
	{
		regY = regA;
		if (flagsKnown) UpdateNZ(&regFlags, regY.value);
	}
	else if (strcmp(name, "TXA") == 0)
	{
		regA = regX;
		if (flagsKnown) UpdateNZ(&regFlags, regA.value);
	}
	else if (strcmp(name, "TYA") == 0)
	{
		regA = regY;
		if (flagsKnown) UpdateNZ(&regFlags, regA.value);
	}
	else if (strcmp(name, "TSX") == 0)
	{
		regX = regSP;
		if (flagsKnown) UpdateNZ(&regFlags, regX.value);
	}
	else if (strcmp(name, "TXS") == 0)
	{
		regSP = regX;
	}
	// Increment/Decrement registers
	else if (strcmp(name, "INX") == 0)
	{
		regX.value++;
		if (regX.certainty == RegCertainty::Certain)
			regX.certainty = RegCertainty::Simulated;
		if (flagsKnown) UpdateNZ(&regFlags, regX.value);
	}
	else if (strcmp(name, "INY") == 0)
	{
		regY.value++;
		if (regY.certainty == RegCertainty::Certain)
			regY.certainty = RegCertainty::Simulated;
		if (flagsKnown) UpdateNZ(&regFlags, regY.value);
	}
	else if (strcmp(name, "DEX") == 0)
	{
		regX.value--;
		if (regX.certainty == RegCertainty::Certain)
			regX.certainty = RegCertainty::Simulated;
		if (flagsKnown) UpdateNZ(&regFlags, regX.value);
	}
	else if (strcmp(name, "DEY") == 0)
	{
		regY.value--;
		if (regY.certainty == RegCertainty::Certain)
			regY.certainty = RegCertainty::Simulated;
		if (flagsKnown) UpdateNZ(&regFlags, regY.value);
	}
	// INC/DEC memory — don't change registers but update flags
	else if (strcmp(name, "INC") == 0 || strcmp(name, "DEC") == 0)
	{
		if (flagsKnown)
		{
			u16 ea = getEA();
			u8 val = ReadByte(ea);
			if (strcmp(name, "INC") == 0) val++; else val--;
			UpdateNZ(&regFlags, val);
		}
	}
	// ALU operations on A
	else if (strcmp(name, "ADC") == 0)
	{
		if (flagsKnown && regA.certainty != RegCertainty::Unknown)
		{
			u8 operand = getImmOrMem();
			u16 result = (u16)regA.value + operand + (regFlags & FLAG_C ? 1 : 0);
			regA.value = (u8)result;
			regA.certainty = RegCertainty::Simulated;
			regFlags = (regFlags & ~FLAG_C) | (result > 0xFF ? FLAG_C : 0);
			UpdateNZ(&regFlags, regA.value);
		}
		else
		{
			regA.certainty = RegCertainty::Unknown;
			flagsKnown = false;
		}
	}
	else if (strcmp(name, "SBC") == 0)
	{
		if (flagsKnown && regA.certainty != RegCertainty::Unknown)
		{
			u8 operand = getImmOrMem();
			u16 result = (u16)regA.value - operand - (regFlags & FLAG_C ? 0 : 1);
			regA.value = (u8)result;
			regA.certainty = RegCertainty::Simulated;
			regFlags = (regFlags & ~FLAG_C) | (result < 0x100 ? FLAG_C : 0);
			UpdateNZ(&regFlags, regA.value);
		}
		else
		{
			regA.certainty = RegCertainty::Unknown;
			flagsKnown = false;
		}
	}
	else if (strcmp(name, "AND") == 0)
	{
		u8 operand = getImmOrMem();
		regA.value &= operand;
		if (regA.certainty == RegCertainty::Certain)
			regA.certainty = RegCertainty::Simulated;
		if (flagsKnown) UpdateNZ(&regFlags, regA.value);
	}
	else if (strcmp(name, "ORA") == 0)
	{
		u8 operand = getImmOrMem();
		regA.value |= operand;
		if (regA.certainty == RegCertainty::Certain)
			regA.certainty = RegCertainty::Simulated;
		if (flagsKnown) UpdateNZ(&regFlags, regA.value);
	}
	else if (strcmp(name, "EOR") == 0)
	{
		u8 operand = getImmOrMem();
		regA.value ^= operand;
		if (regA.certainty == RegCertainty::Certain)
			regA.certainty = RegCertainty::Simulated;
		if (flagsKnown) UpdateNZ(&regFlags, regA.value);
	}
	// Shift/rotate A
	else if (strcmp(name, "ASL") == 0 && mode == ADDR_IMP)
	{
		if (flagsKnown)
		{
			regFlags = (regFlags & ~FLAG_C) | ((regA.value & 0x80) ? FLAG_C : 0);
			regA.value <<= 1;
			regA.certainty = RegCertainty::Simulated;
			UpdateNZ(&regFlags, regA.value);
		}
		else
		{
			regA.certainty = RegCertainty::Unknown;
		}
	}
	else if (strcmp(name, "LSR") == 0 && mode == ADDR_IMP)
	{
		if (flagsKnown)
		{
			regFlags = (regFlags & ~FLAG_C) | (regA.value & 0x01 ? FLAG_C : 0);
			regA.value >>= 1;
			regA.certainty = RegCertainty::Simulated;
			UpdateNZ(&regFlags, regA.value);
		}
		else
		{
			regA.certainty = RegCertainty::Unknown;
		}
	}
	else if (strcmp(name, "ROL") == 0 && mode == ADDR_IMP)
	{
		if (flagsKnown)
		{
			u8 carry = regFlags & FLAG_C ? 1 : 0;
			regFlags = (regFlags & ~FLAG_C) | ((regA.value & 0x80) ? FLAG_C : 0);
			regA.value = (regA.value << 1) | carry;
			regA.certainty = RegCertainty::Simulated;
			UpdateNZ(&regFlags, regA.value);
		}
		else
		{
			regA.certainty = RegCertainty::Unknown;
		}
	}
	else if (strcmp(name, "ROR") == 0 && mode == ADDR_IMP)
	{
		if (flagsKnown)
		{
			u8 carry = regFlags & FLAG_C ? 0x80 : 0;
			regFlags = (regFlags & ~FLAG_C) | (regA.value & 0x01 ? FLAG_C : 0);
			regA.value = (regA.value >> 1) | carry;
			regA.certainty = RegCertainty::Simulated;
			UpdateNZ(&regFlags, regA.value);
		}
		else
		{
			regA.certainty = RegCertainty::Unknown;
		}
	}
	// Shift/rotate on memory (don't change regs, just flags)
	else if ((strcmp(name, "ASL") == 0 || strcmp(name, "LSR") == 0 ||
			  strcmp(name, "ROL") == 0 || strcmp(name, "ROR") == 0) && mode != ADDR_IMP)
	{
		// Flags may change but we don't track memory writes
		flagsKnown = false;
	}
	// Compare instructions
	else if (strcmp(name, "CMP") == 0)
	{
		if (flagsKnown && regA.certainty != RegCertainty::Unknown)
		{
			u8 operand = getImmOrMem();
			u8 result = regA.value - operand;
			regFlags = (regFlags & ~FLAG_C) | (regA.value >= operand ? FLAG_C : 0);
			UpdateNZ(&regFlags, result);
		}
		else
		{
			flagsKnown = false;
		}
	}
	else if (strcmp(name, "CPX") == 0)
	{
		if (flagsKnown && regX.certainty != RegCertainty::Unknown)
		{
			u8 operand = getImmOrMem();
			u8 result = regX.value - operand;
			regFlags = (regFlags & ~FLAG_C) | (regX.value >= operand ? FLAG_C : 0);
			UpdateNZ(&regFlags, result);
		}
		else
		{
			flagsKnown = false;
		}
	}
	else if (strcmp(name, "CPY") == 0)
	{
		if (flagsKnown && regY.certainty != RegCertainty::Unknown)
		{
			u8 operand = getImmOrMem();
			u8 result = regY.value - operand;
			regFlags = (regFlags & ~FLAG_C) | (regY.value >= operand ? FLAG_C : 0);
			UpdateNZ(&regFlags, result);
		}
		else
		{
			flagsKnown = false;
		}
	}
	// BIT test
	else if (strcmp(name, "BIT") == 0)
	{
		if (flagsKnown)
		{
			u8 operand = ReadByte(getEA());
			regFlags = (regFlags & ~(FLAG_Z | FLAG_N | 0x40));
			if ((regA.value & operand) == 0) regFlags |= FLAG_Z;
			regFlags |= (operand & 0xC0); // N and V from operand
		}
		else
		{
			flagsKnown = false;
		}
	}
	// Stack operations
	else if (strcmp(name, "PHA") == 0)
	{
		regSP.value--;
		if (regSP.certainty == RegCertainty::Certain)
			regSP.certainty = RegCertainty::Simulated;
	}
	else if (strcmp(name, "PLA") == 0)
	{
		regSP.value++;
		if (regSP.certainty == RegCertainty::Certain)
			regSP.certainty = RegCertainty::Simulated;
		u16 stackAddr = 0x0100 + regSP.value;
		UpdateRegAfterLoad(&regA, stackAddr);
		if (flagsKnown) UpdateNZ(&regFlags, regA.value);
	}
	else if (strcmp(name, "PHP") == 0)
	{
		regSP.value--;
		if (regSP.certainty == RegCertainty::Certain)
			regSP.certainty = RegCertainty::Simulated;
	}
	else if (strcmp(name, "PLP") == 0)
	{
		regSP.value++;
		if (regSP.certainty == RegCertainty::Certain)
			regSP.certainty = RegCertainty::Simulated;
		regFlags = ReadByte(0x0100 + regSP.value);
		flagsKnown = true;
	}
	// Flag instructions
	else if (strcmp(name, "CLC") == 0) { regFlags &= ~FLAG_C; }
	else if (strcmp(name, "SEC") == 0) { regFlags |= FLAG_C; }
	else if (strcmp(name, "CLD") == 0) { regFlags &= ~0x08; }
	else if (strcmp(name, "SED") == 0) { regFlags |= 0x08; }
	else if (strcmp(name, "CLI") == 0) { regFlags &= ~0x04; }
	else if (strcmp(name, "SEI") == 0) { regFlags |= 0x04; }
	else if (strcmp(name, "CLV") == 0) { regFlags &= ~0x40; }
	// NOP
	else if (strcmp(name, "NOP") == 0)
	{
		// No effect
	}
	// Branch instructions
	else if (strcmp(name, "BPL") == 0 || strcmp(name, "BMI") == 0 ||
			 strcmp(name, "BVC") == 0 || strcmp(name, "BVS") == 0 ||
			 strcmp(name, "BCC") == 0 || strcmp(name, "BCS") == 0 ||
			 strcmp(name, "BNE") == 0 || strcmp(name, "BEQ") == 0)
	{
		u16 branchTarget = (instrAddr + 2 + (int8)lo) & 0xFFFF;
		bool taken = false;
		bool canDetermine = flagsKnown;

		if (canDetermine)
		{
			if (strcmp(name, "BPL") == 0) taken = !(regFlags & FLAG_N);
			else if (strcmp(name, "BMI") == 0) taken = (regFlags & FLAG_N) != 0;
			else if (strcmp(name, "BVC") == 0) taken = !(regFlags & 0x40);
			else if (strcmp(name, "BVS") == 0) taken = (regFlags & 0x40) != 0;
			else if (strcmp(name, "BCC") == 0) taken = !(regFlags & FLAG_C);
			else if (strcmp(name, "BCS") == 0) taken = (regFlags & FLAG_C) != 0;
			else if (strcmp(name, "BNE") == 0) taken = !(regFlags & FLAG_Z);
			else if (strcmp(name, "BEQ") == 0) taken = (regFlags & FLAG_Z) != 0;
		}

		if (!canDetermine)
		{
			// Can't determine branch — registers become unknown
			regA.certainty = RegCertainty::Unknown;
			regX.certainty = RegCertainty::Unknown;
			regY.certainty = RegCertainty::Unknown;
			flagsKnown = false;
			// Continue linearly (fall through)
		}
		else if (taken)
		{
			if (branchTarget <= instrAddr)
			{
				// Backward branch — simulate loop internally
				SimulateLoop(branchTarget, simPC);
			}
			else
			{
				// Forward branch — follow it
				simPC = branchTarget;
			}
		}
		// else: not taken, continue linearly (simPC already advanced)
	}
	// JMP absolute
	else if (op == 0x4C) // JMP abs
	{
		u16 target = ((u16)hi << 8) | lo;
		if (target <= instrAddr)
		{
			// Backward jump — stop simulation
			return 0;
		}
		simPC = target;
	}
	// JMP indirect
	else if (op == 0x6C) // JMP (ind)
	{
		// Can't follow indirect jumps reliably
		return 0;
	}
	// JSR
	else if (op == 0x20) // JSR abs
	{
		u16 target = ((u16)hi << 8) | lo;
		u16 returnAddr = simPC; // already advanced past JSR
		regSP.value -= 2;
		if (regSP.certainty == RegCertainty::Certain)
			regSP.certainty = RegCertainty::Simulated;
		SimulateJSR(target, returnAddr, endAddr);
	}
	// RTS — return from subroutine (stop if not inside SimulateJSR)
	else if (op == 0x60) // RTS
	{
		return 0;
	}
	// RTI
	else if (op == 0x40) // RTI
	{
		return 0;
	}
	// BRK
	else if (op == 0x00) // BRK
	{
		return 0;
	}
	// KIL / JAM — stop
	else if (strcmp(name, "KIL") == 0)
	{
		return 0;
	}
	// Illegal opcodes that modify registers — mark as unknown
	else if (strcmp(name, "LAX") == 0)
	{
		u16 ea = getEA();
		UpdateRegAfterLoad(&regA, ea);
		regX = regA;
		if (flagsKnown) UpdateNZ(&regFlags, regA.value);
	}
	else if (strcmp(name, "SAX") == 0)
	{
		// STA & STX — no register change
	}
	else if (strcmp(name, "DCP") == 0)
	{
		// DEC + CMP — flags change
		flagsKnown = false;
	}
	else if (strcmp(name, "ISC") == 0 || strcmp(name, "ISB") == 0)
	{
		// INC + SBC
		regA.certainty = RegCertainty::Unknown;
		flagsKnown = false;
	}
	else if (strcmp(name, "SLO") == 0)
	{
		// ASL + ORA
		regA.certainty = RegCertainty::Unknown;
		flagsKnown = false;
	}
	else if (strcmp(name, "RLA") == 0)
	{
		// ROL + AND
		regA.certainty = RegCertainty::Unknown;
		flagsKnown = false;
	}
	else if (strcmp(name, "SRE") == 0)
	{
		// LSR + EOR
		regA.certainty = RegCertainty::Unknown;
		flagsKnown = false;
	}
	else if (strcmp(name, "RRA") == 0)
	{
		// ROR + ADC
		regA.certainty = RegCertainty::Unknown;
		flagsKnown = false;
	}
	else if (strcmp(name, "ANC") == 0 || strcmp(name, "ALR") == 0 || strcmp(name, "ARR") == 0 ||
			 strcmp(name, "AXS") == 0 || strcmp(name, "SBX") == 0)
	{
		regA.certainty = RegCertainty::Unknown;
		flagsKnown = false;
	}
	else
	{
		// Unknown instruction — mark all as unknown
		regA.certainty = RegCertainty::Unknown;
		regX.certainty = RegCertainty::Unknown;
		regY.certainty = RegCertainty::Unknown;
		flagsKnown = false;
	}

	return length;
}
