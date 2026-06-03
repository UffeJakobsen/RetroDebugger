#include "CTestAutoLayoutPreservation.h"
#include "CViewC64.h"
#include "CGuiMain.h"
#include "CDebugInterface.h"
#include "CDebugInterfaceC64.h"
#include "CDebugInterfaceAtari.h"
#include "CLayoutManager.h"
#include "SYS_Funct.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>

struct SWindowSnapshot {
	std::string name;
	ImVec2 pos;
	ImVec2 size;
	ImGuiID dockId;
	bool wasDocked;
	bool inMainDockspace;
	std::string dockPath;
};

void CTestAutoLayoutPreservation::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;

	CDebugInterface *diC64 = viewC64->debugInterfaceC64;
	CDebugInterface *diAtari = viewC64->debugInterfaceAtari;

	if (!diC64 || !diAtari)
	{
		TestCompleted(false, "Need both C64 and Atari emulators compiled in");
		return;
	}

	bool c64WasRunning = diC64->isRunning;
	bool atariWasRunning = diAtari->isRunning;

	auto restoreOriginalStates = [&]()
	{
		if (!c64WasRunning && diC64->isRunning)
			viewC64->StopEmulationThread(diC64);
		if (c64WasRunning && !diC64->isRunning)
			viewC64->StartEmulationThread(diC64);
		if (!atariWasRunning && diAtari->isRunning)
			viewC64->StopEmulationThread(diAtari);
		if (atariWasRunning && !diAtari->isRunning)
			viewC64->StartEmulationThread(diAtari);
	};

	auto forceRenderNewViews = [](CDebugInterface *di)
	{
		for (auto *view : di->views)
		{
			if (!view || !view->visible || view->imGuiWindow != NULL)
				continue;
			view->RenderImGui();
		}
	};

	auto reRenderVisibleViews = [](CDebugInterface *di)
	{
		for (auto *view : di->views)
		{
			if (!view || !view->visible)
				continue;
			view->RenderImGui();
		}
	};

	auto setWasActive = [](CDebugInterface *di, bool wasActive)
	{
		for (auto *view : di->views)
		{
			if (!view || !view->imGuiWindow)
				continue;
			view->imGuiWindow->WasActive = wasActive;
		}
	};

	auto valueMatches = [](float a, float b, float tolerance = 8.0f, float pctTolerance = 0.10f) -> bool
	{
		float delta = fabsf(a - b);
		float maxValue = fmaxf(a, b);
		return (delta <= tolerance) || (maxValue > 0.0f && delta / maxValue <= pctTolerance);
	};

	auto vecMatches = [&](ImVec2 a, ImVec2 b, float tolerance = 8.0f, float pctTolerance = 0.10f) -> bool
	{
		return valueMatches(a.x, b.x, tolerance, pctTolerance)
			&& valueMatches(a.y, b.y, tolerance, pctTolerance);
	};

	auto buildDockPath = [&](ImGuiDockNode *node) -> std::string
	{
		if (!node)
			return "missing";

		std::string path;
		ImGuiDockNode *current = node;
		while (current)
		{
			if (current->ParentNode)
			{
				const char *axis = "N";
				if (current->ParentNode->SplitAxis == ImGuiAxis_X)
					axis = "X";
				else if (current->ParentNode->SplitAxis == ImGuiAxis_Y)
					axis = "Y";

				int branch = -1;
				if (current->ParentNode->ChildNodes[0] == current)
					branch = 0;
				else if (current->ParentNode->ChildNodes[1] == current)
					branch = 1;

				char segment[32];
				snprintf(segment, sizeof(segment), "%s%d", axis, branch);
				if (path.empty())
					path = segment;
				else
					path = std::string(segment) + "/" + path;
			}
			current = current->ParentNode;
		}

		if (path.empty())
			path = "root";

		ImGuiDockNode *root = node;
		while (root->ParentNode)
			root = root->ParentNode;

		return std::string(root->IsFloatingNode() ? "floating:" : "main:") + path;
	};

	auto snapshotWindow = [&](const char *windowName, SWindowSnapshot &snapshot, std::string &details) -> bool
	{
		ImGuiWindow *win = ImGui::FindWindowByName(windowName);
		if (!win)
		{
			details += std::string(windowName) + ": window not found; ";
			return false;
		}

		ImGuiDockNode *node = NULL;
		if (win->DockId != 0)
			node = ImGui::DockBuilderGetNode(win->DockId);

		snapshot.name = windowName;
		snapshot.pos = win->Pos;
		snapshot.size = win->SizeFull;
		snapshot.dockId = win->DockId;
		snapshot.wasDocked = (win->DockId != 0 && node != NULL);
		snapshot.inMainDockspace = false;
		snapshot.dockPath = (win->DockId == 0) ? "undocked" : "missing";

		if (node)
		{
			snapshot.dockPath = buildDockPath(node);
			ImGuiDockNode *root = node;
			while (root->ParentNode)
				root = root->ParentNode;
			snapshot.inMainDockspace = !root->IsFloatingNode();
		}

		return true;
	};

	const char *fixtureVerificationWindowNames[] = {
		"C64 Disassembly",
		"C64 Memory map",
		"C64 Memory",
		"C64 CPU",
		"C64 Screen",
		"C64 Timeline"
	};
	const int numFixtureVerificationWindows = sizeof(fixtureVerificationWindowNames) / sizeof(fixtureVerificationWindowNames[0]);

	const char *expectedLayoutsPath = getenv("C64D_TEST_EXPECTED_LAYOUTS_FILE");
	const char *expectedFixtureMarker = getenv("C64D_TEST_EXPECTED_LAYOUTS_FIXTURE");
	if (expectedLayoutsPath && expectedLayoutsPath[0])
	{
		if (settingsPathToLayoutsFile)
		{
			char *actualLayoutsPathRaw = settingsPathToLayoutsFile->GetStdASCII();
			std::string actualLayoutsPath = actualLayoutsPathRaw ? actualLayoutsPathRaw : "";
			if (actualLayoutsPath != expectedLayoutsPath)
			{
				char msg[1024];
				snprintf(msg, sizeof(msg), "Loaded layouts path mismatch: expected '%s', got '%s'", expectedLayoutsPath, actualLayoutsPath.c_str());
				TestCompleted(false, msg);
				return;
			}
		}

		if (expectedFixtureMarker && expectedFixtureMarker[0])
		{
			forceRenderNewViews(diC64);
			reRenderVisibleViews(diC64);

			int verifiedWindows = 0;
			std::string fixtureDetails;
			for (int i = 0; i < numFixtureVerificationWindows; i++)
			{
				SWindowSnapshot fixtureSnapshot;
				if (!snapshotWindow(fixtureVerificationWindowNames[i], fixtureSnapshot, fixtureDetails))
					continue;
				if (!fixtureSnapshot.wasDocked)
				{
					fixtureDetails += std::string(fixtureVerificationWindowNames[i]) + ": expected docked fixture window; ";
					continue;
				}
				verifiedWindows++;
			}

			if (verifiedWindows != numFixtureVerificationWindows)
			{
				char msg[1024];
				snprintf(msg, sizeof(msg),
				         "Fixture verification failed for supplied layouts file: %s",
				         fixtureDetails.c_str());
				TestCompleted(false, msg);
				return;
			}
		}
	}

	int step = 1;

	if (diC64->isRunning)
		diC64->PauseEmulationBlockedWait();
	if (diC64->isRunning)
		viewC64->StopEmulationThread(diC64);
	if (diAtari->isRunning)
		diAtari->PauseEmulationBlockedWait();
	if (diAtari->isRunning)
		viewC64->StopEmulationThread(diAtari);
	SYS_Sleep(500);
	StepCompleted(step, true, "C64 and Atari stopped");

	step++;
	viewC64->StartEmulationThread(diC64);
	SYS_Sleep(2000);
	guiMain->UpdateLayouts();
	StepCompleted(step, true, "C64 started from fixture layout");

	step++;
	forceRenderNewViews(diC64);
	reRenderVisibleViews(diC64);
	setWasActive(diC64, true);
	setWasActive(diAtari, false);

	std::vector<SWindowSnapshot> c64Snapshots;
	std::string captureDetails;
	for (auto *view : diC64->views)
	{
		if (!view || !view->visible)
			continue;

		SWindowSnapshot snapshot;
		if (!snapshotWindow(view->name, snapshot, captureDetails))
			continue;
		if (!snapshot.wasDocked)
			continue;
		c64Snapshots.push_back(snapshot);
	}

	bool captureOk = !c64Snapshots.empty();
	char captureMsg[512];
	snprintf(captureMsg, sizeof(captureMsg),
	         "Captured %d visible docked C64 fixture windows directly from loaded layout",
	         (int)c64Snapshots.size());
	StepCompleted(step, captureOk, captureMsg);
	if (!captureOk)
	{
		restoreOriginalStates();
		char msg[1024];
		snprintf(msg, sizeof(msg), "Could not capture fixture-based C64 baseline: %s", captureDetails.c_str());
		TestCompleted(false, msg);
		return;
	}

	step++;
	diC64->PauseEmulationBlockedWait();
	viewC64->StopEmulationThread(diC64);
	SYS_Sleep(500);
	StepCompleted(step, true, "C64 stopped");

	step++;
	viewC64->StartEmulationThread(diAtari);
	SYS_Sleep(2000);
	StepCompleted(step, true, "Atari started");

	step++;
	forceRenderNewViews(diAtari);
	reRenderVisibleViews(diAtari);
	setWasActive(diC64, false);
	setWasActive(diAtari, true);
	guiMain->RequestAutoLayoutVisibleViewsDockedPreserveScan();
	guiMain->RunAutoLayoutIfRequested();
	guiMain->UpdateLayouts();
	reRenderVisibleViews(diAtari);
	StepCompleted(step, true, "Auto Layout (Docked - Preserve Scan) ran on Atari");

	step++;
	diAtari->PauseEmulationBlockedWait();
	viewC64->StopEmulationThread(diAtari);
	SYS_Sleep(500);
	StepCompleted(step, true, "Atari stopped");

	step++;
	viewC64->StartEmulationThread(diC64);
	SYS_Sleep(2000);
	guiMain->UpdateLayouts();
	StepCompleted(step, true, "C64 restarted");

	step++;
	forceRenderNewViews(diC64);
	reRenderVisibleViews(diC64);
	setWasActive(diC64, true);

	int matched = 0;
	int mismatched = 0;
	std::string failDetails;
	for (const SWindowSnapshot &snapshot : c64Snapshots)
	{
		SWindowSnapshot currentSnapshot;
		if (!snapshotWindow(snapshot.name.c_str(), currentSnapshot, failDetails))
		{
			mismatched++;
			continue;
		}

		if (currentSnapshot.wasDocked != snapshot.wasDocked)
		{
			mismatched++;
			failDetails += snapshot.name + ": docked state changed; ";
			continue;
		}

		if (!currentSnapshot.wasDocked)
		{
			mismatched++;
			failDetails += snapshot.name + ": no longer docked; ";
			continue;
		}

		if (!vecMatches(snapshot.pos, currentSnapshot.pos))
		{
			mismatched++;
			char buf[384];
			snprintf(buf, sizeof(buf), "%s: position mismatch (was %.0fx%.0f, now %.0fx%.0f); ",
			         snapshot.name.c_str(), snapshot.pos.x, snapshot.pos.y,
			         currentSnapshot.pos.x, currentSnapshot.pos.y);
			failDetails += buf;
			continue;
		}

		if (!vecMatches(snapshot.size, currentSnapshot.size))
		{
			mismatched++;
			char buf[384];
			snprintf(buf, sizeof(buf), "%s: size mismatch (was %.0fx%.0f, now %.0fx%.0f); ",
			         snapshot.name.c_str(), snapshot.size.x, snapshot.size.y,
			         currentSnapshot.size.x, currentSnapshot.size.y);
			failDetails += buf;
			continue;
		}

		if (snapshot.inMainDockspace != currentSnapshot.inMainDockspace)
		{
			mismatched++;
			char buf[512];
			snprintf(buf, sizeof(buf), "%s: dockspace root changed (was %s, now %s); ",
			         snapshot.name.c_str(),
			         snapshot.inMainDockspace ? "main" : "floating",
			         currentSnapshot.inMainDockspace ? "main" : "floating");
			failDetails += buf;
			continue;
		}

		if (snapshot.dockPath != currentSnapshot.dockPath)
		{
			mismatched++;
			char buf[768];
			snprintf(buf, sizeof(buf), "%s: dock path changed (was %s, now %s); ",
			         snapshot.name.c_str(), snapshot.dockPath.c_str(), currentSnapshot.dockPath.c_str());
			failDetails += buf;
			continue;
		}

		matched++;
	}

	char verifyMsg[512];
	snprintf(verifyMsg, sizeof(verifyMsg),
	         "%d/%d C64 fixture windows preserved after Atari autolayout",
	         matched, (int)c64Snapshots.size());
	bool verifyOk = (mismatched == 0 && matched == (int)c64Snapshots.size());
	StepCompleted(step, verifyOk, verifyMsg);

	step++;
	restoreOriginalStates();
	StepCompleted(step, true, "Restored original emulator states");

	if (verifyOk)
	{
		char summary[512];
		snprintf(summary, sizeof(summary),
		         "All %d C64 fixture windows preserved after Atari autolayout cycle",
		         matched);
		TestCompleted(true, summary);
	}
	else
	{
		char summary[2048];
		snprintf(summary, sizeof(summary), "Fixture layout corruption detected: %s", failDetails.c_str());
		TestCompleted(false, summary);
	}
}

void CTestAutoLayoutPreservation::Cancel()
{
	isRunning = false;
}
