#pragma once

#include "ITestCallback.h"
#include <ctime>

class CTest
{
public:
	CTest();
	virtual ~CTest();

	virtual const char *GetName() = 0;
	virtual void Run(ITestCallback *callback) = 0;
	virtual void Cancel() = 0;
	virtual void Teardown() {}

	bool IsRunning() { return isRunning; }

	int timeoutSeconds = 0;
	time_t startTime = 0;

	// Test grouping: "core" for RetroDebugger-core tests, or the plugin name
	// (e.g. "Remapper", "Fire") for plugin tests. Set by the per-plugin
	// registrars in src/Plugins/<P>/tests/. Used for grouped listing/reporting.
	const char *category = "core";

protected:
	void StepCompleted(int stepId, bool success, const char *message);
	void TestCompleted(bool success, const char *summary);

	int currentStep;
	bool isRunning;
	ITestCallback *callback;
};
