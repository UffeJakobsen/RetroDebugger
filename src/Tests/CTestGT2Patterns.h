#ifndef _CTestGT2Patterns_h_
#define _CTestGT2Patterns_h_
#include "CTest.h"
class CTestGT2Patterns : public CTest
{
public:
	virtual const char *GetName() { return "GT2Patterns"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};
#endif
