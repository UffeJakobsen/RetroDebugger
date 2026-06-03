#pragma once

#include "CTest.h"

class CTestMCPProtocol : public CTest
{
public:
	virtual const char *GetName() override { return "MCPProtocol"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
