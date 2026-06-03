#ifndef _CDISASSEMBLY_INTERPRETER_H_
#define _CDISASSEMBLY_INTERPRETER_H_

#include "SYS_Defs.h"
#include "C64Opcodes.h"
#include <map>
#include <set>

class CDebugDataAdapter;

enum class RegCertainty : u8
{
	Certain,		// value from actual CPU registers at PC
	Simulated,		// value computed by interpreter forward trace
	Unknown			// value can't be determined (budget exhausted, I/O read, etc.)
};

struct RegState
{
	u8 value;
	RegCertainty certainty;

	RegState() : value(0), certainty(RegCertainty::Unknown) {}
	RegState(u8 v, RegCertainty c) : value(v), certainty(c) {}
};

enum AnnotationDisplayReg : u8
{
	DispRegNone = 0,
	DispRegA,
	DispRegX,
	DispRegY
};

struct AnnotationInfo
{
	u16 effectiveAddr;
	u8 value;
	RegCertainty certainty;
	bool hasAnnotation;
	bool isDirectAddress;  // true for ZP/ABS (address visible in instruction)
	bool isStore;          // true for STA/STX/STY (value is register being stored)

	// Register display for loads/stores (e.g., A=xx, X=03)
	AnnotationDisplayReg displayReg;
	u8 displayRegValue;
	RegCertainty displayRegCertainty;

	// Index register for indexed/indirect modes (e.g., Y=03, ...)
	bool hasIndexReg;
	AnnotationDisplayReg indexReg;
	u8 indexRegValue;
	RegCertainty indexRegCertainty;

	// Read-modify-write instruction (INC, DEC, ASL, LSR, ROL, ROR)
	bool isRMW;

	AnnotationInfo() : effectiveAddr(0), value(0), certainty(RegCertainty::Unknown),
		hasAnnotation(false), isDirectAddress(false), isStore(false),
		displayReg(DispRegNone), displayRegValue(0), displayRegCertainty(RegCertainty::Unknown),
		hasIndexReg(false), indexReg(DispRegNone), indexRegValue(0), indexRegCertainty(RegCertainty::Unknown),
		isRMW(false) {}
};

class CDisassemblyInterpreter
{
public:
	CDisassemblyInterpreter();
	~CDisassemblyInterpreter();

	// Initialize with CPU state and simulate forward
	void Init(u8 a, u8 x, u8 y, u8 sp, u8 flags, u16 pc);
	void SimulateRange(int startAddr, int endAddr, CDebugDataAdapter *dataAdapter, int memoryLength);

	// Get annotation for a specific address
	bool GetAnnotation(int addr, AnnotationInfo *info);
	int GetNumAnnotations();

	// Compute a fallback annotation for addresses beyond budget, using last register state
	bool GetFallbackAnnotation(int addr, AnnotationInfo *info);

	// Settings
	int instructionBudget;
	int jsrDepthLimit;

	// Debug
	u16 debugStartPC;
	int debugStopReason; // 0=budget, 1=visited, 2=simInstr returned 0
	u16 debugStopPC;
	int debugInstructionsSimulated;

	// I/O range for "unknown" detection (platform-specific, set before SimulateRange)
	u16 ioRangeStart;
	u16 ioRangeEnd;

private:
	// Register state
	RegState regA, regX, regY, regSP;
	u8 regFlags;
	bool flagsKnown;
	u16 simPC;

	// Budget tracking
	int remainingBudget;
	int loopDepth; // recursion guard for SimulateLoop
	int jsrDepth;  // recursion guard for SimulateJSR

	// Results
	std::map<int, AnnotationInfo> annotations;

	// Visited addresses (to detect backward branches to already-processed code)
	std::set<int> visitedAddresses;

	// Data access
	CDebugDataAdapter *dataAdapter;
	int memoryLength;

	// Read a byte from memory via adapter
	u8 ReadByte(int addr);
	u16 ReadWord(int addr);

	// Compute effective address for current instruction
	AnnotationInfo ComputeEffectiveAddress(u8 op, u8 lo, u8 hi, u16 instrAddr);

	// Simulate a single instruction, returns number of bytes consumed (0 = stop)
	int SimulateInstruction(u8 op, u8 lo, u8 hi, u16 instrAddr, int endAddr);

	// Update register after a load from memory
	void UpdateRegAfterLoad(RegState *reg, u16 addr);

	// Get the worst certainty between two states
	RegCertainty WorstCertainty(RegCertainty a, RegCertainty b);

	// Check if address is in I/O range
	bool IsIOAddress(u16 addr);

	// Simulate a branch/loop internally
	void SimulateLoop(u16 branchTarget, u16 returnAddr);

	// Simulate into a JSR
	void SimulateJSR(u16 target, u16 returnAddr, int endAddr);
};

#endif //_CDISASSEMBLY_INTERPRETER_H_
