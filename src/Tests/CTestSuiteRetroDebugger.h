#pragma once

#include "CTestSuite.h"
#include <memory>
#include <vector>

class CTest;

void CTestSuiteRegisterRetroDebuggerTests(std::vector<std::unique_ptr<CTest> > &tests);

// RetroDebugger-specific test suite that registers all app tests
class CTestSuiteRetroDebugger : public CTestSuite
{
public:
	CTestSuiteRetroDebugger() {}
	virtual ~CTestSuiteRetroDebugger() {}

	virtual void RegisterTests() override
	{
		CTestSuiteRegisterRetroDebuggerTests(tests);
	}
};
