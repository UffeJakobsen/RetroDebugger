#pragma once

#include "CTest.h"

class CTestVicePeripherals : public CTest
{
public:
	virtual const char *GetName() override { return "VicePeripherals"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
