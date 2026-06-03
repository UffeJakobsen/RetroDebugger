#pragma once

#include "CTest.h"

class CTestDataDumpSelection : public CTest
{
public:
	virtual const char *GetName() override { return "DataDumpSelection"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
