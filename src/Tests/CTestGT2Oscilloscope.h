#ifndef _CTestGT2Oscilloscope_h_
#define _CTestGT2Oscilloscope_h_

#include "CTest.h"

class CTestGT2Oscilloscope : public CTest
{
public:
	virtual const char *GetName() { return "GT2Oscilloscope"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};

#endif
