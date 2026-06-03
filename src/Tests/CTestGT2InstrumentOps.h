#ifndef _CTestGT2InstrumentOps_H_
#define _CTestGT2InstrumentOps_H_

#include "CTest.h"

class CTestGT2InstrumentOps : public CTest
{
public:
	virtual const char *GetName() { return "GT2InstrumentOps"; }
	virtual void Run(ITestCallback *cb);
	virtual void Cancel() {}
};

#endif
