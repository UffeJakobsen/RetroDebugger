#ifndef _C64BACKENDFACTORY_H_
#define _C64BACKENDFACTORY_H_

#include "../../DebugInterface/DebuggerDefs.h"
#include "../../DebugInterface/C64/CDebugInterfaceC64.h"

class CViewC64;

class C64BackendFactory
{
public:
	static inline u8 GetDefaultBackendType()
	{
		return EMULATOR_TYPE_C64_VICE;
	}

	static inline bool IsC64BackendType(u8 emulatorType)
	{
		return emulatorType == EMULATOR_TYPE_C64_VICE || emulatorType == EMULATOR_TYPE_C64U;
	}

	static inline u8 ResolveBackendType(u8 emulatorType)
	{
		if (emulatorType == EMULATOR_TYPE_UNKNOWN)
			return GetDefaultBackendType();
		return emulatorType;
	}

	static CDebugInterfaceC64 *CreateBackend(CViewC64 *viewC64, u8 emulatorType, uint8 *c64memory, bool patchKernalFastBoot);
};

#endif
