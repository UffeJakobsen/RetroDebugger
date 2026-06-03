#ifndef _CTestGT2TitleBar_h_
#define _CTestGT2TitleBar_h_
#include "CTest.h"
class CTestGT2TitleBar : public CTest
{
public:
	virtual const char *GetName() { return "GT2TitleBar"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};
#endif
