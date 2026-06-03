#pragma once

#include "CTest.h"

class CTestGoatTrackerExport : public CTest
{
public:
	virtual const char *GetName() override { return "GoatTrackerExport"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
