#ifndef _CTestGT2TableEditor_H_
#define _CTestGT2TableEditor_H_

#include "CTest.h"

class CTestGT2TableEditor : public CTest
{
public:
	virtual const char *GetName() { return "GT2TableEditor"; }
	virtual void Run(ITestCallback *cb);
	virtual void Cancel() {}
};

#endif
