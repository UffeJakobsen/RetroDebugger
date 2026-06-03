#pragma once
#include "CTest.h"

class CTestMonitorConsoleSelection : public CTest
{
public:
	virtual const char *GetName() { return "MonitorConsoleSelection"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};
