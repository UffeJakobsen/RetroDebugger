#ifndef _CTestGT2Status_h_
#define _CTestGT2Status_h_
#include "CTest.h"
class CTestGT2Status : public CTest
{
public:
	virtual const char *GetName() { return "GT2Status"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};
#endif
