#pragma once

#include "CTest.h"

class CTestDisassemblySelection : public CTest
{
public:
	virtual const char *GetName() override { return "DisassemblySelection"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
