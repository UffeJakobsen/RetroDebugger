#ifndef _CDebugBreakpointData_h_
#define _CDebugBreakpointData_h_

#include "CDebugBreakpointAddr.h"

// TODO: refactor this to 2 breakpoints (make list?)
class CDebugBreakpointData : public CDebugBreakpointAddr
{
public:
	CDebugBreakpointData(int addr,
					  u32 dataAccess, DataBreakpointComparison comparison, int value);
	CDebugBreakpointData(CDebugSymbols *symbols, int addr,
					  u32 dataAccess, DataBreakpointComparison comparison, int value);
	CDebugBreakpointData(CDebugSymbols *symbols, int addr, int addrEnd,
					  u32 dataAccess, DataBreakpointComparison comparison, int value);

	u32 dataAccess;
	int value;
	DataBreakpointComparison comparison;

	// Range support: when addrEnd >= addr, the breakpoint matches every
	// address in [addr, addrEnd]. When addrEnd < 0 (default), only `addr`
	// matches — preserves prior single-address behavior and HJSON files
	// that pre-date the range field.
	int addrEnd;

	bool IsRange() const { return addrEnd >= 0 && addrEnd > addr; }

	virtual void Serialize(Hjson::Value hjsonRoot);
	virtual void Deserialize(Hjson::Value hjsonRoot);

	virtual void GetDetailsJson(nlohmann::json &j);
};

#endif
//_CDebugBreakpoint_h_
