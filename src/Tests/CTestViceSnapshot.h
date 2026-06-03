#pragma once

#include "CTest.h"

class CTestViceSnapshot : public CTest
{
public:
	virtual const char *GetName() override { return "ViceSnapshot"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
