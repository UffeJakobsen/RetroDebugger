#pragma once

#include "CTest.h"

class CTestArpCycling : public CTest
{
public:
	virtual const char *GetName() override { return "ArpCycling"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
