#ifndef _CTestAtariInputReplay_h_
#define _CTestAtariInputReplay_h_

#include "CTest.h"

class CTestAtariInputReplay : public CTest
{
public:
	virtual const char *GetName() { return "AtariInputReplay"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};

#endif
