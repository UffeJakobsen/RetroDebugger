#pragma once

#include "CTest.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "C64SettingsStorage.h"
#include "../DebugInterface/C64/CC64BackendCapabilities.h"
#include "../DebugInterface/C64/C64BackendFactory.h"
#include "../Screens/C64/C64ViewAvailability.h"
#include "SYS_Main.h"
#include <cstdio>

class CTestC64BackendCapabilities : public CTest
{
public:
	virtual const char *GetName() override { return "C64BackendCapabilities"; }
	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

#ifndef RUN_COMMODORE64
		TestCompleted(true, "Skipped (C64 not enabled)");
		return;
#else
		CDebugInterfaceC64 *vice = viewC64->debugInterfaceC64;
		if (vice == NULL)
		{
			TestCompleted(false, "VICE debug interface is NULL");
			return;
		}

		CC64BackendCapabilities viceCaps = vice->GetC64BackendCapabilities();
		if (!viceCaps.isDefaultC64Backend)
		{
			TestCompleted(false, "VICE must stay the default C64 backend");
			return;
		}
		StepCompleted(1, true, "VICE reports default C64 backend capabilities");

		CDebugInterfaceC64 *c64u = C64BackendFactory::CreateBackend(viewC64, EMULATOR_TYPE_C64U, NULL, false);
		if (c64u == NULL)
		{
			TestCompleted(false, "Explicit C64U backend selection must create an additive backend");
			return;
		}
		CC64BackendCapabilities c64uCaps = c64u->GetC64BackendCapabilities();
		if (c64uCaps.isDefaultC64Backend)
		{
			delete c64u;
			TestCompleted(false, "C64U backend must not become the default C64 backend");
			return;
		}
		delete c64u;
		c64u = NULL;

		if (viewC64->debugInterfaceC64U == NULL)
		{
			TestCompleted(false, "Normal startup must create an additive C64U backend");
			return;
		}
		bool foundC64UInInterfaces = false;
		for (std::vector<CDebugInterface *>::iterator it = viewC64->debugInterfaces.begin(); it != viewC64->debugInterfaces.end(); ++it)
		{
			if (*it == viewC64->debugInterfaceC64U)
			{
				foundC64UInInterfaces = true;
				break;
			}
		}
		if (!foundC64UInInterfaces)
		{
			TestCompleted(false, "C64U backend must be registered in debugInterfaces for File-menu selection");
			return;
		}
		StepCompleted(2, true, "C64U backend is registered additively in debugInterfaces");

		if (C64BackendFactory::GetDefaultBackendType() != EMULATOR_TYPE_C64_VICE)
		{
			TestCompleted(false, "C64 backend factory default must resolve to VICE");
			return;
		}
		if (C64BackendFactory::ResolveBackendType(EMULATOR_TYPE_UNKNOWN) != EMULATOR_TYPE_C64_VICE)
		{
			TestCompleted(false, "Unknown C64 backend selection must resolve to VICE");
			return;
		}
		if (C64BackendFactory::ResolveBackendType(EMULATOR_TYPE_C64U) != EMULATOR_TYPE_C64U)
		{
			TestCompleted(false, "Explicit C64U backend selection must be preserved");
			return;
		}
		StepCompleted(3, true, "C64 backend factory keeps VICE default while preserving explicit C64U selection");

		if (C64ViewAvailability::GetMemoryViewAvailability(viceCaps) != C64_VIEW_AVAILABILITY_ENABLED)
		{
			TestCompleted(false, "VICE memory views must remain fully enabled");
			return;
		}
		if (C64ViewAvailability::GetDisassemblyViewAvailability(viceCaps) != C64_VIEW_AVAILABILITY_ENABLED)
		{
			TestCompleted(false, "VICE disassembly must remain fully enabled");
			return;
		}
		if (!C64ViewAvailability::ShouldShowTimeline(viceCaps))
		{
			TestCompleted(false, "VICE timeline must remain available");
			return;
		}
		StepCompleted(4, true, "VICE view-availability policy keeps existing generic C64 views enabled");

		char msg[256];
		snprintf(msg, sizeof(msg), "VICE capabilities: direct RAM=%d step cycle=%d native monitor=%d snapshots=%d", viceCaps.directRamRead, viceCaps.stepCycle, viceCaps.nativeMonitor, viceCaps.snapshots);
		TestCompleted(true, msg);
#endif
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
