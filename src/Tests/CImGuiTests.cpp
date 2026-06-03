#ifdef ENABLE_IMGUI_TEST_ENGINE

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "CViewC64.h"
#include "CViewBreakpoints.h"
#include "CDefaultWorkspaceLayouts.h"
#include "CViewDataDump.h"
#include "CViewDataMap.h"
#include "CViewDisassembly.h"
#include "CViewC64Screen.h"
#include "CViewC64StateCPU.h"
#include "CViewDrive1541StateCPU.h"
#include "CByteBuffer.h"
#include "CGuiMain.h"
#include "CLayoutManager.h"
#include "CDebugInterface.h"
#include "CDebugInterfaceC64.h"
#include "CDebugInterfaceAtari.h"
#include "CDebugInterfaceNes.h"
#include "CDebugSymbolsC64.h"
#include "CDebugSymbolsSegment.h"
#include "CDebugBreakpointsData.h"
#include "../DebugInterface/MenuItems/CDebugInterfaceMenuItem.h"
#include "../Emulators/c64u/C64UTestFixture.h"
#include "../Plugins/GoatTracker/C64DebuggerPluginGoatTracker.h"
#include "../Plugins/GoatTracker/CViewGT2Patterns.h"
#include "../Plugins/GoatTracker/CViewGT2Instrument.h"
#include "../Plugins/GoatTracker/CViewC64GoatTracker.h"
#include "SYS_Funct.h"
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

// GT2 native globals — only resolved when GoatTracker plugin is linked + active.
extern "C" {
#include "gcommon.h"
#include "ginstr.h"
#include "goattrk2.h"
	extern unsigned char *chardata;
	extern unsigned char pattern[208][128*4+4];
	extern int pattlen[208];
	extern int epnum[3];
	extern int eppos, epview, epcolumn, epchn;
	extern int editmode, recordmode, epoctave, eamode, menu;
	extern int einum, eipos, eicolumn;
	extern unsigned keypreset;
}

namespace
{
	struct SAutoLayoutWindowSnapshot
	{
		std::string name;
		ImVec2 pos;
		ImVec2 size;
		ImVec2 dockNodePos;
		ImVec2 dockNodeSize;
		bool inMainDockspace;
		std::string dockPath;
	};

	static bool ValueMatches(float a, float b, float tolerance = 8.0f, float pctTolerance = 0.10f)
	{
		float delta = fabsf(a - b);
		float maxValue = fmaxf(a, b);
		return (delta <= tolerance) || (maxValue > 0.0f && delta / maxValue <= pctTolerance);
	}

	static bool VecMatches(ImVec2 a, ImVec2 b, float tolerance = 8.0f, float pctTolerance = 0.10f)
	{
		return ValueMatches(a.x, b.x, tolerance, pctTolerance)
			&& ValueMatches(a.y, b.y, tolerance, pctTolerance);
	}

	static float SnapshotRight(const SAutoLayoutWindowSnapshot &snapshot)
	{
		return snapshot.dockNodePos.x + snapshot.dockNodeSize.x;
	}

	static float SnapshotBottom(const SAutoLayoutWindowSnapshot &snapshot)
	{
		return snapshot.dockNodePos.y + snapshot.dockNodeSize.y;
	}

	static bool NearlyAligned(float a, float b, float tolerance)
	{
		return fabsf(a - b) <= tolerance;
	}

	static ImGuiTable *FindActiveTableInWindow(ImGuiWindow *window, const char *tableId)
	{
		if (window == NULL || GImGui == NULL)
			return NULL;

		ImGuiContext &g = *GImGui;
		ImGuiTable *table = g.Tables.GetByKey(window->GetID(tableId));
		if (table == NULL || table->LastFrameActive < g.FrameCount - 1 || table->OuterWindow != window)
			return NULL;

		return table;
	}

	static std::string BuildDockPath(ImGuiDockNode *node)
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
	}

	static bool SnapshotWindow(CGuiView *view, SAutoLayoutWindowSnapshot &snapshot, std::string &details)
	{
		if (!view)
			return false;

		ImGuiWindow *foundWin = ImGui::FindWindowByName(view->name);
		ImGuiWindow *win = view->imGuiWindow ? view->imGuiWindow : foundWin;
		if (!win)
		{
			details += std::string(view->name) + ": window not found; ";
			return false;
		}

		if (win->DockId == 0)
		{
			char buf[384];
			snprintf(buf, sizeof(buf), "%s: window not docked visible=%d active=%d wasActive=%d hidden=%d ptr=%p found=%p foundDock=%08x; ",
					 view->name,
					 view->visible ? 1 : 0,
					 win->Active ? 1 : 0,
					 win->WasActive ? 1 : 0,
					 win->Hidden ? 1 : 0,
					 (void *)win,
					 (void *)foundWin,
					 foundWin != NULL ? foundWin->DockId : 0);
			details += buf;
			return false;
		}

		ImGuiDockNode *node = ImGui::DockBuilderGetNode(win->DockId);
		if (!node)
		{
			details += std::string(view->name) + ": dock node missing; ";
			return false;
		}

		snapshot.name = view->name;
		snapshot.pos = ImVec2(view->windowPosX, view->windowPosY);
		snapshot.size = ImVec2(view->windowSizeX, view->windowSizeY);
		snapshot.dockNodePos = node->Pos;
		snapshot.dockNodeSize = node->Size;
		snapshot.dockPath = BuildDockPath(node);

		ImGuiDockNode *root = node;
		while (root->ParentNode)
			root = root->ParentNode;
		snapshot.inMainDockspace = !root->IsFloatingNode();
		return true;
	}

	static void AppendSnapshotDetails(std::string &details, const char *viewId, const SAutoLayoutWindowSnapshot &snapshot)
	{
		char buf[256];
		snprintf(buf, sizeof(buf), "%s=(%.1f,%.1f %.1fx%.1f) ", viewId,
				 snapshot.dockNodePos.x, snapshot.dockNodePos.y,
				 snapshot.dockNodeSize.x, snapshot.dockNodeSize.y);
		details += buf;
	}

	static bool CheckC64DriveMemoryMapDockedGeometry(std::string &details)
	{
		if (viewC64 == NULL)
		{
			details += "viewC64 missing; ";
			return false;
		}

		SAutoLayoutWindowSnapshot c64Cpu;
		SAutoLayoutWindowSnapshot screen;
		SAutoLayoutWindowSnapshot driveCpu;
		SAutoLayoutWindowSnapshot c64Disassembly;
		SAutoLayoutWindowSnapshot driveDisassembly;
		SAutoLayoutWindowSnapshot c64MemoryMap;
		SAutoLayoutWindowSnapshot driveMemoryMap;

		bool snapshotsOk = true;
		snapshotsOk = SnapshotWindow(viewC64->viewC64StateCPU, c64Cpu, details) && snapshotsOk;
		snapshotsOk = SnapshotWindow(viewC64->viewC64Screen, screen, details) && snapshotsOk;
		snapshotsOk = SnapshotWindow(viewC64->viewDrive1541StateCPU, driveCpu, details) && snapshotsOk;
		snapshotsOk = SnapshotWindow(viewC64->viewC64Disassembly, c64Disassembly, details) && snapshotsOk;
		snapshotsOk = SnapshotWindow(viewC64->viewDrive1541Disassembly, driveDisassembly, details) && snapshotsOk;
		snapshotsOk = SnapshotWindow(viewC64->viewC64MemoryMap, c64MemoryMap, details) && snapshotsOk;
		snapshotsOk = SnapshotWindow(viewC64->viewDrive1541MemoryMap, driveMemoryMap, details) && snapshotsOk;
		if (!snapshotsOk)
			return false;

		AppendSnapshotDetails(details, "c64.cpu_state", c64Cpu);
		AppendSnapshotDetails(details, "c64.screen", screen);
		AppendSnapshotDetails(details, "drive1541.cpu_state", driveCpu);
		AppendSnapshotDetails(details, "c64.disassembly", c64Disassembly);
		AppendSnapshotDetails(details, "c64.memory_map", c64MemoryMap);
		AppendSnapshotDetails(details, "drive1541.memory_map", driveMemoryMap);
		AppendSnapshotDetails(details, "drive1541.disassembly", driveDisassembly);

		const float tolerance = 18.0f;
		bool topBandOk = NearlyAligned(c64Cpu.dockNodePos.y, screen.dockNodePos.y, tolerance)
			&& NearlyAligned(screen.dockNodePos.y, driveCpu.dockNodePos.y, tolerance)
			&& NearlyAligned(SnapshotBottom(c64Cpu), SnapshotBottom(driveCpu), tolerance)
			&& c64Cpu.dockNodePos.x <= c64Disassembly.dockNodePos.x + tolerance
			&& SnapshotRight(c64Cpu) <= screen.dockNodePos.x + tolerance
			&& SnapshotRight(screen) <= driveCpu.dockNodePos.x + tolerance
			&& SnapshotRight(driveCpu) >= SnapshotRight(driveDisassembly) - tolerance
			&& SnapshotBottom(c64Cpu) <= SnapshotBottom(screen) - tolerance
			&& SnapshotBottom(driveCpu) <= SnapshotBottom(screen) - tolerance;
		bool sideColumnsOk = NearlyAligned(c64Disassembly.dockNodePos.x, c64Cpu.dockNodePos.x, tolerance)
			&& c64Disassembly.dockNodePos.y >= SnapshotBottom(c64Cpu) - tolerance
			&& SnapshotRight(c64Disassembly) <= screen.dockNodePos.x + tolerance
			&& driveDisassembly.dockNodePos.y >= SnapshotBottom(driveCpu) - tolerance
			&& SnapshotRight(screen) <= driveDisassembly.dockNodePos.x + tolerance
			&& NearlyAligned(SnapshotRight(driveDisassembly), SnapshotRight(driveCpu), tolerance)
			&& SnapshotBottom(c64Disassembly) >= SnapshotBottom(c64MemoryMap) - tolerance
			&& SnapshotBottom(driveDisassembly) >= SnapshotBottom(driveMemoryMap) - tolerance;
		bool middleColumnOk = c64MemoryMap.dockNodePos.x >= SnapshotRight(c64Disassembly) - tolerance
			&& SnapshotRight(driveMemoryMap) <= driveDisassembly.dockNodePos.x + tolerance
			&& SnapshotRight(c64MemoryMap) <= driveMemoryMap.dockNodePos.x + tolerance
			&& screen.dockNodePos.x <= c64MemoryMap.dockNodePos.x + tolerance
			&& SnapshotRight(screen) >= SnapshotRight(driveMemoryMap) - tolerance
			&& SnapshotBottom(screen) <= c64MemoryMap.dockNodePos.y + tolerance
			&& SnapshotBottom(screen) <= driveMemoryMap.dockNodePos.y + tolerance
			&& NearlyAligned(c64MemoryMap.dockNodePos.y, driveMemoryMap.dockNodePos.y, tolerance);

		if (!topBandOk || !sideColumnsOk || !middleColumnOk)
		{
			char buf[256];
			snprintf(buf, sizeof(buf), "top=%d sides=%d middle=%d ",
					 topBandOk ? 1 : 0, sideColumnsOk ? 1 : 0,
					 middleColumnOk ? 1 : 0);
			details = std::string(buf) + details;
			return false;
		}

		return true;
	}

	static ImGuiDockNode *GetViewDockNode(CGuiView *view)
	{
		if (view == NULL)
			return NULL;

		ImGuiWindow *win = view->imGuiWindow ? view->imGuiWindow : ImGui::FindWindowByName(view->name);
		if (win == NULL || win->DockId == 0)
			return NULL;

		return ImGui::DockBuilderGetNode(win->DockId);
	}

	static bool ViewDockNodeHasNoTabBar(CGuiView *view)
	{
		ImGuiDockNode *node = GetViewDockNode(view);
		return node != NULL && (node->MergedFlags & ImGuiDockNodeFlags_NoTabBar) != 0;
	}

	static const SDefaultWorkspaceViewPlacementSpec *FindDefaultWorkspacePlacementForTest(const SDefaultWorkspaceLayoutSpec *layoutSpec, const char *viewId)
	{
		if (layoutSpec == NULL || layoutSpec->placements == NULL || viewId == NULL)
			return NULL;

		for (int i = 0; i < layoutSpec->numPlacements; i++)
		{
			if (strcmp(layoutSpec->placements[i].viewId, viewId) == 0)
				return &layoutSpec->placements[i];
		}

		return NULL;
	}

	static void CheckC64DataDumpNoTabBarState(bool expectedNoTabBar)
	{
		IM_CHECK(ViewDockNodeHasNoTabBar(viewC64->viewC64Screen) == expectedNoTabBar);
		IM_CHECK(ViewDockNodeHasNoTabBar(viewC64->viewC64MemoryDataDump) == expectedNoTabBar);

		if (expectedNoTabBar)
		{
			const SDefaultWorkspaceLayoutSpec *dataDumpSpec = FindDefaultWorkspaceLayoutSpec(DefaultWorkspacePlatform_C64, DefaultWorkspaceSlot_DataDump);
			const SDefaultWorkspaceViewPlacementSpec *dataDumpPlacement = FindDefaultWorkspacePlacementForTest(dataDumpSpec, "c64.data_dump");
			IM_CHECK(dataDumpPlacement != NULL);
			ImGuiViewport *viewport = ImGui::GetMainViewport();
			IM_CHECK(viewport != NULL);
			SDefaultWorkspaceViewPlacementSpec scaledDataDump = ScaleDefaultWorkspacePlacement(*dataDumpPlacement, viewport->WorkSize.x, viewport->WorkSize.y);
			IM_CHECK(viewC64->viewC64MemoryDataDump->windowInnterRectSizeY >= scaledDataDump.height - ImGui::GetFrameHeight() * 0.5f);
		}
	}

	static bool WaitForRunning(ImGuiTestContext *ctx, CDebugInterface *di, bool shouldBeRunning, int frames, const char *label)
	{
		for (int i = 0; i < frames; i++)
		{
			if (di->isRunning == shouldBeRunning)
			{
				LOGD("WaitForRunning[%s]: reached expected running=%d after %d frames", label, shouldBeRunning, i);
				return true;
			}
			if (i == 0 || ((i + 1) % 30) == 0)
				LOGD("WaitForRunning[%s]: frame=%d current running=%d expected=%d", label, i + 1, di->isRunning, shouldBeRunning);
			ctx->Yield();
		}
		LOGD("WaitForRunning[%s]: timed out after %d frames current running=%d expected=%d", label, frames, di->isRunning, shouldBeRunning);
		return di->isRunning == shouldBeRunning;
	}

	static CGuiView *FindViewByName(CDebugInterface *di, const char *name)
	{
		for (auto *view : di->views)
			if (view && !strcmp(view->name, name))
				return view;
		return NULL;
	}

	static bool ToggleEmulatorViaFileMenu(ImGuiTestContext *ctx, CDebugInterface *di, bool shouldBeRunning, const char *label)
	{
		if (di->isRunning == shouldBeRunning)
			return true;

		std::string escapedName = di->GetPlatformNameString();
		for (size_t pos = 0; (pos = escapedName.find('/', pos)) != std::string::npos; pos += 2)
			escapedName.replace(pos, 1, "\\/");

		std::string path = std::string("File/") + escapedName;
		LOGD("ToggleEmulatorViaFileMenu[%s]: click %s target running=%d current=%d", label, path.c_str(), shouldBeRunning, di->isRunning);
		ctx->MenuClick(path.c_str());
		return WaitForRunning(ctx, di, shouldBeRunning, shouldBeRunning ? 120 : 60, label);
	}
}

void RegisterRetroDebuggerTests(ImGuiTestEngine *engine)
{
	if (C64UTestFixture::IsEnabled())
	{
		ImGuiTest *tC64u = IM_REGISTER_TEST(engine, "ui", "c64u_fixture_menu_exposure");
		tC64u->TestFunc = [](ImGuiTestContext *ctx)
		{
			ctx->SetRef("##MainMenuBar");
			ctx->Yield(10);

			IM_CHECK(viewC64->debugInterfaceC64 != NULL);
			IM_CHECK(((CDebugInterface *)viewC64->debugInterfaceC64)->isRunning);
			IM_CHECK(viewC64->debugInterfaceC64U != NULL);
			IM_CHECK(((CDebugInterface *)viewC64->debugInterfaceC64U)->isRunning);

			auto hasMenuItem = [](CDebugInterface *debugInterface, const char *name)
			{
				for (std::list<CDebugInterfaceMenuItem *>::iterator it = debugInterface->menuItems.begin(); it != debugInterface->menuItems.end(); it++)
				{
					if (!strcmp((*it)->menuItemStr, name))
						return true;
				}
				return false;
			};

			ctx->MenuCheck("C64");
			ctx->MenuCheck("C64U");
			IM_CHECK(hasMenuItem((CDebugInterface *)viewC64->debugInterfaceC64U, "C64U Screen"));
			IM_CHECK(hasMenuItem((CDebugInterface *)viewC64->debugInterfaceC64U, "C64U Connection Status"));
			IM_CHECK(hasMenuItem((CDebugInterface *)viewC64->debugInterfaceC64U, "C64U Mode Status"));
			IM_CHECK(hasMenuItem((CDebugInterface *)viewC64->debugInterfaceC64U, "C64U Media Status"));
			IM_CHECK(hasMenuItem((CDebugInterface *)viewC64->debugInterfaceC64U, "C64U Disassembly"));
			IM_CHECK(hasMenuItem((CDebugInterface *)viewC64->debugInterfaceC64U, "C64U Memory"));
			IM_CHECK(hasMenuItem((CDebugInterface *)viewC64->debugInterfaceC64U, "C64U VIC State"));
			IM_CHECK(hasMenuItem((CDebugInterface *)viewC64->debugInterfaceC64U, "C64U CIA State"));
			IM_CHECK(hasMenuItem((CDebugInterface *)viewC64->debugInterfaceC64U, "C64U SID State"));
			IM_CHECK(hasMenuItem((CDebugInterface *)viewC64->debugInterfaceC64U, "C64U Memory Bank"));
			IM_CHECK(!hasMenuItem((CDebugInterface *)viewC64->debugInterfaceC64, "C64U Screen"));
		};

		return;
	}

	// Test: Verify main menu bar items exist
	ImGuiTest *t = IM_REGISTER_TEST(engine, "ui", "main_menu_bar_items");
		t->TestFunc = [](ImGuiTestContext *ctx)
		{
			ctx->SetRef("##MainMenuBar");
			ctx->MenuClick("File");
		ctx->MenuClick("Code");
		ctx->MenuClick("Workspace");
		ctx->MenuClick("Settings");
		ctx->MenuClick("Help");
	};

	ImGuiTest *tDefaultWorkspacesPopup = IM_REGISTER_TEST(engine, "ui", "default_workspaces_popup");
	tDefaultWorkspacesPopup->TestFunc = [](ImGuiTestContext *ctx)
	{
		ctx->SetRef("##MainMenuBar");
		ctx->Yield(5);
		ctx->MenuClick("Workspace/Auto Layout/Create Default Layouts...");
		ctx->Yield(3);

		ImGuiWindow *popup = ImGui::FindWindowByName("Create Default Layouts");
		IM_CHECK(popup != NULL && popup->Active);

		ctx->SetRef("Create Default Layouts");
		IM_CHECK(ctx->ItemExists("Locked default layouts"));
		IM_CHECK(ctx->ItemExists("Create context shortcuts"));
		IM_CHECK(ctx->ItemExists("No tab bars in generated layouts"));
		ImGuiTable *shortcutTable = FindActiveTableInWindow(popup, "##DefaultWorkspaceShortcutSlots");
		IM_CHECK(shortcutTable != NULL);
		if (shortcutTable != NULL)
		{
			IM_CHECK(shortcutTable->ColumnsCount == 6);
			IM_CHECK(strcmp(ImGui::TableGetColumnName(shortcutTable, 0), "Slot") == 0);
			IM_CHECK(strcmp(ImGui::TableGetColumnName(shortcutTable, 1), "Layout Role") == 0);
			IM_CHECK(strcmp(ImGui::TableGetColumnName(shortcutTable, 2), "Default Shortcut") == 0);
			IM_CHECK(strcmp(ImGui::TableGetColumnName(shortcutTable, 3), "Assigned Shortcut") == 0);
			IM_CHECK(strcmp(ImGui::TableGetColumnName(shortcutTable, 4), "Status") == 0);
			IM_CHECK(strcmp(ImGui::TableGetColumnName(shortcutTable, 5), "Actions") == 0);
			IM_CHECK(shortcutTable->CurrentRow >= (int)GetDefaultWorkspaceShortcutSlotSpecs().size());
		}
		IM_CHECK(ctx->ItemExists("Create"));
		IM_CHECK(ctx->ItemExists("Cancel"));
		IM_CHECK(ctx->ItemExists("Reset to defaults"));
		ImGuiTestItemInfo resetButton = ctx->ItemInfo("Reset to defaults");
		ImGuiTestItemInfo cancelButton = ctx->ItemInfo("Cancel");
		ImGuiTestItemInfo createButton = ctx->ItemInfo("Create");
		float footerTolerance = ImGui::GetStyle().ItemSpacing.x + 2.0f;
		IM_CHECK(resetButton.RectFull.Min.x <= popup->WorkRect.Min.x + footerTolerance);
		IM_CHECK(cancelButton.RectFull.Min.x > resetButton.RectFull.Max.x);
		IM_CHECK(createButton.RectFull.Min.x > cancelButton.RectFull.Max.x);
		IM_CHECK(popup->WorkRect.Max.x - createButton.RectFull.Max.x <= footerTolerance);
		IM_CHECK(fabsf(resetButton.RectFull.Min.y - cancelButton.RectFull.Min.y) <= 2.0f);
		IM_CHECK(fabsf(cancelButton.RectFull.Min.y - createButton.RectFull.Min.y) <= 2.0f);
		ctx->ItemClick("Cancel");
	};

	ImGuiTest *tDefaultWorkspacesCreate = IM_REGISTER_TEST(engine, "ui", "default_workspaces_create_and_menu");
	tDefaultWorkspacesCreate->TestFunc = [](ImGuiTestContext *ctx)
	{
		ctx->SetRef("##MainMenuBar");
		ctx->Yield(5);
		bool wasC64Running = viewC64->debugInterfaceC64 != NULL && viewC64->debugInterfaceC64->isRunning;
		bool wasAtariRunning = viewC64->debugInterfaceAtari != NULL && viewC64->debugInterfaceAtari->isRunning;
		bool wasNesRunning = viewC64->debugInterfaceNes != NULL && viewC64->debugInterfaceNes->isRunning;
		ctx->MenuClick("Workspace/Auto Layout/Create Default Layouts...");
		ctx->Yield(3);

		ctx->SetRef("Create Default Layouts");
		ctx->ItemClick("Create");
		ctx->Yield(260);
		IM_CHECK((viewC64->debugInterfaceC64 != NULL && viewC64->debugInterfaceC64->isRunning) == wasC64Running);
		IM_CHECK((viewC64->debugInterfaceAtari != NULL && viewC64->debugInterfaceAtari->isRunning) == wasAtariRunning);
		IM_CHECK((viewC64->debugInterfaceNes != NULL && viewC64->debugInterfaceNes->isRunning) == wasNesRunning);

		CLayoutData *c64DataDump = guiMain->layoutManager->GetPredefinedLayoutById("retro.default.c64.data_dump");
		CLayoutData *c64DriveMemoryMap = guiMain->layoutManager->GetPredefinedLayoutById("retro.default.c64.1541_memory_map");
		CLayoutData *atariDataDump = guiMain->layoutManager->GetPredefinedLayoutById("retro.default.atari800.data_dump");
		CLayoutData *nesDataDump = guiMain->layoutManager->GetPredefinedLayoutById("retro.default.nes.data_dump");
		IM_CHECK(c64DataDump != NULL);
		IM_CHECK(c64DriveMemoryMap != NULL);
		IM_CHECK(atariDataDump != NULL);
		IM_CHECK(nesDataDump != NULL);
		IM_CHECK(c64DataDump->serializedLayoutBuffer != NULL && c64DataDump->serializedLayoutBuffer->length > 0);
		IM_CHECK(c64DriveMemoryMap->serializedLayoutBuffer != NULL && c64DriveMemoryMap->serializedLayoutBuffer->length > 0);
		IM_CHECK(atariDataDump->serializedLayoutBuffer != NULL && atariDataDump->serializedLayoutBuffer->length > 0);
		IM_CHECK(nesDataDump->serializedLayoutBuffer != NULL && nesDataDump->serializedLayoutBuffer->length > 0);

		ctx->SetRef("##MainMenuBar");
		ctx->MenuClick("Workspace/C64/C64 Data Dump");
		ctx->Yield(20);
		IM_CHECK(guiMain->layoutManager->currentLayout == c64DataDump);

		CDebugInterface *diC64 = (CDebugInterface *)viewC64->debugInterfaceC64;
		IM_CHECK(diC64 != NULL);
		if (!diC64->isRunning)
			IM_CHECK(ToggleEmulatorViaFileMenu(ctx, diC64, true, "default-workspaces-start-c64"));
		ctx->Yield(20);
		CheckC64DataDumpNoTabBarState(true);
		guiMain->RequestAutoLayoutVisibleViewsDockedPreserveScan();
		ctx->Yield(20);
		CheckC64DataDumpNoTabBarState(true);

		ctx->SetRef("##MainMenuBar");
		ctx->MenuClick("Workspace/C64/C64 1541 Memory Map");
		ctx->Yield(30);
		IM_CHECK(guiMain->layoutManager->currentLayout == c64DriveMemoryMap);

		CViewDisassembly *c64Disassembly = dynamic_cast<CViewDisassembly *>(FindViewByName(diC64, "C64 Disassembly"));
		CViewDisassembly *driveDisassembly = dynamic_cast<CViewDisassembly *>(FindViewByName(diC64, "1541 Disassembly"));
		IM_CHECK(c64Disassembly != NULL);
		IM_CHECK(driveDisassembly != NULL);
		IM_CHECK(!c64Disassembly->showHexCodes && !c64Disassembly->showCodeCycles && !c64Disassembly->showLabels);
		IM_CHECK(!driveDisassembly->showHexCodes && !driveDisassembly->showCodeCycles && !driveDisassembly->showLabels);

		SAutoLayoutWindowSnapshot c64DisassemblySnapshot;
		SAutoLayoutWindowSnapshot driveDisassemblySnapshot;
		std::string snapshotDetails;
		IM_CHECK(SnapshotWindow(c64Disassembly, c64DisassemblySnapshot, snapshotDetails));
		IM_CHECK(SnapshotWindow(driveDisassembly, driveDisassemblySnapshot, snapshotDetails));
		std::string geometryDetails;
		bool geometryOk = CheckC64DriveMemoryMapDockedGeometry(geometryDetails);
		if (!geometryOk)
		{
			IM_ERRORF("C64 1541 Memory Map docked geometry mismatch: %s", geometryDetails.c_str());
		}

		if (diC64->isRunning != wasC64Running)
			IM_CHECK(ToggleEmulatorViaFileMenu(ctx, diC64, wasC64Running, "default-workspaces-restore-c64"));
		if (!geometryOk)
			return;
	};

	ImGuiTest *tDefaultWorkspacesCreateWithTabBars = IM_REGISTER_TEST(engine, "ui", "default_workspaces_create_with_tab_bars");
	tDefaultWorkspacesCreateWithTabBars->TestFunc = [](ImGuiTestContext *ctx)
	{
		ctx->SetRef("##MainMenuBar");
		ctx->Yield(5);
		bool wasC64Running = viewC64->debugInterfaceC64 != NULL && viewC64->debugInterfaceC64->isRunning;
		bool wasAtariRunning = viewC64->debugInterfaceAtari != NULL && viewC64->debugInterfaceAtari->isRunning;
		bool wasNesRunning = viewC64->debugInterfaceNes != NULL && viewC64->debugInterfaceNes->isRunning;

		ctx->MenuClick("Workspace/Auto Layout/Create Default Layouts...");
		ctx->Yield(3);
		ctx->SetRef("Create Default Layouts");
		ctx->ItemClick("No tab bars in generated layouts");
		ctx->ItemClick("Create");
		ctx->Yield(260);
		IM_CHECK((viewC64->debugInterfaceC64 != NULL && viewC64->debugInterfaceC64->isRunning) == wasC64Running);
		IM_CHECK((viewC64->debugInterfaceAtari != NULL && viewC64->debugInterfaceAtari->isRunning) == wasAtariRunning);
		IM_CHECK((viewC64->debugInterfaceNes != NULL && viewC64->debugInterfaceNes->isRunning) == wasNesRunning);

		CLayoutData *c64DataDump = guiMain->layoutManager->GetPredefinedLayoutById("retro.default.c64.data_dump");
		IM_CHECK(c64DataDump != NULL);
		IM_CHECK(c64DataDump->serializedLayoutBuffer != NULL && c64DataDump->serializedLayoutBuffer->length > 0);

		ctx->SetRef("##MainMenuBar");
		ctx->MenuClick("Workspace/C64/C64 Data Dump");
		ctx->Yield(20);
		IM_CHECK(guiMain->layoutManager->currentLayout == c64DataDump);

		CDebugInterface *diC64 = (CDebugInterface *)viewC64->debugInterfaceC64;
		IM_CHECK(diC64 != NULL);
		if (!diC64->isRunning)
			IM_CHECK(ToggleEmulatorViaFileMenu(ctx, diC64, true, "default-workspaces-tab-bars-start-c64"));
		ctx->Yield(20);
		CheckC64DataDumpNoTabBarState(false);
		guiMain->RequestAutoLayoutVisibleViewsDockedPreserveScan();
		ctx->Yield(20);
		CheckC64DataDumpNoTabBarState(false);
		if (diC64->isRunning != wasC64Running)
			IM_CHECK(ToggleEmulatorViaFileMenu(ctx, diC64, wasC64Running, "default-workspaces-tab-bars-restore-c64"));
	};

	// Test: Open all views from all emulators to catch rendering crashes
	ImGuiTest *t2 = IM_REGISTER_TEST(engine, "ui", "open_all_views");
	t2->TestFunc = [](ImGuiTestContext *ctx)
	{
		// 1. Save initial emulator running states
		std::vector<bool> wasRunning;
		for (auto *di : viewC64->debugInterfaces)
			wasRunning.push_back(di->isRunning);

		// 2. Start all emulators that aren't running
		for (auto *di : viewC64->debugInterfaces)
		{
			if (!di->isRunning)
				viewC64->StartEmulationThread(di);
		}

		// Wait for emulators to initialize
		ctx->Yield(30);

		// 3. For each emulator, show each view, render frames, hide view
		for (auto *di : viewC64->debugInterfaces)
		{
			if (!di->isRunning) continue;

			for (auto *view : di->views)
			{
				bool wasVisible = view->visible;
				view->SetVisible(true);
				ctx->Yield(5);  // render 5 frames to catch crashes
				view->SetVisible(wasVisible);  // restore original visibility
			}
		}

		// 4. Restore emulator running states
		for (size_t i = 0; i < viewC64->debugInterfaces.size(); i++)
		{
			CDebugInterface *di = viewC64->debugInterfaces[i];
			if (di->isRunning && !wasRunning[i])
				viewC64->StopEmulationThread(di);
		}

			ctx->Yield(10);  // let cleanup settle
		};

	ImGuiTest *tMemBpEnter = IM_REGISTER_TEST(engine, "ui", "memory_breakpoint_enter_creates");
	tMemBpEnter->TestFunc = [](ImGuiTestContext *ctx)
	{
		ctx->SetRef("##MainMenuBar");
		ctx->Yield(20);

		CDebugInterfaceC64 *diC64 = (CDebugInterfaceC64 *)viewC64->debugInterfaceC64;
		IM_CHECK(diC64 != NULL);
		IM_CHECK(diC64->symbolsC64 != NULL);
		IM_CHECK(diC64->symbolsC64->currentSegment != NULL);
		IM_CHECK(viewC64->viewC64BreakpointsMemory != NULL);

		CDebugSymbolsSegment *segment = diC64->symbolsC64->currentSegment;
		CDebugBreakpointsData *breakpoints = segment->breakpointsData;
		IM_CHECK(breakpoints != NULL);

		const int testAddr = 0xCE00;
		breakpoints->DeleteBreakpoint(testAddr);
		breakpoints->UpdateRenderBreakpoints();

		bool wasVisible = viewC64->viewC64BreakpointsMemory->visible;
		viewC64->viewC64BreakpointsMemory->SetVisible(true);
		ctx->Yield(5);

		ctx->SetRef("C64 Memory Breakpoints");
		ctx->ItemClick("+");
		ctx->Yield(2);

		ctx->SetRef("//$FOCUSED");
		ctx->ItemInput("##addMemBreakpointPopupAddress_Vice");
		ctx->KeyCharsReplace("ce00");
		ctx->KeyPress(ImGuiKey_Enter);
		ctx->Yield(3);

		bool created = breakpoints->GetBreakpoint(testAddr) != NULL;
		if (created)
			breakpoints->DeleteBreakpoint(testAddr);
		breakpoints->UpdateRenderBreakpoints();
		viewC64->viewC64BreakpointsMemory->SetVisible(wasVisible);

		IM_CHECK(created);
	};

	ImGuiTest *t3 = IM_REGISTER_TEST(engine, "ui", "autolayout_preserve_scan_fixture");
	t3->TestFunc = [](ImGuiTestContext *ctx)
	{
		LOGD("autolayout_preserve_scan_fixture: entered test body");
		ctx->SetRef("##MainMenuBar");
		ctx->Yield(20);
		LOGD("autolayout_preserve_scan_fixture: after initial yield");

			CDebugInterface *diC64 = (CDebugInterface *)viewC64->debugInterfaceC64;
			CDebugInterface *diAtari = (CDebugInterface *)viewC64->debugInterfaceAtari;
		IM_CHECK(diC64 != NULL);
		IM_CHECK(diAtari != NULL);

		bool c64WasRunning = diC64->isRunning;
		bool atariWasRunning = diAtari->isRunning;

		auto restoreStates = [&]()
		{
			LOGD("autolayout_preserve_scan_fixture: restoring original emulator states");
			if (diC64->isRunning && !c64WasRunning)
			{
				ToggleEmulatorViaFileMenu(ctx, diC64, false, "restore-stop-c64");
			}
			else if (!diC64->isRunning && c64WasRunning)
			{
				ToggleEmulatorViaFileMenu(ctx, diC64, true, "restore-start-c64");
				ctx->Yield(20);
			}

			if (diAtari->isRunning && !atariWasRunning)
			{
				ToggleEmulatorViaFileMenu(ctx, diAtari, false, "restore-stop-atari");
			}
			else if (!diAtari->isRunning && atariWasRunning)
			{
				ToggleEmulatorViaFileMenu(ctx, diAtari, true, "restore-start-atari");
				ctx->Yield(20);
			}
		};

		auto failWithRestore = [&](const char *message, const std::string &details)
		{
			ctx->LogInfo("%s", message);
			if (!details.empty())
				ctx->LogInfo("%s", details.c_str());
			restoreStates();
			IM_CHECK(false);
		};

		if (diC64->isRunning)
		{
			LOGD("autolayout_preserve_scan_fixture: stopping initial C64");
			if (!ToggleEmulatorViaFileMenu(ctx, diC64, false, "initial-stop-c64"))
			{
				failWithRestore("Failed to stop C64 before fixture capture", "");
				return;
			}
		}
		if (diAtari->isRunning)
		{
			LOGD("autolayout_preserve_scan_fixture: stopping initial Atari");
			if (!ToggleEmulatorViaFileMenu(ctx, diAtari, false, "initial-stop-atari"))
			{
				failWithRestore("Failed to stop Atari before fixture capture", "");
				return;
			}
		}

		LOGD("autolayout_preserve_scan_fixture: starting C64 for fixture capture");
		if (!ToggleEmulatorViaFileMenu(ctx, diC64, true, "fixture-start-c64"))
		{
			failWithRestore("Failed to start C64 from fixture layout", "");
			return;
		}
		ctx->Yield(30);
		LOGD("autolayout_preserve_scan_fixture: verifying fixture windows");

		const char *requiredFixtureWindows[] = {
			"C64 Screen",
			"C64 CPU",
			"C64 Disassembly",
			"C64 Memory",
			"C64 Memory map",
			"C64 Timeline"
		};
		for (const char *name : requiredFixtureWindows)
		{
			CGuiView *view = FindViewByName(diC64, name);
			if (view == NULL || view->imGuiWindow == NULL || view->imGuiWindow->DockId == 0)
			{
				failWithRestore("Fixture verification failed before capture", std::string(name) + ": expected visible docked fixture window");
				return;
			}
		}

		std::vector<SAutoLayoutWindowSnapshot> c64Snapshots;
		std::string captureDetails;
		for (auto *view : diC64->views)
		{
			if (!view || !view->visible)
				continue;
			SAutoLayoutWindowSnapshot snapshot;
			if (SnapshotWindow(view, snapshot, captureDetails))
				c64Snapshots.push_back(snapshot);
		}
		if (c64Snapshots.empty())
		{
			failWithRestore("Could not capture any visible docked C64 fixture windows", captureDetails);
			return;
		}
		LOGD("autolayout_preserve_scan_fixture: captured %d docked C64 windows", (int)c64Snapshots.size());

		LOGD("autolayout_preserve_scan_fixture: stopping C64 before Atari autolayout");
		if (!ToggleEmulatorViaFileMenu(ctx, diC64, false, "pre-atari-stop-c64"))
		{
			failWithRestore("Failed to stop C64 before Atari autolayout", "");
			return;
		}

		LOGD("autolayout_preserve_scan_fixture: starting Atari before autolayout");
		if (!ToggleEmulatorViaFileMenu(ctx, diAtari, true, "start-atari"))
		{
			failWithRestore("Failed to start Atari before autolayout", "");
			return;
		}
		ctx->Yield(30);

		LOGD("autolayout_preserve_scan_fixture: requesting autolayout preserve scan");
		guiMain->RequestAutoLayoutVisibleViewsDockedPreserveScan();
		ctx->Yield(20);

		LOGD("autolayout_preserve_scan_fixture: stopping Atari after autolayout");
		if (!ToggleEmulatorViaFileMenu(ctx, diAtari, false, "post-autolayout-stop-atari"))
		{
			failWithRestore("Failed to stop Atari after autolayout", "");
			return;
		}

		LOGD("autolayout_preserve_scan_fixture: restarting C64 for comparison");
		if (!ToggleEmulatorViaFileMenu(ctx, diC64, true, "restart-c64"))
		{
			failWithRestore("Failed to restart C64 after Atari autolayout", "");
			return;
		}
		ctx->Yield(30);
		LOGD("autolayout_preserve_scan_fixture: comparing restored C64 windows");

		std::string failDetails;
		for (const SAutoLayoutWindowSnapshot &snapshot : c64Snapshots)
		{
				CGuiView *view = FindViewByName(diC64, snapshot.name.c_str());
			if (view == NULL)
			{
				failDetails += snapshot.name + ": layout view not found; ";
				continue;
			}

			SAutoLayoutWindowSnapshot currentSnapshot;
			std::string currentDetails;
			if (!SnapshotWindow(view, currentSnapshot, currentDetails))
			{
				failDetails += snapshot.name + ": no longer docked or window missing; ";
				continue;
			}

			if (!VecMatches(snapshot.pos, currentSnapshot.pos))
			{
				char buf[256];
				snprintf(buf, sizeof(buf), "%s: position mismatch (was %.0fx%.0f, now %.0fx%.0f); ",
						snapshot.name.c_str(), snapshot.pos.x, snapshot.pos.y, currentSnapshot.pos.x, currentSnapshot.pos.y);
				failDetails += buf;
				continue;
			}

			if (!VecMatches(snapshot.size, currentSnapshot.size))
			{
				char buf[256];
				snprintf(buf, sizeof(buf), "%s: size mismatch (was %.0fx%.0f, now %.0fx%.0f); ",
						snapshot.name.c_str(), snapshot.size.x, snapshot.size.y, currentSnapshot.size.x, currentSnapshot.size.y);
				failDetails += buf;
				continue;
			}

			if (snapshot.inMainDockspace != currentSnapshot.inMainDockspace)
			{
				failDetails += snapshot.name + ": dock root type changed; ";
				continue;
			}

			if (snapshot.dockPath != currentSnapshot.dockPath)
			{
				failDetails += snapshot.name + ": dock path changed (was " + snapshot.dockPath + ", now " + currentSnapshot.dockPath + "); ";
				continue;
			}
		}

		if (!failDetails.empty())
		{
			failWithRestore("Autolayout preserve-scan corrupted hidden C64 fixture windows", failDetails);
			return;
		}

		restoreStates();
		LOGD("autolayout_preserve_scan_fixture: success");
		ctx->LogInfo("Preserved %d C64 fixture windows after Atari autolayout", (int)c64Snapshots.size());
	};

	ImGuiTest *t4 = IM_REGISTER_TEST(engine, "ui", "autolayout_preserve_scan_fixture_atari");
	t4->TestFunc = [](ImGuiTestContext *ctx)
	{
		LOGD("autolayout_preserve_scan_fixture_atari: entered test body");
		ctx->SetRef("##MainMenuBar");
		ctx->Yield(20);

		CDebugInterface *diC64 = (CDebugInterface *)viewC64->debugInterfaceC64;
		CDebugInterface *diAtari = (CDebugInterface *)viewC64->debugInterfaceAtari;
		IM_CHECK(diC64 != NULL);
		IM_CHECK(diAtari != NULL);

		bool c64WasRunning = diC64->isRunning;
		bool atariWasRunning = diAtari->isRunning;

		auto restoreStates = [&]()
		{
			LOGD("autolayout_preserve_scan_fixture_atari: restoring original emulator states");
			if (diC64->isRunning && !c64WasRunning)
				ToggleEmulatorViaFileMenu(ctx, diC64, false, "atari-restore-stop-c64");
			else if (!diC64->isRunning && c64WasRunning)
			{
				ToggleEmulatorViaFileMenu(ctx, diC64, true, "atari-restore-start-c64");
				ctx->Yield(20);
			}

			if (diAtari->isRunning && !atariWasRunning)
				ToggleEmulatorViaFileMenu(ctx, diAtari, false, "atari-restore-stop-atari");
			else if (!diAtari->isRunning && atariWasRunning)
			{
				ToggleEmulatorViaFileMenu(ctx, diAtari, true, "atari-restore-start-atari");
				ctx->Yield(20);
			}
		};

		auto failWithRestore = [&](const char *message, const std::string &details)
		{
			ctx->LogInfo("%s", message);
			if (!details.empty())
				ctx->LogInfo("%s", details.c_str());
			restoreStates();
			IM_CHECK(false);
		};

		if (diC64->isRunning)
		{
			if (!ToggleEmulatorViaFileMenu(ctx, diC64, false, "atari-initial-stop-c64"))
			{
				failWithRestore("Failed to stop C64 before Atari fixture capture", "");
				return;
			}
		}
		if (diAtari->isRunning)
		{
			if (!ToggleEmulatorViaFileMenu(ctx, diAtari, false, "atari-initial-stop-atari"))
			{
				failWithRestore("Failed to stop Atari before fixture capture", "");
				return;
			}
		}

		if (!ToggleEmulatorViaFileMenu(ctx, diAtari, true, "atari-fixture-start-atari"))
		{
			failWithRestore("Failed to start Atari from fixture layout", "");
			return;
		}
		ctx->Yield(30);

		const char *requiredFixtureWindows[] = {
			"Atari Screen",
			"Atari CPU",
			"Atari Disassembly",
			"Atari Memory",
			"Atari Memory map",
			"Atari Timeline"
		};
		for (const char *name : requiredFixtureWindows)
		{
			CGuiView *view = FindViewByName(diAtari, name);
			if (view == NULL || view->imGuiWindow == NULL || view->imGuiWindow->DockId == 0)
			{
				failWithRestore("Atari fixture verification failed before capture", std::string(name) + ": expected visible docked fixture window");
				return;
			}
		}

		std::vector<SAutoLayoutWindowSnapshot> atariSnapshots;
		std::string captureDetails;
		for (auto *view : diAtari->views)
		{
			if (!view || !view->visible)
				continue;
			SAutoLayoutWindowSnapshot snapshot;
			if (SnapshotWindow(view, snapshot, captureDetails))
				atariSnapshots.push_back(snapshot);
		}
		if (atariSnapshots.empty())
		{
			failWithRestore("Could not capture any visible docked Atari fixture windows", captureDetails);
			return;
		}

		if (!ToggleEmulatorViaFileMenu(ctx, diAtari, false, "atari-pre-c64-stop-atari"))
		{
			failWithRestore("Failed to stop Atari before C64 autolayout", "");
			return;
		}

		if (!ToggleEmulatorViaFileMenu(ctx, diC64, true, "atari-start-c64"))
		{
			failWithRestore("Failed to start C64 before autolayout", "");
			return;
		}
		ctx->Yield(30);

		guiMain->RequestAutoLayoutVisibleViewsDockedPreserveScan();
		ctx->Yield(20);

		if (!ToggleEmulatorViaFileMenu(ctx, diC64, false, "atari-post-autolayout-stop-c64"))
		{
			failWithRestore("Failed to stop C64 after autolayout", "");
			return;
		}

		if (!ToggleEmulatorViaFileMenu(ctx, diAtari, true, "atari-restart-atari"))
		{
			failWithRestore("Failed to restart Atari after C64 autolayout", "");
			return;
		}
		ctx->Yield(30);

		std::string failDetails;
		for (const SAutoLayoutWindowSnapshot &snapshot : atariSnapshots)
		{
			CGuiView *view = FindViewByName(diAtari, snapshot.name.c_str());
			if (view == NULL)
			{
				failDetails += snapshot.name + ": layout view not found; ";
				continue;
			}

			SAutoLayoutWindowSnapshot currentSnapshot;
			std::string currentDetails;
			if (!SnapshotWindow(view, currentSnapshot, currentDetails))
			{
				failDetails += snapshot.name + ": no longer docked or window missing; ";
				continue;
			}

			if (!VecMatches(snapshot.pos, currentSnapshot.pos))
			{
				char buf[256];
				snprintf(buf, sizeof(buf), "%s: position mismatch (was %.0fx%.0f, now %.0fx%.0f); ",
						snapshot.name.c_str(), snapshot.pos.x, snapshot.pos.y, currentSnapshot.pos.x, currentSnapshot.pos.y);
				failDetails += buf;
				continue;
			}

			if (!VecMatches(snapshot.size, currentSnapshot.size))
			{
				char buf[256];
				snprintf(buf, sizeof(buf), "%s: size mismatch (was %.0fx%.0f, now %.0fx%.0f); ",
						snapshot.name.c_str(), snapshot.size.x, snapshot.size.y, currentSnapshot.size.x, currentSnapshot.size.y);
				failDetails += buf;
				continue;
			}

			if (snapshot.inMainDockspace != currentSnapshot.inMainDockspace)
			{
				failDetails += snapshot.name + ": dock root type changed; ";
				continue;
			}

			if (snapshot.dockPath != currentSnapshot.dockPath)
			{
				failDetails += snapshot.name + ": dock path changed (was " + snapshot.dockPath + ", now " + currentSnapshot.dockPath + "); ";
				continue;
			}
		}

		if (!failDetails.empty())
		{
			failWithRestore("Autolayout preserve-scan corrupted hidden Atari fixture windows", failDetails);
			return;
		}

		restoreStates();
		ctx->LogInfo("Preserved %d Atari fixture windows after C64 autolayout", (int)atariSnapshots.size());
	};

	// ui/data_map_show_scaling
	//
	// Reproducer for: when the C64 Memory Map view (CViewDataMap) is shown
	// from the menu after the active layout did NOT contain it, the window
	// appears at (roughly) full size but the rendered map texture is tiny in
	// a corner until the user manually resizes the window. The map's render
	// geometry (renderMapPos*/renderMapSize*) is derived once by
	// UpdateMapPosition() and only recomputed when CGuiView::PreRenderImGui
	// detects a geometry delta. This asserts the rendered map fills the live
	// ImGui window (its InnerRect) after show, not a stale rectangle.
	ImGuiTest *tDataMapScale = IM_REGISTER_TEST(engine, "ui", "data_map_show_scaling");
	tDataMapScale->TestFunc = [](ImGuiTestContext *ctx)
	{
		ctx->SetRef("##MainMenuBar");
		ctx->Yield(10);

		CDebugInterface *diC64 = (CDebugInterface *)viewC64->debugInterfaceC64;
		IM_CHECK(diC64 != NULL);
		bool wasC64Running = diC64->isRunning;
		if (!diC64->isRunning)
			IM_CHECK(ToggleEmulatorViaFileMenu(ctx, diC64, true, "datamap-start-c64"));
		ctx->Yield(20);

		CViewDataMap *map = viewC64->viewC64MemoryMap;
		IM_CHECK(map != NULL);

		// Show it via the real menu path and let the window materialise.
		bool wasVisible = map->visible;
		map->SetVisible(false);
		ctx->Yield(10);
		ctx->SetRef("##MainMenuBar");
		ctx->MenuClick("C64/C64 Memory map");
		ctx->Yield(10);

		IM_CHECK(map->imGuiWindow != NULL);

		// The authoritative geometry is the ImGui window's InnerRect — the
		// same rectangle CGuiView::PreRenderImGui pushes into the view via
		// SetPosition (size = InnerRect - 1). The bug: when the map is shown
		// after a layout that did not contain it, posX/sizeX (and hence the
		// rendered map rect + clip rect) stay at the stale rectangle a prior
		// layout restore pushed in via SetPosition, while the real window is
		// a different size — so the map renders tiny in a corner until a
		// manual resize forces CGuiView's delta gate to resync.
		auto realWinSizeX = [&]() { return map->imGuiWindow->InnerRect.GetSize().x - 1.0f; };
		auto realWinSizeY = [&]() { return map->imGuiWindow->InnerRect.GetSize().y - 1.0f; };
		auto realWinPosX  = [&]() { return map->imGuiWindow->InnerRect.Min.x; };
		auto realWinPosY  = [&]() { return map->imGuiWindow->InnerRect.Min.y; };

		// Give the view a few frames to settle after being shown.
		ctx->Yield(6);

		// The rendered map must fill the real window.
		float rwSizeX = realWinSizeX();
		float rwSizeY = realWinSizeY();
		float rwPosX = realWinPosX();
		float rwPosY = realWinPosY();
		bool fillsView =
			ValueMatches(map->renderMapSizeX, rwSizeX, 2.0f, 0.02f)
			&& ValueMatches(map->renderMapSizeY, rwSizeY, 2.0f, 0.02f)
			&& ValueMatches(map->renderMapPosX, rwPosX, 2.0f, 0.02f)
			&& ValueMatches(map->renderMapPosY, rwPosY, 2.0f, 0.02f);
		if (!fillsView)
		{
			ctx->LogError("CViewDataMap render geometry does not fill window: realWin=%.1f,%.1f %.1fx%.1f renderMapPos=%.1f,%.1f renderMapSize=%.1f,%.1f",
						  rwPosX, rwPosY, rwSizeX, rwSizeY,
						  map->renderMapPosX, map->renderMapPosY,
						  map->renderMapSizeX, map->renderMapSizeY);
		}

		map->SetVisible(wasVisible);

		if (diC64->isRunning != wasC64Running)
			ToggleEmulatorViaFileMenu(ctx, diC64, wasC64Running, "datamap-restore-c64");

		IM_CHECK(fillsView);
	};

	// gt2/dock_focus_keyboard
	//
	// Reproducer for: after Shift+drop dock of "GT2 Instrument" onto
	// "GT2 Patterns" (so they become tabs in one dock node), keyboard
	// input stops reaching the active GT2 view. Two symptoms the user
	// saw on real input: (a) note keys typed in the pattern grid don't
	// write to pattern[], (b) hex digits typed in the instrument
	// editor's attack/decay field don't update ginstr[].ad.
	//
	// What this test actually exercises
	// ---------------------------------
	// The test programmatically reproduces the dock layout via
	// ImGui::DockBuilderDockWindow and then drives ImGui's NavWindow to
	// the dock HOST window (the same state Shift+drop leaves behind in
	// the real app). That is the precise scenario CGuiView::PreRenderImGui
	// previously failed to detect: the strict IsWindowFocused() check
	// returns false for the visible tab because NavWindow points at the
	// host, not the docked view, so focusedView never advances and the
	// next guiMain->KeyDown drops the key.
	//
	// We then call guiMain->KeyDown() directly (rather than using the
	// imgui_test_engine's ctx->KeyPress, which only feeds ImGui's own
	// input queue and bypasses the engine focus dispatch path we care
	// about) and assert pattern[] / ginstr[].ad changed.
	//
	// Skip rules
	// ----------
	// GT2 plugin is required. If GoatTracker isn't compiled in or hasn't
	// been initialised yet, the test calls PLUGIN_GoatTrackerInit(); if
	// chardata is still NULL after that, the engine genuinely isn't
	// available and we log + return rather than fail.
	ImGuiTest *tGT2Dock = IM_REGISTER_TEST(engine, "gt2", "dock_focus_keyboard");
	tGT2Dock->TestFunc = [](ImGuiTestContext *ctx)
	{
		// Skip cleanly if GT2 plugin isn't already initialised in this
		// session. Activating it from the test thread races with the
		// C64 thread and either deadlocks or crashes inside chardata
		// init, so the test requires the user to have GT2 visible in
		// the workspace before running it (or to use a layouts fixture
		// where it is). A skip is the right outcome here, NOT a fail.
		if (pluginGoatTracker == NULL
			|| pluginGoatTracker->viewPatterns == NULL
			|| pluginGoatTracker->viewInstrument == NULL
			|| chardata == NULL)
		{
			ctx->LogInfo("GT2 plugin not initialised — skipped (activate it via Plugins menu, or save a layouts.dat with GT2 visible and pass --layouts-fixture)");
			return;
		}

		// User instruction: stop C64 while GT2 is active so the SID and
		// chardata pointers are not racing with the VICE thread.
		bool wasC64Running = (viewC64->debugInterfaceC64 != NULL
		                      && viewC64->debugInterfaceC64->isRunning);
		if (wasC64Running)
		{
			viewC64->StopEmulationThread(viewC64->debugInterfaceC64);
			ctx->Yield(30);
		}
		auto restoreC64 = [&]()
		{
			if (wasC64Running && viewC64->debugInterfaceC64 != NULL
			    && !viewC64->debugInterfaceC64->isRunning)
			{
				viewC64->StartEmulationThread(viewC64->debugInterfaceC64);
				ctx->Yield(30);
			}
		};

		CViewGT2Patterns *patterns = pluginGoatTracker->viewPatterns;
		CViewGT2Instrument *instrument = pluginGoatTracker->viewInstrument;

		patterns->SetVisible(true);
		instrument->SetVisible(true);
		ctx->Yield(10);

		if (patterns->imGuiWindow == NULL || instrument->imGuiWindow == NULL)
		{
			ctx->LogError("GT2 views did not materialise an ImGui window");
			restoreC64();
			IM_CHECK(false);
			return;
		}

		IM_CHECK(patterns->imGuiWindow != NULL);
		IM_CHECK(instrument->imGuiWindow != NULL);

		// Snapshot dock state we will restore at the end so this test
		// doesn't permanently rearrange the user's layout when run on
		// their real layouts.dat.
		ImGuiID origPatternsDock   = patterns->imGuiWindow->DockId;
		ImGuiID origInstrumentDock = instrument->imGuiWindow->DockId;

		// Force a fresh shared dock node so the test result doesn't
		// depend on whatever pre-existing layout the user shipped.
		ImGuiID sharedNodeId = ImGui::DockBuilderAddNode(0, ImGuiDockNodeFlags_None);
		ImGui::DockBuilderSetNodePos(sharedNodeId, ImVec2(200.0f, 200.0f));
		ImGui::DockBuilderSetNodeSize(sharedNodeId, ImVec2(800.0f, 600.0f));
		ImGui::DockBuilderDockWindow(patterns->imGuiWindow->Name,   sharedNodeId);
		ImGui::DockBuilderDockWindow(instrument->imGuiWindow->Name, sharedNodeId);
		ImGui::DockBuilderFinish(sharedNodeId);
		ctx->Yield(8);

		// After the dock builder commits and ImGui has had a few frames
		// to lay the node out, both views must share the same DockId and
		// the node must have a host window.
		IM_CHECK_EQ(patterns->imGuiWindow->DockId, instrument->imGuiWindow->DockId);
		ImGuiDockNode *node = patterns->imGuiWindow->DockNode;
		IM_CHECK(node != NULL);
		IM_CHECK(node->HostWindow != NULL);

		// --- Case A: Patterns is the active tab ---
		// Force "GT2 Patterns" to be the visible tab in the shared dock
		// node, then park ImGui's NavWindow on the dock host (this is
		// what Shift+drop leaves behind in the real app). My
		// CGuiView::PreRenderImGui dock-host fallback should now route
		// focusedView to the visible tab (= patterns).
		if (node->TabBar)
			node->TabBar->NextSelectedTabId = patterns->imGuiWindow->TabId;
		node->SelectedTabId = patterns->imGuiWindow->TabId;
		ctx->Yield(3);

		ImGui::FocusWindow(node->HostWindow);
		ctx->Yield(2);

		if (guiMain->focusedView != (CGuiView *)patterns)
		{
			ctx->LogError("Patterns active tab + NavWindow on dock host: focusedView=%s expected=%s",
			              guiMain->focusedView ? guiMain->focusedView->name : "NULL",
			              patterns->name);
			IM_CHECK(false);
		}

		// Now drive a note key through guiMain->KeyDown and verify that
		// HandleMainTrackNoteEntry wrote into pattern[]. Save state so
		// we can restore it on exit (the test is meant to be non-
		// destructive to the user's loaded song).
		int savedEditmode  = editmode;
		int savedRecord    = recordmode;
		int savedEppos     = eppos;
		int savedEpchn     = epchn;
		int savedEpcolumn  = epcolumn;
		int savedKeypreset = (int)keypreset;
		int savedEparpcol  = patterns->eparpcol;
		bool savedEpInSus  = patterns->epInSustain;

		int writeRow = eppos;
		int writeCh  = epchn;
		int writePatt = epnum[writeCh];
		unsigned char savedNoteByte  = pattern[writePatt][writeRow * 4 + 0];
		unsigned char savedInstrByte = pattern[writePatt][writeRow * 4 + 1];

		editmode   = EDIT_PATTERN;
		recordmode = 1;
		epcolumn   = 0;
		patterns->eparpcol  = -1;
		patterns->epInSustain = false;
		// Use the KEY_TRACKER preset for 'q' = lower-octave C. (KEY_RENOISE
		// note tables use different ASCII keys.) Either way the route
		// being tested is the engine focus dispatch, not the keymap.
		keypreset = KEY_TRACKER;

		guiMain->isShiftPressed   = false;
		guiMain->isAltPressed     = false;
		guiMain->isControlPressed = false;
		guiMain->isSuperPressed   = false;
		guiMain->KeyDown('q');

		bool patternsWrote = (pattern[writePatt][writeRow * 4 + 0] != savedNoteByte
		                      || pattern[writePatt][writeRow * 4 + 1] != savedInstrByte);
		if (!patternsWrote)
		{
			ctx->LogError("Patterns tab: 'q' did not write pattern[%d][%d]: note=0x%02X instr=0x%02X focusedView=%s",
			              writePatt, writeRow * 4,
			              (int)pattern[writePatt][writeRow * 4 + 0],
			              (int)pattern[writePatt][writeRow * 4 + 1],
			              guiMain->focusedView ? guiMain->focusedView->name : "NULL");
		}
		// Restore pattern bytes regardless.
		pattern[writePatt][writeRow * 4 + 0] = savedNoteByte;
		pattern[writePatt][writeRow * 4 + 1] = savedInstrByte;
		IM_CHECK(patternsWrote);

		// --- Case B: Instrument is the active tab ---
		// Switch tabs, re-park NavWindow on the dock host, expect
		// focusedView to land on instrument now.
		if (node->TabBar)
			node->TabBar->NextSelectedTabId = instrument->imGuiWindow->TabId;
		node->SelectedTabId = instrument->imGuiWindow->TabId;
		ctx->Yield(3);

		ImGui::FocusWindow(node->HostWindow);
		ctx->Yield(2);

		if (guiMain->focusedView != (CGuiView *)instrument)
		{
			ctx->LogError("Instrument active tab + NavWindow on dock host: focusedView=%s expected=%s",
			              guiMain->focusedView ? guiMain->focusedView->name : "NULL",
			              instrument->name);
			IM_CHECK(false);
		}

		// Instrument key handling forwards to native GT2 via the
		// CViewC64GoatTracker event queue, processed on the GT2 thread.
		// We need to (a) point at the attack/decay field (eipos==0),
		// (b) inject two hex digits, (c) yield long enough for the GT2
		// thread to drain its event queue. The hex digit route uses
		// 'h'/'l' nybble cursoring in native ginstr.c — the easiest
		// observable side-effect is the final ginstr[einum].ad byte.
		int  savedEipos    = eipos;
		int  savedEicolumn = eicolumn;
		int  savedEinum    = einum;
		unsigned char savedAd = ginstr[einum].ad;

		editmode = EDIT_INSTRUMENT;
		eipos    = 0;   // attack/decay row
		eicolumn = 0;   // high nybble
		ginstr[einum].ad = 0x00;

		guiMain->KeyDown('3');
		// Each forwarded key has to make it through CViewC64GoatTracker's
		// internal event queue, which the GT2 main thread drains one
		// event per frame. Yield generously between digits so the second
		// digit doesn't queue ahead of the first being processed.
		ctx->Yield(20);

		guiMain->KeyDown('f');
		ctx->Yield(40);

		unsigned char observedAd = ginstr[einum].ad;
		// Restore before asserting so a failure path doesn't leak state.
		ginstr[einum].ad = savedAd;
		eipos    = savedEipos;
		eicolumn = savedEicolumn;
		einum    = savedEinum;

		if (observedAd != 0x3F)
		{
			ctx->LogError("Instrument tab: typing '3' then 'f' on attack/decay produced ad=0x%02X (expected 0x3F) focusedView=%s",
			              observedAd, guiMain->focusedView ? guiMain->focusedView->name : "NULL");
			IM_CHECK(false);
		}

		// --- Restore pattern-side state and the original dock layout
		// so this test doesn't permanently rearrange the user's
		// workspace when run against a real layouts.dat.
		editmode   = savedEditmode;
		recordmode = savedRecord;
		eppos      = savedEppos;
		epchn      = savedEpchn;
		epcolumn   = savedEpcolumn;
		keypreset  = (unsigned)savedKeypreset;
		patterns->eparpcol  = savedEparpcol;
		patterns->epInSustain = savedEpInSus;

		ImGui::DockBuilderDockWindow(patterns->imGuiWindow->Name,   origPatternsDock);
		ImGui::DockBuilderDockWindow(instrument->imGuiWindow->Name, origInstrumentDock);
		ImGui::DockBuilderRemoveNode(sharedNodeId);
		ctx->Yield(5);

		restoreC64();
	};
}

#endif // ENABLE_IMGUI_TEST_ENGINE
