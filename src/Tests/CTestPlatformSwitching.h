#pragma once

#include "CTest.h"

class CTestPlatformSwitching : public CTest
{
public:
	virtual const char *GetName() override { return "PlatformSwitching"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
