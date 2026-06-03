#ifndef _CTestViceInputReplay_h_
#define _CTestViceInputReplay_h_

#include "CTest.h"

class CTestViceInputReplay : public CTest
{
public:
	virtual const char *GetName() { return "ViceInputReplay"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};

#endif
