#ifndef _CTestGT2Instrument_h_
#define _CTestGT2Instrument_h_
#include "CTest.h"
class CTestGT2Instrument : public CTest
{
public:
	virtual const char *GetName() { return "GT2Instrument"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};
#endif
