#pragma once

#include "CTest.h"

class CTestRemoteProtocol : public CTest
{
public:
	virtual const char *GetName() override { return "RemoteProtocol"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
