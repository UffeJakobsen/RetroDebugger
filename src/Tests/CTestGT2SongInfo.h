#ifndef _CTestGT2SongInfo_h_
#define _CTestGT2SongInfo_h_
#include "CTest.h"
class CTestGT2SongInfo : public CTest
{
public:
	virtual const char *GetName() { return "GT2SongInfo"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};
#endif
