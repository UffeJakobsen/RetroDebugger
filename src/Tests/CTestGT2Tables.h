#ifndef _CTestGT2Tables_h_
#define _CTestGT2Tables_h_
#include "CTest.h"
class CTestGT2Tables : public CTest
{
public:
	virtual const char *GetName() { return "GT2Tables"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};
#endif
