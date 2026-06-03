#pragma once

#include "CTest.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "C64SettingsStorage.h"
#include "../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../Emulators/c64u/C64UConnectionStatus.h"
#include "../Emulators/c64u/C64UTestFixture.h"
#include "../DebugInterface/C64/C64BackendFactory.h"

class CTestC64UModeSwitch : public CTest
{
public:
	virtual const char *GetName() override { return "C64UModeSwitch"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

#ifndef RUN_COMMODORE64
		TestCompleted(true, "Skipped (C64 not enabled)");
		return;
#else
		// Create a fresh C64U backend in fixture mode
		C64UTestFixture::SetEnabled(true);
		CDebugInterfaceC64U *c64u = (CDebugInterfaceC64U *)C64BackendFactory::CreateBackend(viewC64, EMULATOR_TYPE_C64U, NULL, false);
		if (c64u == NULL)
		{
			C64UTestFixture::SetEnabled(false);
			TestCompleted(false, "Failed to create C64U backend for mode switch test");
			return;
		}

		// Connect in fixture mode
		c64u->Connect();
		if (c64u->GetConnectionStatus() != C64U_CONNECTION_STATUS_STREAMING)
		{
			C64UTestFixture::SetEnabled(false);
			delete c64u;
			TestCompleted(false, "Connect() in fixture mode must reach STREAMING");
			return;
		}

		// Step 1: After fixture Connect(), verify mode is SCREEN
		if (c64u->GetMode() != C64U_MODE_SCREEN)
		{
			C64UTestFixture::SetEnabled(false);
			delete c64u;
			TestCompleted(false, "Initial mode after Connect() must be SCREEN");
			return;
		}
		if (c64u->IsInTraceMode())
		{
			C64UTestFixture::SetEnabled(false);
			delete c64u;
			TestCompleted(false, "IsInTraceMode() must be false in SCREEN mode");
			return;
		}
		StepCompleted(1, true, "After Connect(), mode is SCREEN and IsInTraceMode() is false");

		// Step 2: EnterTraceMode(1) transitions to TRACE
		c64u->EnterTraceMode(1);
		if (c64u->GetMode() != C64U_MODE_TRACE)
		{
			C64UTestFixture::SetEnabled(false);
			delete c64u;
			TestCompleted(false, "EnterTraceMode(1) must set mode to TRACE");
			return;
		}
		if (!c64u->IsInTraceMode())
		{
			C64UTestFixture::SetEnabled(false);
			delete c64u;
			TestCompleted(false, "IsInTraceMode() must be true after EnterTraceMode()");
			return;
		}
		StepCompleted(2, true, "EnterTraceMode(1) transitions to TRACE, IsInTraceMode() returns true");

		// Step 3: EnterScreenMode() transitions back to SCREEN
		c64u->EnterScreenMode();
		if (c64u->GetMode() != C64U_MODE_SCREEN)
		{
			C64UTestFixture::SetEnabled(false);
			delete c64u;
			TestCompleted(false, "EnterScreenMode() must set mode to SCREEN");
			return;
		}
		if (c64u->IsInTraceMode())
		{
			C64UTestFixture::SetEnabled(false);
			delete c64u;
			TestCompleted(false, "IsInTraceMode() must be false after EnterScreenMode()");
			return;
		}
		StepCompleted(3, true, "EnterScreenMode() transitions to SCREEN, IsInTraceMode() returns false");

		// Step 4: Rapid switching (EnterTraceMode then immediately EnterScreenMode) - no crash
		c64u->EnterTraceMode(1);
		c64u->EnterScreenMode();
		if (c64u->GetMode() != C64U_MODE_SCREEN)
		{
			C64UTestFixture::SetEnabled(false);
			delete c64u;
			TestCompleted(false, "Rapid switch must end in SCREEN");
			return;
		}
		StepCompleted(4, true, "Rapid EnterTraceMode then EnterScreenMode is safe");

		// Step 5: EnterScreenMode() when already in screen mode is idempotent (no crash)
		c64u->EnterScreenMode();
		if (c64u->GetMode() != C64U_MODE_SCREEN)
		{
			C64UTestFixture::SetEnabled(false);
			delete c64u;
			TestCompleted(false, "Double EnterScreenMode() must remain SCREEN");
			return;
		}
		StepCompleted(5, true, "Double EnterScreenMode() is a safe no-op");

		// Step 6: Verify IsInTraceMode() returns correct value in each state
		// Already in SCREEN - should be false
		bool screenCheck = !c64u->IsInTraceMode();
		c64u->EnterTraceMode(1);
		bool traceCheck = c64u->IsInTraceMode();
		c64u->EnterScreenMode();
		bool backToScreenCheck = !c64u->IsInTraceMode();

		if (!screenCheck || !traceCheck || !backToScreenCheck)
		{
			C64UTestFixture::SetEnabled(false);
			delete c64u;
			TestCompleted(false, "IsInTraceMode() must track mode correctly through transitions");
			return;
		}
		StepCompleted(6, true, "IsInTraceMode() returns correct value across all state transitions");

		// Cleanup
		c64u->Disconnect();
		C64UTestFixture::SetEnabled(false);
		delete c64u;
		TestCompleted(true, "Mode switching works correctly: Screen<->Trace transitions, rapid switching, idempotency");
#endif
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
