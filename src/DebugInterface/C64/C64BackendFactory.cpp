#include "C64BackendFactory.h"
#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/vice/ViceInterface/CDebugInterfaceVice.h"
#include "../../Screens/CViewC64.h"

CDebugInterfaceC64 *C64BackendFactory::CreateBackend(CViewC64 *viewC64, u8 emulatorType, uint8 *c64memory, bool patchKernalFastBoot)
{
	switch (ResolveBackendType(emulatorType))
	{
		case EMULATOR_TYPE_C64_VICE:
			return new CDebugInterfaceVice(viewC64, c64memory, patchKernalFastBoot);

		case EMULATOR_TYPE_C64U:
			return new CDebugInterfaceC64U(viewC64);

		default:
			return NULL;
	}
}
