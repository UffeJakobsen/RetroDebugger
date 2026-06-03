#pragma once

#include "CTest.h"

class CTestVicePlatformAbstraction : public CTest
{
public:
	virtual const char *GetName() override { return "VicePlatformAbstraction"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
