#include "CDebugBreakpointData.h"
#include "CDebugBreakpointsData.h"

CDebugBreakpointData::CDebugBreakpointData(int addr,
									u32 dataAccess, DataBreakpointComparison comparison, int value)
: CDebugBreakpointAddr(NULL, addr)
{
	this->breakpointType = BREAKPOINT_TYPE_DATA;
	this->dataAccess = dataAccess;
	this->value = value;
	this->comparison = comparison;
	this->addrEnd = -1;
}

CDebugBreakpointData::CDebugBreakpointData(CDebugSymbols *debugSymbols, int addr,
									u32 dataAccess, DataBreakpointComparison comparison, int value)
: CDebugBreakpointAddr(debugSymbols, addr)
{
	this->breakpointType = BREAKPOINT_TYPE_DATA;
	this->dataAccess = dataAccess;
	this->value = value;
	this->comparison = comparison;
	this->addrEnd = -1;
}

CDebugBreakpointData::CDebugBreakpointData(CDebugSymbols *debugSymbols, int addr, int addrEnd,
									u32 dataAccess, DataBreakpointComparison comparison, int value)
: CDebugBreakpointAddr(debugSymbols, addr)
{
	this->breakpointType = BREAKPOINT_TYPE_DATA;
	this->dataAccess = dataAccess;
	this->value = value;
	this->comparison = comparison;
	this->addrEnd = addrEnd;
}

void CDebugBreakpointData::Serialize(Hjson::Value hjsonRoot)
{
	CDebugBreakpointAddr::Serialize(hjsonRoot);

	hjsonRoot["MemoryAccess"] = dataAccess;
	hjsonRoot["Value"] = value;
	hjsonRoot["Comparison"] = (int)comparison;
	hjsonRoot["AddrEnd"] = addrEnd;
}

void CDebugBreakpointData::Deserialize(Hjson::Value hjsonRoot)
{
	CDebugBreakpointAddr::Deserialize(hjsonRoot);

	dataAccess = hjsonRoot["MemoryAccess"];
	value = hjsonRoot["Value"];
	comparison = (DataBreakpointComparison) ((int)hjsonRoot["Comparison"]);

	// AddrEnd was added later — keep older HJSON loadable as single-address.
	Hjson::Value hjsonAddrEnd = hjsonRoot["AddrEnd"];
	if (hjsonAddrEnd != Hjson::Type::Undefined)
	{
		addrEnd = hjsonAddrEnd;
	}
	else
	{
		addrEnd = -1;
	}
}

void CDebugBreakpointData::GetDetailsJson(nlohmann::json &j)
{
	CDebugBreakpoint::GetDetailsJson(j);
	j["type"] = "data";
	if (IsRange())
	{
		j["addrEnd"] = addrEnd;
	}
//	j["triggerValue"] = value;
//	j["triggerComparison"] = CDebugBreakpointsData::DataBreakpointComparisonToStr(comparison);
}
