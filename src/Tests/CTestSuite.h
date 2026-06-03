#pragma once

#include "ITestCallback.h"
#include <ctime>
#include <memory>
#include <string>
#include <vector>

class CTest;

struct CTestSuiteResult
{
	std::string testName;
	bool success;
	std::string summary;
};

class CTestSuite : public ITestCallback
{
public:
	CTestSuite();
	virtual ~CTestSuite();

	virtual void RegisterTests() = 0;

	static void RunFromCLI(CTestSuite *suite, const char *testName);
	// Register tests, write their names (one per line) to tests/results/test_list.txt
	// and stdout, then exit. Used by the subprocess-per-test orchestrator to
	// enumerate tests without hardcoding the list.
	static void ListTestsFromCLI(CTestSuite *suite);
	static bool isCLIModeActive;

	int defaultTestTimeoutSeconds = 30;
	int suiteTimeoutSeconds = 180;

	void Run();
	void Cancel();

	virtual void OnTestStepCompleted(CTest *test, int stepId, bool success, const char *message) override;
	virtual void OnTestCompleted(CTest *test, bool success, const char *summary) override;

protected:
	std::vector<std::unique_ptr<CTest> > tests;

private:
	void RunNextTest();
	void WriteResults();
	void StartTimeoutWatchdog();

	std::vector<CTestSuiteResult> results;
	int currentTestIndex;
	bool isRunning;
	bool exitOnCompletion;
	// When false (default), the suite runs every test and reports all failures
	// in one pass; a failing test no longer hides the tests after it. Set true
	// (CLI: --stop-on-first-failure) for fast-fail behaviour.
	bool stopOnFirstFailure;
	std::string resultsFilePath;
	time_t suiteStartTime = 0;
};
