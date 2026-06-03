#pragma once

#include "CTest.h"

class CTestViceSoundIntegration : public CTest
{
public:
	virtual const char *GetName() override { return "ViceSoundIntegration"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
