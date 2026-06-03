#ifndef _CC64BACKENDCAPABILITIES_H_
#define _CC64BACKENDCAPABILITIES_H_

#include "SYS_Types.h"

enum EC64CapabilityLevel
{
	C64CAP_NONE = 0,
	C64CAP_DIRECT,
	C64CAP_DERIVED,
	C64CAP_OBSERVED,
	C64CAP_HELPER,
};

struct CC64BackendCapabilities
{
	bool isDefaultC64Backend;
	bool supportsScreenMode;
	bool supportsTraceMode;
	bool screenAndTraceAreMutuallyExclusive;

	EC64CapabilityLevel screenStream;
	EC64CapabilityLevel mappedMemoryRead;
	EC64CapabilityLevel mappedMemoryWrite;
	EC64CapabilityLevel directRamRead;
	EC64CapabilityLevel directRamWrite;
	EC64CapabilityLevel cpuRegistersRead;
	EC64CapabilityLevel cpuRegistersWrite;
	EC64CapabilityLevel currentPcTracking;
	EC64CapabilityLevel breakpointsPC;
	EC64CapabilityLevel breakpointsRead;
	EC64CapabilityLevel breakpointsWrite;
	EC64CapabilityLevel stepInstruction;
	EC64CapabilityLevel stepCycle;
	EC64CapabilityLevel nativeMonitor;
	EC64CapabilityLevel snapshots;
	EC64CapabilityLevel timeline;
	EC64CapabilityLevel keyboardTextInput;
	EC64CapabilityLevel keyboardMatrixInput;
	EC64CapabilityLevel joystickInput;
	EC64CapabilityLevel mouseInput;

	CC64BackendCapabilities()
	: isDefaultC64Backend(false),
	  supportsScreenMode(false),
	  supportsTraceMode(false),
	  screenAndTraceAreMutuallyExclusive(false),
	  screenStream(C64CAP_NONE),
	  mappedMemoryRead(C64CAP_NONE),
	  mappedMemoryWrite(C64CAP_NONE),
	  directRamRead(C64CAP_NONE),
	  directRamWrite(C64CAP_NONE),
	  cpuRegistersRead(C64CAP_NONE),
	  cpuRegistersWrite(C64CAP_NONE),
	  currentPcTracking(C64CAP_NONE),
	  breakpointsPC(C64CAP_NONE),
	  breakpointsRead(C64CAP_NONE),
	  breakpointsWrite(C64CAP_NONE),
	  stepInstruction(C64CAP_NONE),
	  stepCycle(C64CAP_NONE),
	  nativeMonitor(C64CAP_NONE),
	  snapshots(C64CAP_NONE),
	  timeline(C64CAP_NONE),
	  keyboardTextInput(C64CAP_NONE),
	  keyboardMatrixInput(C64CAP_NONE),
	  joystickInput(C64CAP_NONE),
	  mouseInput(C64CAP_NONE)
	{
	}
};

static inline bool C64HasCapability(EC64CapabilityLevel capabilityLevel)
{
	return capabilityLevel != C64CAP_NONE;
}

#endif
