#pragma once

#include "CTest.h"

class CTestDefaultWorkspaceSpecs : public CTest
{
public:
	virtual const char *GetName() override { return "DefaultWorkspaceSpecs"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
