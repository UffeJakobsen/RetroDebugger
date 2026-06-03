#include "CTestPlatformSwitching.h"
#include "CDebuggerServer.h"
#include "CViewC64.h"
#include "CDebugInterface.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <set>

using namespace nlohmann;

static char failureMsg[512];

static bool WaitForServerRunning(CViewC64 *viewC64, int timeoutMs)
{
	for (int elapsed = 0; elapsed < timeoutMs; elapsed += 50)
	{
		if (viewC64->debuggerServer != NULL && viewC64->debuggerServer->isRunning)
		{
			return true;
		}
		SYS_Sleep(50);
	}

	return viewC64->debuggerServer != NULL && viewC64->debuggerServer->isRunning;
}

// Helper: call an endpoint on the debugger server, parse JSON result
static json CallEndpoint(CDebuggerServer *server, const char *fn, json params = json())
{
	std::vector<char> *result = server->RunEndpointFunction(fn, "", params, nullptr, 0);
	if (!result || result->empty())
	{
		delete result;
		return json();
	}
	std::string raw(result->data(), result->size());
	delete result;

	// Strip binary portion if present (null-byte separated)
	auto nullPos = raw.find('\0');
	if (nullPos != std::string::npos)
		raw = raw.substr(0, nullPos);

	try
	{
		return json::parse(raw);
	}
	catch (const json::exception &e)
	{
		return {
			{"status", HTTP_INTERNAL_SERVER_ERROR},
			{"error", "Malformed JSON response"},
			{"details", e.what()}
		};
	}
}

static bool WaitForInterfaceRunningState(CDebugInterface *di, bool shouldBeRunning, int timeoutMs)
{
	for (int elapsed = 0; elapsed < timeoutMs; elapsed += 50)
	{
		if (di->isRunning == shouldBeRunning)
		{
			return true;
		}
		SYS_Sleep(50);
	}

	return di->isRunning == shouldBeRunning;
}

static bool WaitForInterfacePauseState(CDebugInterface *di, bool shouldBePaused, int timeoutMs)
{
	for (int elapsed = 0; elapsed < timeoutMs; elapsed += 50)
	{
		bool isPaused = di->GetDebugMode() != DEBUGGER_MODE_RUNNING;
		if (isPaused == shouldBePaused)
		{
			return true;
		}
		SYS_Sleep(50);
	}

	return (di->GetDebugMode() != DEBUGGER_MODE_RUNNING) == shouldBePaused;
}

static bool WaitForPlatformStateField(CDebuggerServer *server, const std::string &platformName,
										const char *fieldName, bool expectedValue, int timeoutMs)
{
	std::string fn = platformName + "/state";

	for (int elapsed = 0; elapsed < timeoutMs; elapsed += 50)
	{
		json resp = CallEndpoint(server, fn.c_str());
		if (!resp.empty() && resp.contains("result") && resp["result"].contains(fieldName)
			&& resp["result"][fieldName].is_boolean()
			&& resp["result"][fieldName].get<bool>() == expectedValue)
		{
			return true;
		}
		SYS_Sleep(50);
	}

	json resp = CallEndpoint(server, fn.c_str());
	return !resp.empty() && resp.contains("result") && resp["result"].contains(fieldName)
		&& resp["result"][fieldName].is_boolean()
		&& resp["result"][fieldName].get<bool>() == expectedValue;
}

static const json *FindPlatformEntry(const json &platforms, const std::string &name)
{
	if (!platforms.is_array())
		return NULL;

	for (const auto &platform : platforms)
	{
		if (platform.contains("name") && platform["name"].is_string()
			&& platform["name"].get<std::string>() == name)
		{
			return &platform;
		}
	}

	return NULL;
}

static void RestorePlatformState(CViewC64 *viewC64, CDebugInterface *di, bool shouldBeRunning, bool shouldBePaused)
{
	if (shouldBeRunning)
	{
		if (!di->isRunning)
		{
			viewC64->StartEmulationThread(di);
			WaitForInterfaceRunningState(di, true, 2000);
		}

		if (shouldBePaused)
		{
			if (di->GetDebugMode() == DEBUGGER_MODE_RUNNING)
			{
				di->PauseEmulationBlockedWait();
			}
		}
		else
		{
			if (di->GetDebugMode() != DEBUGGER_MODE_RUNNING)
			{
				di->RunContinueEmulation();
				WaitForInterfacePauseState(di, false, 1000);
			}
		}
	}
	else if (di->isRunning)
	{
		viewC64->StopEmulationThread(di);
	}
}

void CTestPlatformSwitching::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	failureMsg[0] = '\0';

	std::vector<bool> wasRunning;
	std::vector<bool> wasPaused;

	// Start the debugger server if not already running
	bool serverStartedByTest = false;
	if (viewC64->debuggerServer == NULL)
	{
		viewC64->DebuggerServerWebSocketsStart();
		serverStartedByTest = true;
		if (!WaitForServerRunning(viewC64, 2000))
		{
			if (viewC64->debuggerServer != NULL && viewC64->debuggerServer->isRunning)
			{
				viewC64->debuggerServer->Stop();
			}
			TestCompleted(false, "FAIL: Debugger server did not reach running state");
			return;
		}
	}

	auto FinishTest = [&](bool success, const char *message)
	{
		for (size_t i = 0; i < viewC64->debugInterfaces.size(); i++)
		{
			CDebugInterface *di = viewC64->debugInterfaces[i];
			RestorePlatformState(viewC64, di, wasRunning[i], wasPaused[i]);
		}

		if (serverStartedByTest && viewC64->debuggerServer != NULL && viewC64->debuggerServer->isRunning)
		{
			viewC64->debuggerServer->Stop();
		}

		TestCompleted(success, message);
	};

	CDebuggerServer *server = viewC64->debuggerServer;
	if (!server)
	{
		FinishTest(false, "FAIL: Could not start debugger server");
		return;
	}

	// Save the real initial emulator running states for cleanup and transition restoration.
	std::set<std::string> expectedPlatforms;
	const std::set<std::string> primaryPlatforms = {"c64", "atari800", "nes"};
	for (auto *di : viewC64->debugInterfaces)
	{
		wasRunning.push_back(di->isRunning);
		wasPaused.push_back(di->isRunning && di->GetDebugMode() != DEBUGGER_MODE_RUNNING);
		if (di->isRunning)
		{
			std::string name = di->GetPlatformNameEndpointString();
			if (primaryPlatforms.find(name) != primaryPlatforms.end())
			{
				expectedPlatforms.insert(name);
			}
		}
	}

	if (expectedPlatforms.size() < 2)
	{
		for (auto *di : viewC64->debugInterfaces)
		{
			if (expectedPlatforms.size() >= 2)
				break;

			std::string name = di->GetPlatformNameEndpointString();
			if (primaryPlatforms.find(name) == primaryPlatforms.end() || di->isRunning)
				continue;

			viewC64->StartEmulationThread(di);
			WaitForInterfaceRunningState(di, true, 2000);
		}

		expectedPlatforms.clear();
		for (auto *di : viewC64->debugInterfaces)
		{
			if (!di->isRunning)
				continue;

			std::string name = di->GetPlatformNameEndpointString();
			if (primaryPlatforms.find(name) != primaryPlatforms.end())
			{
				expectedPlatforms.insert(name);
			}
		}
	}

	if (expectedPlatforms.size() < 2)
	{
		sprintf(failureMsg, "FAIL: Need at least 2 running platforms for switching test, got %d", (int)expectedPlatforms.size());
		FinishTest(false, failureMsg);
		return;
	}

	int step = 0;

	// --- Test 1: server/platforms lists all running platforms ---
	{
		step++;
		json resp = CallEndpoint(server, "server/platforms");
		if (resp.empty() || !resp.contains("result") || !resp["result"].contains("platforms"))
		{
			FinishTest(false, "Test 1 FAIL: server/platforms returned no platforms");
			goto cleanup;
		}

		std::set<std::string> reportedPlatforms;
		for (const auto &p : resp["result"]["platforms"])
		{
			if (p.contains("name") && p.contains("running") && p["running"].get<bool>())
			{
				reportedPlatforms.insert(p["name"].get<std::string>());
			}
		}

		for (const auto &expected : expectedPlatforms)
		{
			if (reportedPlatforms.find(expected) == reportedPlatforms.end())
			{
				sprintf(failureMsg, "Test 1 FAIL: platform '%s' not listed as running in server/platforms", expected.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
		}

		char msg[256];
		snprintf(msg, sizeof(msg), "server/platforms lists %d running platforms", (int)reportedPlatforms.size());
		StepCompleted(step, true, msg);
	}

	// --- Test 2: server/platforms reflects running-state transitions for one primary platform ---
	{
		step++;
		auto platIt = expectedPlatforms.begin();
		std::string transitionPlatform = *platIt;
		bool initialRunning = false;
		bool initialPaused = false;

		CDebugInterface *transitionDi = NULL;
		for (size_t i = 0; i < viewC64->debugInterfaces.size(); i++)
		{
			auto *di = viewC64->debugInterfaces[i];
			if (di->GetPlatformNameEndpointString() == transitionPlatform)
			{
				transitionDi = di;
				initialRunning = di->isRunning;
				initialPaused = wasPaused[i];
				break;
			}
		}

		if (transitionDi == NULL)
		{
			sprintf(failureMsg, "Test 2 FAIL: could not map platform '%s' to debug interface", transitionPlatform.c_str());
			FinishTest(false, failureMsg);
			goto cleanup;
		}

		if (transitionDi->isRunning)
		{
			viewC64->StopEmulationThread(transitionDi);
		}

		bool sawStopped = false;
		for (int attempt = 0; attempt < 20; attempt++)
		{
			json resp = CallEndpoint(server, "server/platforms");
			if (!resp.empty() && resp.contains("result") && resp["result"].contains("platforms"))
			{
				const json *platform = FindPlatformEntry(resp["result"]["platforms"], transitionPlatform);
				if (platform != NULL && platform->contains("running") && (*platform)["running"].is_boolean()
					&& (*platform)["running"].get<bool>() == false)
				{
					sawStopped = true;
					break;
				}
			}
			SYS_Sleep(100);
		}
		if (!sawStopped)
		{
			sprintf(failureMsg, "Test 2 FAIL: platform '%s' did not report running=false after stop", transitionPlatform.c_str());
			FinishTest(false, failureMsg);
			goto cleanup;
		}

		if (initialRunning)
		{
			viewC64->StartEmulationThread(transitionDi);

			bool sawStarted = false;
			for (int attempt = 0; attempt < 20; attempt++)
			{
				json resp = CallEndpoint(server, "server/platforms");
				if (!resp.empty() && resp.contains("result") && resp["result"].contains("platforms"))
				{
					const json *platform = FindPlatformEntry(resp["result"]["platforms"], transitionPlatform);
					if (platform != NULL && platform->contains("running") && (*platform)["running"].is_boolean()
						&& (*platform)["running"].get<bool>() == true)
					{
						sawStarted = true;
						break;
					}
				}
				SYS_Sleep(100);
			}
			if (!sawStarted)
			{
				sprintf(failureMsg, "Test 2 FAIL: platform '%s' did not report running=true after restart", transitionPlatform.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
		}

		RestorePlatformState(viewC64, transitionDi, initialRunning, initialPaused);

		char msg[256];
		snprintf(msg, sizeof(msg), "server/platforms updates running state for '%s'", transitionPlatform.c_str());
		StepCompleted(step, true, msg);
	}

	// --- Test 3: cpu/status works for each platform ---
	{
		step++;
		for (const auto &plat : expectedPlatforms)
		{
			std::string fn = plat + "/cpu/status";
			json resp = CallEndpoint(server, fn.c_str());
			if (resp.empty())
			{
				sprintf(failureMsg, "Test 2 FAIL: %s returned empty", fn.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
			if (!resp.contains("result"))
			{
				sprintf(failureMsg, "Test 2 FAIL: %s missing result", fn.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
			// CPU status should contain at least PC register
			if (!resp["result"].contains("PC") && !resp["result"].contains("pc"))
			{
				sprintf(failureMsg, "Test 2 FAIL: %s result has no PC register", fn.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
		}
		char msg[256];
		snprintf(msg, sizeof(msg), "cpu/status works for %d platforms", (int)expectedPlatforms.size());
		StepCompleted(step, true, msg);
	}

	// --- Test 4: state endpoint works for each platform ---
	{
		step++;
		for (const auto &plat : expectedPlatforms)
		{
			std::string fn = plat + "/state";
			json resp = CallEndpoint(server, fn.c_str());
			if (resp.empty() || !resp.contains("result"))
			{
				sprintf(failureMsg, "Test 3 FAIL: %s returned empty or no result", fn.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
			json &result = resp["result"];
			if (!result.contains("platform") || !result.contains("isRunning"))
			{
				sprintf(failureMsg, "Test 3 FAIL: %s missing platform or isRunning", fn.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
			if (result["platform"].get<std::string>() != plat)
			{
				sprintf(failureMsg, "Test 3 FAIL: %s/state returned platform '%s' expected '%s'",
						fn.c_str(), result["platform"].get<std::string>().c_str(), plat.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
		}
		char msg[256];
		snprintf(msg, sizeof(msg), "state endpoint works for %d platforms", (int)expectedPlatforms.size());
		StepCompleted(step, true, msg);
	}

	// --- Test 5: memory read works for each platform ---
	{
		step++;
		for (const auto &plat : expectedPlatforms)
		{
			std::string fn = plat + "/cpu/memory/readBlock";
			json params;
			params["address"] = 0;
			params["size"] = 16;
			json resp = CallEndpoint(server, fn.c_str(), params);
			if (resp.empty())
			{
				sprintf(failureMsg, "Test 4 FAIL: %s returned empty", fn.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
			// A successful read returns status 200
			if (!resp.contains("status") || resp["status"].get<int>() != 200)
			{
				sprintf(failureMsg, "Test 4 FAIL: %s returned status %d",
						fn.c_str(), resp.contains("status") ? resp["status"].get<int>() : -1);
				FinishTest(false, failureMsg);
				goto cleanup;
			}
		}
		char msg[256];
		snprintf(msg, sizeof(msg), "memory read works for %d platforms", (int)expectedPlatforms.size());
		StepCompleted(step, true, msg);
	}

	// --- Test 6: server/endpoints contains entries for each platform ---
	{
		step++;
		json resp = CallEndpoint(server, "server/endpoints");
		if (resp.empty() || !resp.contains("result") || !resp["result"].contains("endpoints"))
		{
			FinishTest(false, "Test 5 FAIL: server/endpoints returned empty");
			goto cleanup;
		}

		for (const auto &plat : expectedPlatforms)
		{
			std::string prefix = plat + "/";
			bool found = false;
			for (const auto &ep : resp["result"]["endpoints"])
			{
				if (ep.contains("fn"))
				{
					std::string fn = ep["fn"].get<std::string>();
					if (fn.substr(0, prefix.size()) == prefix)
					{
						found = true;
						break;
					}
				}
			}
			if (!found)
			{
				sprintf(failureMsg, "Test 5 FAIL: no endpoints with prefix '%s' in server/endpoints", prefix.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
		}
		StepCompleted(step, true, "server/endpoints contains entries for all platforms");
	}

	// --- Test 7: server/capabilities lists capabilities for each platform ---
	{
		step++;
		json resp = CallEndpoint(server, "server/capabilities");
		if (resp.empty() || !resp.contains("result") || !resp["result"].contains("platforms"))
		{
			FinishTest(false, "Test 6 FAIL: server/capabilities returned empty");
			goto cleanup;
		}

		for (const auto &plat : expectedPlatforms)
		{
			bool found = false;
			for (const auto &p : resp["result"]["platforms"])
			{
				if (p.contains("name") && p["name"].get<std::string>() == plat)
				{
					found = true;
					if (!p.contains("endpointCount") || p["endpointCount"].get<int>() < 1)
					{
						sprintf(failureMsg, "Test 6 FAIL: platform '%s' has no endpoints in capabilities", plat.c_str());
						FinishTest(false, failureMsg);
						goto cleanup;
					}
					break;
				}
			}
			if (!found)
			{
				sprintf(failureMsg, "Test 6 FAIL: platform '%s' not found in server/capabilities", plat.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
		}
		StepCompleted(step, true, "server/capabilities lists all platforms with endpoints");
	}

	// --- Test 8: pause/continue works across platforms without cross-talk ---
	{
		step++;
		// Pause first platform, verify others are not paused
		auto platIt = expectedPlatforms.begin();
		std::string firstPlat = *platIt;

		// Ensure all are running (un-paused)
		for (const auto &plat : expectedPlatforms)
		{
			std::string fn = plat + "/continue";
			json resp = CallEndpoint(server, fn.c_str());
			if (resp.empty() || !resp.contains("status") || resp["status"].get<int>() != 200)
			{
				sprintf(failureMsg, "Test 7 FAIL: %s/continue failed while preparing pause test", plat.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
			if (!WaitForPlatformStateField(server, plat, "isPaused", false, 1000))
			{
				sprintf(failureMsg, "Test 7 FAIL: %s did not report unpaused after continue", plat.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
		}

		// Pause the first platform
		{
			std::string fn = firstPlat + "/pause";
			json resp = CallEndpoint(server, fn.c_str());
			if (resp.empty() || !resp.contains("status") || resp["status"].get<int>() != 200)
			{
				sprintf(failureMsg, "Test 7 FAIL: %s/pause failed", firstPlat.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
		}

		// Verify the paused platform reports paused state
		if (!WaitForPlatformStateField(server, firstPlat, "isPaused", true, 1000))
		{
			sprintf(failureMsg, "Test 7 FAIL: %s reports not paused after pause command", firstPlat.c_str());
			FinishTest(false, failureMsg);
			goto cleanup;
		}

		for (const auto &plat : expectedPlatforms)
		{
			if (plat == firstPlat)
				continue;

			std::string fn = plat + "/state";
			json resp = CallEndpoint(server, fn.c_str());
			if (resp.empty() || !resp.contains("result") || !resp["result"].contains("isPaused")
				|| !resp["result"]["isPaused"].is_boolean())
			{
				sprintf(failureMsg, "Test 7 FAIL: %s/state missing isPaused while checking cross-talk", plat.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
			if (resp["result"]["isPaused"].get<bool>())
			{
				sprintf(failureMsg, "Test 7 FAIL: %s was paused by %s/pause cross-talk", plat.c_str(), firstPlat.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
		}

		// Resume the paused platform
		{
			std::string fn = firstPlat + "/continue";
			json resp = CallEndpoint(server, fn.c_str());
			if (resp.empty() || !resp.contains("status") || resp["status"].get<int>() != 200)
			{
				sprintf(failureMsg, "Test 7 FAIL: %s/continue failed after pause", firstPlat.c_str());
				FinishTest(false, failureMsg);
				goto cleanup;
			}
		}
		if (!WaitForPlatformStateField(server, firstPlat, "isPaused", false, 1000))
		{
			sprintf(failureMsg, "Test 7 FAIL: %s remained paused after continue", firstPlat.c_str());
			FinishTest(false, failureMsg);
			goto cleanup;
		}

		StepCompleted(step, true, "pause/continue works without cross-platform interference");
	}

	{
		char summary[256];
		snprintf(summary, sizeof(summary), "All platform switching tests passed (%d/%d) across %d platforms",
				 step, step, (int)expectedPlatforms.size());
		FinishTest(true, summary);
		return;
	}

cleanup:
	return;
}

void CTestPlatformSwitching::Cancel()
{
	isRunning = false;
}
