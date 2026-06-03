#ifndef _C64VIEWAVAILABILITY_H_
#define _C64VIEWAVAILABILITY_H_

#include "../../DebugInterface/C64/CC64BackendCapabilities.h"

enum EC64ViewAvailability
{
	C64_VIEW_AVAILABILITY_HIDDEN = 0,
	C64_VIEW_AVAILABILITY_READ_ONLY,
	C64_VIEW_AVAILABILITY_ENABLED,
};

class C64ViewAvailability
{
public:
	static inline EC64ViewAvailability GetMemoryViewAvailability(const CC64BackendCapabilities &capabilities)
	{
		if (!C64HasCapability(capabilities.mappedMemoryRead))
			return C64_VIEW_AVAILABILITY_HIDDEN;
		if (!C64HasCapability(capabilities.mappedMemoryWrite))
			return C64_VIEW_AVAILABILITY_READ_ONLY;
		return C64_VIEW_AVAILABILITY_ENABLED;
	}

	static inline EC64ViewAvailability GetDisassemblyViewAvailability(const CC64BackendCapabilities &capabilities)
	{
		if (!C64HasCapability(capabilities.mappedMemoryRead) && !C64HasCapability(capabilities.currentPcTracking))
			return C64_VIEW_AVAILABILITY_HIDDEN;
		if (!C64HasCapability(capabilities.stepInstruction) || !C64HasCapability(capabilities.breakpointsPC))
			return C64_VIEW_AVAILABILITY_READ_ONLY;
		return C64_VIEW_AVAILABILITY_ENABLED;
	}

	static inline bool ShouldShowTimeline(const CC64BackendCapabilities &capabilities)
	{
		return C64HasCapability(capabilities.timeline);
	}
};

#endif
