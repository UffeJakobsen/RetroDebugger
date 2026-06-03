#ifndef _CTestViceInstructionStepping_h_
#define _CTestViceInstructionStepping_h_

#include "CTest.h"

class CTestViceInstructionStepping : public CTest
{
public:
	virtual const char *GetName() override { return "ViceInstructionStepping"; }
	virtual void Run(ITestCallback *cb) override;
	virtual void Cancel() override;
};

#endif
