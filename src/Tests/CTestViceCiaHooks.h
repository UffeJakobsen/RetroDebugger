#pragma once

#include "CTest.h"

class CTestViceCiaHooks : public CTest
{
public:
	virtual const char *GetName() override { return "ViceCiaHooks"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
