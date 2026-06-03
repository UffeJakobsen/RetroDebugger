#pragma once

#include "CTest.h"

class CTestViceModelConfig : public CTest
{
public:
	virtual const char *GetName() override { return "ViceModelConfig"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
