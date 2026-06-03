#pragma once

#include "CTest.h"

class CTestViceDrive1541 : public CTest
{
public:
	virtual const char *GetName() override { return "ViceDrive1541"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
