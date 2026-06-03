#ifndef _CTestGT2SelectionOps_H_
#define _CTestGT2SelectionOps_H_

#include "CTest.h"

class CTestGT2SelectionOps : public CTest
{
public:
	virtual const char *GetName() { return "GT2SelectionOps"; }
	virtual void Run(ITestCallback *cb);
	virtual void Cancel() {}
};

#endif
