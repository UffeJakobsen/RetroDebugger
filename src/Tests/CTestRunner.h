#pragma once

#include "ITestCallback.h"
#include <string>
#include <vector>

class CTest;

struct CTestResult
{
	std::string testName;
	bool success;
	std::string summary;
};

class CTestRunner : public ITestCallback
{
public:
	CTestRunner();
	virtual ~CTestRunner();

	void RunTest(CTest *test);
	void CancelCurrentTest();

	bool IsRunning();
	CTest *GetCurrentTest() { return currentTest; }

	static bool isTestPending;
	static bool IsTestPending() { return isTestPending; }

	virtual void OnTestStepCompleted(CTest *test, int stepId, bool success, const char *message) override;
	virtual void OnTestCompleted(CTest *test, bool success, const char *summary) override;

	std::vector<CTestResult> results;

private:
	CTest *currentTest;
};
