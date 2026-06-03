#ifndef _CTestGT2OrderList_h_
#define _CTestGT2OrderList_h_
#include "CTest.h"
class CTestGT2OrderList : public CTest
{
public:
	virtual const char *GetName() { return "GT2OrderList"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};
#endif
