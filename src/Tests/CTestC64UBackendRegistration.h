#pragma once

#include "CTest.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "C64SettingsStorage.h"
#include "C64CommandLine.h"
#include "../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../Emulators/c64u/C64UMode.h"
#include "../Emulators/c64u/C64UTestFixture.h"
#include "../DebugInterface/C64/C64BackendFactory.h"

#include <vector>

extern std::vector<const char *> sysCommandLineArguments;

class CTestC64UBackendRegistration : public CTest
{
public:
	virtual const char *GetName() override { return "C64UBackendRegistration"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

#ifndef RUN_COMMODORE64
		TestCompleted(true, "Skipped (C64 not enabled)");
		return;
#else
		C64UTestFixture::SetEnabled(false);
		std::vector<const char *> previousCommandLineArguments = sysCommandLineArguments;
		u8 previousSelectedEmulator = c64SettingsSelectEmulator;
		sysCommandLineArguments.clear();
		sysCommandLineArguments.push_back("--c64u-test-fixture");
		c64SettingsSelectEmulator = EMULATOR_TYPE_UNKNOWN;
		C64DebuggerParseCommandLine2();
		if (!C64UTestFixture::IsEnabled())
		{
			sysCommandLineArguments = previousCommandLineArguments;
			c64SettingsSelectEmulator = previousSelectedEmulator;
			TestCompleted(false, "C64U test fixture command line flag must enable fixture mode");
			return;
		}
		if (c64SettingsSelectEmulator != EMULATOR_TYPE_UNKNOWN)
		{
			sysCommandLineArguments = previousCommandLineArguments;
			c64SettingsSelectEmulator = previousSelectedEmulator;
			C64UTestFixture::SetEnabled(false);
			TestCompleted(false, "C64U test fixture flag must stay off the selected-backend startup path");
			return;
		}
		sysCommandLineArguments = previousCommandLineArguments;
		c64SettingsSelectEmulator = previousSelectedEmulator;
		C64UTestFixture::SetEnabled(false);
		StepCompleted(1, true, "C64U fixture flag stays off the selected-backend startup path");

		CDebugInterfaceC64 *di = C64BackendFactory::CreateBackend(viewC64, EMULATOR_TYPE_C64U, NULL, false);
		if (di == NULL)
		{
			TestCompleted(false, "C64U backend factory failed");
			return;
		}

		if (di->GetEmulatorType() != EMULATOR_TYPE_C64U)
		{
			delete di;
			TestCompleted(false, "wrong emulator type");
			return;
		}
		StepCompleted(2, true, "Factory creates the explicit C64U backend");

		CDebugInterfaceC64U *c64u = (CDebugInterfaceC64U *)di;
		if (c64u->GetMode() != C64U_MODE_DISCONNECTED)
		{
			delete c64u;
			TestCompleted(false, "C64U must start disconnected");
			return;
		}

		CC64BackendCapabilities caps = c64u->GetC64BackendCapabilities();
		if (caps.isDefaultC64Backend)
		{
			delete c64u;
			TestCompleted(false, "C64U must not become the default C64 backend");
			return;
		}
		if (!caps.supportsScreenMode || !caps.screenAndTraceAreMutuallyExclusive)
		{
			delete c64u;
			TestCompleted(false, "C64U skeleton must expose the screen/trace mode model conservatively");
			return;
		}
		StepCompleted(3, true, "C64U skeleton reports conservative additive backend capabilities");

		if (C64UTestFixture::IsEnabled())
		{
			delete c64u;
			TestCompleted(false, "C64U test fixture must be disabled by default");
			return;
		}
		C64UTestFixture::SetEnabled(true);
		if (!C64UTestFixture::IsEnabled())
		{
			delete c64u;
			TestCompleted(false, "C64U test fixture flag did not enable");
			return;
		}
		if (!c64u->IsRunningInTestFixtureMode())
		{
			delete c64u;
			TestCompleted(false, "C64U skeleton must report deterministic fixture mode when enabled");
			return;
		}
		StepCompleted(4, true, "C64U deterministic no-hardware fixture mode is available");

		if (viewC64->debugInterfaceC64U == NULL)
		{
			C64UTestFixture::SetEnabled(false);
			delete c64u;
			TestCompleted(false, "normal VICE startup must still create an additive C64U backend for the File menu selector");
			return;
		}
		bool foundRegisteredC64U = false;
		for (std::vector<CDebugInterface *>::iterator it = viewC64->debugInterfaces.begin(); it != viewC64->debugInterfaces.end(); it++)
		{
			if (*it == viewC64->debugInterfaceC64U)
			{
				foundRegisteredC64U = true;
				break;
			}
		}
		if (!foundRegisteredC64U)
		{
			C64UTestFixture::SetEnabled(false);
			delete c64u;
			TestCompleted(false, "normal VICE startup must register C64U as an additive File-menu backend option");
			return;
		}
		StepCompleted(5, true, "Normal VICE startup still registers additive C64U backend selection");

		bool viceWasRunning = viewC64->debugInterfaceC64 != NULL && viewC64->debugInterfaceC64->isRunning;
		bool previousRunVice = c64SettingsRunVice;
		u8 previousEmulatorType = c64SettingsSelectEmulator;
		CDebugInterfaceC64 *previousC64U = viewC64->debugInterfaceC64U;
		if (viceWasRunning)
		{
			viewC64->StopEmulationThread(viewC64->debugInterfaceC64);
		}
		viewC64->debugInterfaceC64U = c64u;
		c64SettingsRunVice = true;
		c64SettingsSelectEmulator = EMULATOR_TYPE_C64U;
		viewC64->StartEmulationThread(c64u);
		if (!c64u->isRunning)
		{
			viewC64->debugInterfaceC64U = previousC64U;
			c64SettingsRunVice = previousRunVice;
			c64SettingsSelectEmulator = previousEmulatorType;
			delete c64u;
			if (viceWasRunning)
			{
				viewC64->StartEmulationThread(viewC64->debugInterfaceC64);
			}
			TestCompleted(false, "C64U fixture backend must follow the normal start lifecycle");
			return;
		}
		if (viewC64->debugInterfaceC64 != NULL && viewC64->debugInterfaceC64->isRunning)
		{
			viewC64->debugInterfaceC64U = previousC64U;
			c64SettingsRunVice = previousRunVice;
			c64SettingsSelectEmulator = previousEmulatorType;
			delete c64u;
			TestCompleted(false, "VICE must stay stopped while the C64U fixture backend is active");
			return;
		}
		StepCompleted(6, true, "C64U fixture backend starts without booting VICE underneath it");

		viewC64->StopEmulationThread(c64u);
		viewC64->debugInterfaceC64U = previousC64U;
		c64SettingsRunVice = previousRunVice;
		c64SettingsSelectEmulator = previousEmulatorType;
		C64UTestFixture::SetEnabled(false);
		delete c64u;
		if (viceWasRunning)
		{
			viewC64->StartEmulationThread(viewC64->debugInterfaceC64);
		}
		TestCompleted(true, "C64U backend skeleton is registered and fixture-ready");
#endif
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
