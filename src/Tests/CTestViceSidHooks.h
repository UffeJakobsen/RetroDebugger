#pragma once

#include "CTest.h"

class CTestViceSidHooks : public CTest
{
public:
	virtual const char *GetName() override { return "ViceSidHooks"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
