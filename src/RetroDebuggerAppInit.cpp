#include "RetroDebuggerAppInit.h"
#include "DBG_Log.h"
#include "CSlrImage.h"
#include "RES_ResourceManager.h"
#include "GUI_Main.h"
#include "MT_API.h"
#include "C64CommandLine.h"
#include "CViewC64.h"
#include "SYS_Defs.h"
#include "RetroDebuggerEmbeddedData.h"
#include "CTestSuiteRetroDebugger.h"
#include "CTestRunner.h"
#include "VID_Main.h"
#include "SYS_CommandLine.h"
#include "SYS_Main.h"
#include "CMCPServer.h"
#include "C64SettingsStorage.h"
#include <cstring>
#include <cstdio>


#ifdef __APPLE__
extern "C" void RD_HideDockIcon();
#endif

#ifdef ENABLE_IMGUI_TEST_ENGINE
#include "CImGuiTestEngine.h"
#include "imgui_te_engine.h"
extern void RegisterRetroDebuggerTests(ImGuiTestEngine *engine);
static bool sRunTests = false;
static const char *sImGuiTestFilter = NULL;
static int sWarmupFrames = 0;
static bool sTestsQueued = false;
static const char *sImGuiResultsFilePath = "tests/results/last_run.txt";

static void WriteImGuiTestResults(const char *failureLabel, const char *failureSummary)
{
	FILE *f = fopen(sImGuiResultsFilePath, "w");
	if (!f)
	{
		LOGError("WriteImGuiTestResults: Failed to open %s", sImGuiResultsFilePath);
		return;
	}

	int passed = 0;
	int total = 0;

	if (failureLabel != NULL)
	{
		fprintf(f, "[%s] FAIL: %s\n", failureLabel, failureSummary);
		total = 1;
	}
	else
	{
		ImGuiTestEngine *engine = CImGuiTestEngine::GetEngine();
		if (engine != NULL)
		{
			ImVector<ImGuiTest *> tests;
			ImGuiTestEngine_GetTestList(engine, &tests);
			for (int i = 0; i < tests.Size; i++)
			{
				ImGuiTest *test = tests[i];
				ImGuiTestStatus status = test->Output.Status;
				if (status == ImGuiTestStatus_Unknown || status == ImGuiTestStatus_Queued || status == ImGuiTestStatus_Running)
					continue;

				bool success = (status == ImGuiTestStatus_Success);
				const char *summary = success ? "ImGui test passed" : "ImGui test failed";
				ImGuiTextBuffer failureLog;
				if (!success && !test->Output.Log.IsEmpty())
				{
					test->Output.Log.ExtractLinesForVerboseLevels(ImGuiTestVerboseLevel_Error, ImGuiTestVerboseLevel_Trace, &failureLog);
					if (!failureLog.empty())
						summary = failureLog.c_str();
				}
				fprintf(f, "[%s/%s] %s: %s\n",
						test->Category ? test->Category : "imgui",
						test->Name ? test->Name : "unnamed",
						success ? "PASS" : "FAIL",
						summary);
				total++;
				if (success)
					passed++;
			}
		}
	}

	fprintf(f, "---\n");
	fprintf(f, "RESULT: %d/%d passed\n", passed, total);
	fclose(f);

	LOGM("ImGui test results written to %s", sImGuiResultsFilePath);
}
#endif

// CTestSuite CLI flags
static bool sRunSuiteTest = false;
static bool sRunSuiteAll = false;
static bool sListTests = false;
static bool sExitAfterTests = false;
static bool sStartMCPServer = false;
static bool sStartMCPBridge = false;
static const char *sMCPBridgeHost = "127.0.0.1";
static int sMCPBridgePort = 0x0DEB;
static const char *sMCPBridgePath = "/stream";
static const char *sSuiteTestName = NULL;
static bool sSuiteTestScheduled = false;
static int sSuiteWarmupFrames = 0;

static void WriteCliFailureResult(const char *label, const char *summary)
{
	FILE *f = fopen("tests/results/last_run.txt", "w");
	if (!f)
	{
		LOGError("WriteCliFailureResult: Failed to open tests/results/last_run.txt");
		return;
	}
	fprintf(f, "[%s] FAIL: %s\n", label, summary);
	fprintf(f, "---\n");
	fprintf(f, "RESULT: 0/1 passed\n");
	fclose(f);
}

const char *MT_GetMainWindowTitle()
{
#if defined(GLOBAL_DEBUG_OFF)
	return "Retro Debugger v" RETRODEBUGGER_VERSION_STRING;
#else
	return "Retro Debugger v" RETRODEBUGGER_VERSION_STRING " (compiled on " __DATE__ " " __TIME__ ")";
#endif
}

const char *MT_GetSettingsFolderName()
{
	return "RetroDebugger";
}

void MT_GetDefaultWindowPositionAndSize(int *defaultWindowPosX, int *defaultWindowPosY, int *defaultWindowWidth, int *defaultWindowHeight, bool *maximized)
{
	*defaultWindowPosX = 50; //SDL_WINDOWPOS_CENTERED;
	*defaultWindowPosY = 125; //SDL_WINDOWPOS_CENTERED;
	*defaultWindowWidth = 510;
	*defaultWindowHeight = 510*9/16;
	*maximized = true;
}

void MT_PreInit()
{
	C64DebuggerInitStartupTasks();
	C64DebuggerParseCommandLine0();

	// Start MCP server as early as possible — the JSON-RPC thread responds
	// to the initialize handshake immediately. Tool registration is deferred
	// to the first tools/list call (when viewC64 is ready).
	// Parse MCP flags here since MT_PostInit hasn't run yet.
	for (int i = 0; i < (int)sysCommandLineArguments.size(); i++)
	{
		const char *arg = sysCommandLineArguments[i];
		if (strcmp(arg, "--mcp-server") == 0 || strcmp(arg, "--mcp-headless") == 0)
		{
			sStartMCPServer = true;
		}
		else if (strcmp(arg, "--mcp-live") == 0)
		{
			sStartMCPBridge = true;
		}
		else if (strcmp(arg, "--host") == 0 && i + 1 < (int)sysCommandLineArguments.size())
		{
			sMCPBridgeHost = sysCommandLineArguments[++i];
		}
		else if (strcmp(arg, "--port") == 0 && i + 1 < (int)sysCommandLineArguments.size())
		{
			sMCPBridgePort = atoi(sysCommandLineArguments[++i]);
		}
		else if (strcmp(arg, "--path") == 0 && i + 1 < (int)sysCommandLineArguments.size())
		{
			sMCPBridgePath = sysCommandLineArguments[++i];
		}
	}
	if (sStartMCPBridge)
	{
		// Set headless before returning to SYS_Startup — this ensures the
		// SDL_HINT_MAC_BACKGROUND_APP hint is set and no window is created.
		gHeadlessMode = true;
#ifdef __APPLE__
		// SDL_HINT_MAC_BACKGROUND_APP is only applied when SDL_INIT_VIDEO runs,
		// which is skipped in headless mode. Call setActivationPolicy directly
		// so the bridge process has no Dock icon.
		RD_HideDockIcon();
#endif
		MCP_BridgeStart(sMCPBridgeHost, sMCPBridgePort, sMCPBridgePath);
	}
	else if (sStartMCPServer)
	{
		MCP_ServerStart();
	}
}

void MT_GuiPreInit()
{
}

void MT_PostInit()
{
	LOGD("MT_PostInit");

	// Parse CLI flags early (before view creation)
	for (int i = 0; i < (int)sysCommandLineArguments.size(); i++)
	{
		const char *arg = sysCommandLineArguments[i];
		if (strcmp(arg, "--run-test") == 0)
		{
			if (i + 1 >= (int)sysCommandLineArguments.size())
			{
				const char *failureSummary = "--run-test requires a test name";
				LOGError("%s", failureSummary);
				WriteCliFailureResult("cli", failureSummary);
				SYS_CleanExit();
			}
			sRunSuiteTest = true;
			sSuiteTestName = sysCommandLineArguments[i + 1];
			i++;
		}
		else if (strcmp(arg, "--run-suite") == 0)
		{
			sRunSuiteAll = true;
		}
		else if (strcmp(arg, "--list-tests") == 0)
		{
			sListTests = true;
			gHeadlessMode = true;
		}
		else if (strcmp(arg, "--exit-after-tests") == 0)
		{
			sExitAfterTests = true;
		}
		else if (strcmp(arg, "--headless") == 0)
		{
			gHeadlessMode = true;
		}
		else if (strcmp(arg, "--mcp-server") == 0)
		{
			sStartMCPServer = true;
		}
		else if (strcmp(arg, "--mcp-headless") == 0)
		{
			sStartMCPServer = true;
			gHeadlessMode = true;
		}
		else if (strcmp(arg, "--mcp-live") == 0)
		{
			sStartMCPBridge = true;
			gHeadlessMode = true;
		}
#ifdef ENABLE_IMGUI_TEST_ENGINE
		else if (strcmp(arg, "--run-tests") == 0)
		{
			sRunTests = true;
		}
		else if (strcmp(arg, "--run-imgui-test") == 0)
		{
			if (i + 1 >= (int)sysCommandLineArguments.size())
			{
				const char *failureSummary = "--run-imgui-test requires a filter";
				LOGError("%s", failureSummary);
				WriteCliFailureResult("cli", failureSummary);
				SYS_CleanExit();
			}
			sRunTests = true;
			sImGuiTestFilter = sysCommandLineArguments[i + 1];
			i++;
		}
#endif
	}

	// Disable ImGui ini saving in headless mode to avoid overwriting user's layout
	#ifdef ENABLE_IMGUI_TEST_ENGINE
	if (sRunTests && (sRunSuiteTest || sRunSuiteAll))
	{
		const char *failureSummary = "Cannot combine CTestSuite and ImGui test CLI flags";
		LOGError("%s", failureSummary);
		WriteImGuiTestResults("cli", failureSummary);
		SYS_CleanExit();
	}
	#endif

	if (gHeadlessMode)
	{
		ImGui::GetIO().IniFilename = NULL;
	}

	// Set CLI mode flags before view creation
	if (sRunSuiteTest || sRunSuiteAll
#ifdef ENABLE_IMGUI_TEST_ENGINE
		|| sRunTests
#endif
	)
	{
		CTestSuite::isCLIModeActive = true;
		CTestRunner::isTestPending = true;
	}

	RetroDebuggerEmbeddedAddData();

	CViewC64 *viewC64 = new CViewC64(0, 0, -1, SCREEN_WIDTH, SCREEN_HEIGHT);
	guiMain->SetView(viewC64);

	if (sListTests)
	{
		// Enumerate registered tests and exit (used by the subprocess-per-test
		// orchestrator). Done after view creation so test construction has the
		// same app context it gets in a normal run.
		CTestSuite::ListTestsFromCLI(new CTestSuiteRetroDebugger());
	}

	VID_SetFPS(5);

#ifdef ENABLE_IMGUI_TEST_ENGINE
	CImGuiTestEngine::Init();
	RegisterRetroDebuggerTests(CImGuiTestEngine::GetEngine());
#endif
}

void MT_Render()
{
	// CTestSuite CLI scheduling
	// Note: scheduled here (not in MT_PostRenderEndFrame) because Metal's
	// nextDrawable blocks on hidden windows in headless mode, preventing
	// MT_PostRenderEndFrame from ever being called. Runs on first frame
	// since ImGui state is valid by this point (guiMain->RenderImGui()
	// has already executed in the same frame).
	if ((sRunSuiteTest || sRunSuiteAll) && !sSuiteTestScheduled)
	{
		sSuiteWarmupFrames++;
		if (sSuiteWarmupFrames >= 1)
		{
			sSuiteTestScheduled = true;
			if (viewC64->testRunner == NULL)
			{
				viewC64->testRunner = new CTestRunner();
			}
			CTestSuiteRetroDebugger *suite = new CTestSuiteRetroDebugger();
			if (sRunSuiteTest)
			{
				CTestSuite::RunFromCLI(suite, sSuiteTestName);
			}
			else
			{
				CTestSuite::RunFromCLI(suite, NULL);
			}
		}
	}

	// MCP server auto-start from saved settings (when user toggled it on
	// via Settings > Remote menu). Only start once, on the first render frame
	// so that viewC64 and all debug interfaces are fully initialized.
	static bool sMCPAutoStartChecked = false;
	if (!sMCPAutoStartChecked && !sStartMCPServer && c64SettingsRunMCPServer && mcpServer == NULL)
	{
		sMCPAutoStartChecked = true;
		MCP_ServerStart();
		// MCP tools dispatch through the WebSocket server's endpoint map
		if (viewC64->debuggerServer == NULL)
		{
			viewC64->DebuggerServerWebSocketsStart();
		}
	}
}

void MT_PostRenderEndFrame()
{
#ifdef ENABLE_IMGUI_TEST_ENGINE
	CImGuiTestEngine::PostSwap();

	if (sRunTests && !sTestsQueued)
	{
		sWarmupFrames++;
		if (sWarmupFrames >= 10)
		{
			ImGuiTestEngine *engine = CImGuiTestEngine::GetEngine();
			if (engine != NULL)
			{
				ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, sImGuiTestFilter, ImGuiTestRunFlags_RunFromCommandLine);
			}
			sTestsQueued = true;

			ImVector<ImGuiTestRunTask> queuedTests;
			if (engine != NULL)
			{
				ImGuiTestEngine_GetTestQueue(engine, &queuedTests);
			}
			if (queuedTests.Size == 0)
			{
				const char *failureLabel = (sImGuiTestFilter != NULL) ? sImGuiTestFilter : "imgui";
				const char *failureSummary = (sImGuiTestFilter != NULL) ? "No ImGui tests matched filter" : "No ImGui tests were queued";
				LOGError("ImGui CLI: %s", failureSummary);
				WriteImGuiTestResults(failureLabel, failureSummary);
				CTestRunner::isTestPending = false;
				if (sExitAfterTests)
				{
					SYS_Shutdown();
					return;
				}
			}
		}
	}
	if (sExitAfterTests && sTestsQueued && CImGuiTestEngine::IsTestQueueEmpty())
	{
		int tested = 0, success = 0;
		CImGuiTestEngine::GetResultSummary(&tested, &success);
		WriteImGuiTestResults(NULL, NULL);
		CTestRunner::isTestPending = false;
		LOGM("TEST RESULTS: %d/%d passed", success, tested);
		if (success == tested) {
			LOGM("ALL TESTS PASSED");
		} else {
			LOGM("SOME TESTS FAILED (%d failures)", tested - success);
		}
		SYS_Shutdown();
	}
#endif
}

void MT_Shutdown()
{
#ifdef ENABLE_IMGUI_TEST_ENGINE
	CImGuiTestEngine::Shutdown();
#endif
}
