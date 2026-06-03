#pragma once

#include "CTest.h"

class CTestViceEmbeddedRoms : public CTest
{
public:
	virtual const char *GetName() override { return "ViceEmbeddedRoms"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
