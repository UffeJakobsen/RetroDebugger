#ifndef _CTestNesInputReplay_h_
#define _CTestNesInputReplay_h_

#include "CTest.h"

class CTestNesInputReplay : public CTest
{
public:
	virtual const char *GetName() { return "NesInputReplay"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};

#endif
