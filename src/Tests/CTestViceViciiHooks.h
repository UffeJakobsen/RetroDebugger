#pragma once

#include "CTest.h"

class CTestViceViciiHooks : public CTest
{
public:
	virtual const char *GetName() override { return "ViceViciiHooks"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
