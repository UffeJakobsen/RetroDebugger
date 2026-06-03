#pragma once

#include "CTest.h"

class CTestMCPBridge : public CTest
{
public:
	virtual const char *GetName() override { return "MCPBridge"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
