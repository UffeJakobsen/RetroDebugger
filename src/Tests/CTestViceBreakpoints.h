#pragma once

#include "CTest.h"

class CTestViceBreakpoints : public CTest
{
public:
	virtual const char *GetName() override { return "ViceBreakpoints"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
