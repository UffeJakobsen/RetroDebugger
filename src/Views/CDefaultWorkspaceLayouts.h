#pragma once

#include "CSlrKeyboardShortcuts.h"
#include <cstddef>
#include <string>
#include <vector>

class CLayoutManager;
class CLayoutData;
class CDebugInterface;
class CConfigStorageHjson;
class CGuiView;
class CViewC64;

enum EDefaultWorkspacePlatform
{
	DefaultWorkspacePlatform_C64,
	DefaultWorkspacePlatform_Atari800,
	DefaultWorkspacePlatform_NES
};

// Slot numbers preserve old C64 Debugger shortcut slots. Do not infer platform
// capabilities from the enum names; resolve platform behavior through specs.
enum EDefaultWorkspaceSlot
{
	DefaultWorkspaceSlot_Only = 1,
	DefaultWorkspaceSlot_DataDump = 2,
	DefaultWorkspaceSlot_Debugger = 3,
	DefaultWorkspaceSlot_1541MemoryMap = 4,
	DefaultWorkspaceSlot_ShowStates = 5,
	DefaultWorkspaceSlot_MemoryMap = 6,
	DefaultWorkspaceSlot_1541Debugger = 7,
	DefaultWorkspaceSlot_MonitorConsole = 8,
	DefaultWorkspaceSlot_FullScreenZoom = 9,
	DefaultWorkspaceSlot_Cycler = 10,
	DefaultWorkspaceSlot_VicDisplayLite = 11,
	DefaultWorkspaceSlot_VicDisplay = 12,
	DefaultWorkspaceSlot_SourceCode = 13,
	DefaultWorkspaceSlot_AllGraphics = 14,
	DefaultWorkspaceSlot_AllSids = 15,
	DefaultWorkspaceSlot_MemoryDebugger = 16
};

struct SDefaultWorkspaceViewPlacementSpec
{
	const char *viewId;
	float x;
	float y;
	float width;
	float height;
	float fontSize;
	bool hasDisassemblyParameters = false;
	bool disassemblyShowHexCodes = true;
	bool disassemblyShowCodeCycles = true;
	bool disassemblyShowLabels = false;
	float disassemblyFontFitScale = 1.0f;
};

static const float kDefaultWorkspaceReferenceWidth = 580.0f;
static const float kDefaultWorkspaceReferenceHeight = 360.0f;
static const float kDefaultWorkspaceMinimumWindowSize = 24.0f;
static const float kDefaultWorkspaceMinimumReadableFontSize = 5.0f;

struct SDefaultWorkspaceShortcutSlotSpec
{
	EDefaultWorkspaceSlot slot;
	const char *roleName;
	const char *defaultShortcutLabel;
	int keyCode;
	bool isShift;
	bool isAlt;
	bool isControl;
	bool isSuper;
};

struct SDefaultWorkspaceShortcutSlotSnapshot
{
	EDefaultWorkspaceSlot slot;
	bool hasShortcut = false;
	int keyCode = 0;
	bool isShift = false;
	bool isAlt = false;
	bool isControl = false;
	bool isSuper = false;
	bool hasConflict = false;
	bool userModified = false;
	bool suppressNextActivation = false;
	std::string conflictShortcutName;
	std::string assignedShortcutLabel;
	std::string statusText = "Unassigned";
};

struct SDefaultWorkspacePopupState
{
	bool lockedDefaultLayouts = true;
	bool createContextShortcuts = true;
	bool noTabBarsInGeneratedLayouts = true;
	EDefaultWorkspaceSlot capturingShortcutSlot = (EDefaultWorkspaceSlot)0;
	bool pendingKeyboardCallbackRemoval = false;
	bool hasShortcutSlotSnapshot = false;
	std::vector<SDefaultWorkspaceShortcutSlotSnapshot> shortcutSlotSnapshot;

	void StartShortcutCapture(EDefaultWorkspaceSlot slot);
	void StopShortcutCapture(bool removeKeyboardCallback);
	bool ConsumeKeyboardCallbackRemovalRequest();
};

struct SDefaultWorkspaceActivePlatformState
{
	bool c64 = false;
	bool atari800 = false;
	bool nes = false;
};

struct SDefaultWorkspaceShortcutSlotState
{
	EDefaultWorkspaceSlot slot;
	CSlrKeyboardShortcut *shortcut = NULL;
	bool hasConflict = false;
	bool userModified = false;
	bool suppressNextActivation = false;
	std::string conflictShortcutName;
	std::string assignedShortcutLabel;
	std::string statusText = "Unassigned";
};

struct SDefaultWorkspaceLayoutSpec
{
	EDefaultWorkspacePlatform platform;
	EDefaultWorkspaceSlot slot;
	const char *displayName;
	const char *predefinedId;
	const SDefaultWorkspaceViewPlacementSpec *placements = NULL;
	int numPlacements = 0;
};

struct SDefaultWorkspaceGenerationSummary
{
	int numCreatedOrUpdated = 0;
	int numGenerated = 0;
	int numFailed = 0;
	int numAssignedShortcutSlots = 0;
	int numShortcutConflicts = 0;
	bool completed = false;
};

const std::vector<SDefaultWorkspaceLayoutSpec> &GetDefaultWorkspaceLayoutSpecs(EDefaultWorkspacePlatform platform);
const std::vector<SDefaultWorkspaceLayoutSpec> &GetAllDefaultWorkspaceLayoutSpecs();
const SDefaultWorkspaceLayoutSpec *FindDefaultWorkspaceLayoutSpec(EDefaultWorkspacePlatform platform, EDefaultWorkspaceSlot slot);
const std::vector<SDefaultWorkspaceShortcutSlotSpec> &GetDefaultWorkspaceShortcutSlotSpecs();
const SDefaultWorkspaceShortcutSlotSpec *FindDefaultWorkspaceShortcutSlotSpec(EDefaultWorkspaceSlot slot);
SDefaultWorkspaceViewPlacementSpec ScaleDefaultWorkspacePlacement(const SDefaultWorkspaceViewPlacementSpec &placement, float viewportWidth, float viewportHeight);

class CDefaultWorkspaceLayouts : public CSlrKeyboardShortcutCallback
{
public:
	CDefaultWorkspaceLayouts(CViewC64 *viewC64, CLayoutManager *layoutManager);
	virtual ~CDefaultWorkspaceLayouts();

	void RegisterDefaultShortcutSlots(CSlrKeyboardShortcuts *keyboardShortcuts);
	bool AssignShortcutSlot(CSlrKeyboardShortcuts *keyboardShortcuts, EDefaultWorkspaceSlot slot, int keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	void ClearShortcutSlot(CSlrKeyboardShortcuts *keyboardShortcuts, EDefaultWorkspaceSlot slot);
	void ClearAllShortcutSlots(CSlrKeyboardShortcuts *keyboardShortcuts);
	void ResetShortcutSlotsToDefaults(CSlrKeyboardShortcuts *keyboardShortcuts);
	void SaveShortcutSlotStates(std::vector<SDefaultWorkspaceShortcutSlotSnapshot> *snapshots) const;
	void RestoreShortcutSlotStates(CSlrKeyboardShortcuts *keyboardShortcuts, const std::vector<SDefaultWorkspaceShortcutSlotSnapshot> &snapshots);
	void SaveShortcutSlotSettings(CConfigStorageHjson *config, bool contextShortcutsEnabled) const;
	bool RegisterStoredShortcutSlotsForGeneratedLayouts(CSlrKeyboardShortcuts *keyboardShortcuts, CConfigStorageHjson *config);

	const SDefaultWorkspaceShortcutSlotState *GetShortcutSlotState(EDefaultWorkspaceSlot slot) const;
	const char *GetShortcutLabel(EDefaultWorkspaceSlot slot) const;
	const char *GetShortcutStatus(EDefaultWorkspaceSlot slot) const;
	CLayoutData *CreateOrUpdateDefaultWorkspaceLayoutData(const SDefaultWorkspaceLayoutSpec *layoutSpec, bool lockedDefaultLayout);
	int CreateOrUpdateAllDefaultWorkspaceLayoutData(bool lockedDefaultLayouts);
	void BeginDefaultWorkspaceGeneration(bool lockedDefaultLayouts, bool noTabBarsInGeneratedLayouts);
	void UpdateDefaultWorkspaceGenerationFrame();
	bool IsGeneratingDefaultWorkspaces() const;
	const SDefaultWorkspaceGenerationSummary &GetLastGenerationSummary() const;
	CGuiView *FindViewForPlacement(const char *viewId) const;
	bool ApplyLayoutSpecFloating(const SDefaultWorkspaceLayoutSpec *layoutSpec, float viewportWidth, float viewportHeight, bool positionForNoTabBarPreserveScan = false);

	SDefaultWorkspaceActivePlatformState GetActivePlatformState() const;
	const SDefaultWorkspaceLayoutSpec *ResolveShortcutLayout(const SDefaultWorkspaceActivePlatformState &activePlatforms, EDefaultWorkspaceSlot slot) const;
	virtual bool ProcessKeyboardShortcut(u32 zone, u8 actionType, CSlrKeyboardShortcut *keyboardShortcut);

private:
	CViewC64 *viewC64;
	CLayoutManager *layoutManager;
	CSlrKeyboardShortcuts *registeredKeyboardShortcuts;
	std::vector<SDefaultWorkspaceShortcutSlotState> shortcutSlotStates;

	SDefaultWorkspaceShortcutSlotState *GetMutableShortcutSlotState(EDefaultWorkspaceSlot slot);
	const SDefaultWorkspaceShortcutSlotState *FindShortcutSlotStateForShortcut(CSlrKeyboardShortcut *keyboardShortcut) const;
	bool AssignShortcutSlotInternal(CSlrKeyboardShortcuts *keyboardShortcuts, EDefaultWorkspaceSlot slot, int keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper, const char *assignedShortcutLabel, bool userModified, bool suppressNextActivation);
	void ClearShortcutSlotInternal(CSlrKeyboardShortcuts *keyboardShortcuts, EDefaultWorkspaceSlot slot, bool userModified);
	bool HasGeneratedDefaultWorkspaceLayouts() const;
	bool SwitchToDefaultWorkspace(const SDefaultWorkspaceLayoutSpec *layoutSpec);
	CDebugInterface *GetDebugInterfaceForPlatform(EDefaultWorkspacePlatform platform) const;
	bool SetDebugInterfaceRunning(CDebugInterface *debugInterface, bool shouldBeRunning);
	bool EnsureOnlyGenerationPlatformRunning(EDefaultWorkspacePlatform platform);
	bool IsOnlyGenerationPlatformRunning(EDefaultWorkspacePlatform platform) const;
	bool RestoreGenerationOriginalPlatformState();
	bool IsGenerationOriginalPlatformStateRestored() const;
	void RestoreOriginalLayoutOrFinish();

	enum EDefaultWorkspaceGenerationStep
	{
		DefaultWorkspaceGenerationStep_Idle,
		DefaultWorkspaceGenerationStep_ApplyFloating,
		DefaultWorkspaceGenerationStep_WaitPlatformFrame,
		DefaultWorkspaceGenerationStep_WaitFloatingFrame,
		DefaultWorkspaceGenerationStep_WaitDockedFrame,
		DefaultWorkspaceGenerationStep_RestoreOriginal,
		DefaultWorkspaceGenerationStep_WaitRestoreEmulators,
		DefaultWorkspaceGenerationStep_WaitRestoreFrame
	};

	EDefaultWorkspaceGenerationStep generationStep;
	bool generationLockedDefaultLayouts;
	bool generationNoTabBarsInGeneratedLayouts;
	int generationSpecIndex;
	int generationWaitFrames;
	CLayoutData *generationOriginalLayout;
	CLayoutData *generationCurrentLayoutData;
	bool generationOriginalC64Running;
	bool generationOriginalAtariRunning;
	bool generationOriginalNesRunning;
	SDefaultWorkspaceGenerationSummary lastGenerationSummary;
	void FinishDefaultWorkspaceGeneration();
};
