#include "CViewGT2Toolbar.h"
#include "C64DebuggerPluginGoatTracker.h"
#include "CViewGT2Patterns.h"
#include "CViewGT2Tables.h"
#include "GT2ViewCommon.h"
#include "GT2RenderHelper.h"
#include "IconsFontAwesome_c.h"
#include "imgui.h"
#include "imgui_internal.h"   // ArrowButtonEx (explicit-size arrow button)

extern "C" {
#include "gcommon.h"
#include "gplay.h"
#include "gorder.h"
#include "gsong.h"
#include "gsid.h"
extern int editmode, eamode, menu, followplay, songinit;
extern int stepsize;            // GT2 step size; also the row-highlight interval
extern int epoctave;            // GT2 keyboard octave, clamped to 0..7
extern int gt2RenoiseEditStep;  // Renoise-layout cursor advance
extern unsigned keypreset;
}
#include <cstring>

extern bool gt2RenoiseFollowTrack;
extern bool gt2MetronomeEnabled;

#define EDIT_TABLES 3
#define KEY_RENOISE 4

static bool GT2ToolbarButton(ImGuiToolbar &toolbar, const char *icon, const char *tooltip, bool active = false, bool enabled = true)
{
	if (active)
	{
		// Toggled-on state: a bright, saturated fill so it is clearly
		// distinguishable from the default (untoggled) button shade.
		ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.60f, 1.00f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.38f, 0.72f, 1.00f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.14f, 0.48f, 0.86f, 1.0f));
	}
	bool pressed = toolbar.Button(icon, tooltip, enabled);
	if (active)
	{
		ImGui::PopStyleColor(3);
	}
	return pressed;
}

CViewGT2Toolbar::CViewGT2Toolbar(const char *name, float posX, float posY, float posZ,
								 float sizeX, float sizeY, C64DebuggerPluginGoatTracker *plugin)
: CGuiView(posX, posY, posZ, sizeX, sizeY)
{
	this->name = name;
	this->plugin = plugin;
	imGuiNoScrollbar = true;
}

CViewGT2Toolbar::~CViewGT2Toolbar()
{
}

bool CViewGT2Toolbar::TriggerPlayPause()
{
	if (eamode || menu) return false;

	if (IsPlaybackActive())
	{
		return TriggerStop();
	}

	if (eseditpos != espos[eschn])
	{
		for (int c = 0; c < MAX_CHN; c++)
		{
			if (eseditpos < songlen[esnum][c]) espos[c] = eseditpos;
			if (esend[c] <= espos[c]) esend[c] = 0;
		}
	}
	initsongpos(esnum, PLAY_POS, 0);
	followplay = gt2RenoiseFollowTrack ? 1 : 0;
	return true;
}

bool CViewGT2Toolbar::TriggerStop()
{
	// Total player reset: stop transport, wipe every channel's runtime
	// state (gate / arp position / current note / current instr / tempo
	// counters), and zero the SID register mirror so the next sid->clock
	// silences any leftover envelopes. Equivalent to the player state you
	// get right after a fresh `loadsong()` — song / instrument / pattern
	// DATA is left intact (we don't call clearsong), but no notes are
	// sustaining, no arps are mid-cycle, no funktable is mid-swap.
	stopsong();
	initchannels();
	memset(sidreg, 0, NUMSIDREGS);
	followplay = 0;
	return true;
}

bool CViewGT2Toolbar::TriggerUndo()
{
	if (editmode == EDIT_TABLES && plugin && plugin->viewTables)
		return plugin->viewTables->UndoTableEdit();
	return plugin && plugin->viewPatterns && plugin->viewPatterns->UndoPatternEdit();
}

bool CViewGT2Toolbar::TriggerRedo()
{
	if (editmode == EDIT_TABLES && plugin && plugin->viewTables)
		return plugin->viewTables->RedoTableEdit();
	return plugin && plugin->viewPatterns && plugin->viewPatterns->RedoPatternEdit();
}

bool CViewGT2Toolbar::ToggleLoopCurrentPattern()
{
	gt2LoopCurrentPattern = gt2LoopCurrentPattern ? 0 : 1;
	return IsLoopCurrentPatternEnabled();
}

bool CViewGT2Toolbar::ToggleFollowPattern()
{
	gt2RenoiseFollowTrack = !gt2RenoiseFollowTrack;
	if (gt2RenoiseFollowTrack)
	{
		if (IsPlaybackActive()) followplay = 1;
	}
	else
	{
		followplay = 0;
	}
	return gt2RenoiseFollowTrack;
}

bool CViewGT2Toolbar::ToggleMetronome()
{
	gt2MetronomeEnabled = !gt2MetronomeEnabled;
	return gt2MetronomeEnabled;
}

void CViewGT2Toolbar::AdjustOctave(int delta)
{
	SetOctave(epoctave + delta);
}

void CViewGT2Toolbar::SetOctave(int octave)
{
	if (octave < 0) octave = 0;
	if (octave > 7) octave = 7;
	epoctave = octave;
}

int CViewGT2Toolbar::GetOctaveEditValue() const
{
	return epoctave + 1;
}

void CViewGT2Toolbar::SetOctaveEditValue(int octave)
{
	SetOctave(octave - 1);
}

bool CViewGT2Toolbar::IsPlaybackActive() const
{
	return songinit != PLAY_STOPPED && songinit != PLAY_STOP;
}

bool CViewGT2Toolbar::CanUndo() const
{
	if (editmode == EDIT_TABLES && plugin && plugin->viewTables)
		return plugin->viewTables->CanUndoTableEdit();
	return plugin && plugin->viewPatterns && plugin->viewPatterns->CanUndoPatternEdit();
}

bool CViewGT2Toolbar::CanRedo() const
{
	if (editmode == EDIT_TABLES && plugin && plugin->viewTables)
		return plugin->viewTables->CanRedoTableEdit();
	return plugin && plugin->viewPatterns && plugin->viewPatterns->CanRedoPatternEdit();
}

bool CViewGT2Toolbar::IsLoopCurrentPatternEnabled() const
{
	return gt2LoopCurrentPattern != 0;
}

bool CViewGT2Toolbar::IsFollowPatternEnabled() const
{
	return gt2RenoiseFollowTrack;
}

bool CViewGT2Toolbar::IsMetronomeEnabled() const
{
	return gt2MetronomeEnabled;
}

void CViewGT2Toolbar::RenderImGui()
{
	PreRenderImGui();
	float uiScale = GT2EffectiveUIScale();
	ImGui::SetWindowFontScale(uiScale);
	ImGuiStyle &style = ImGui::GetStyle();
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		ImVec2(style.FramePadding.x * uiScale, style.FramePadding.y * uiScale));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
		ImVec2(style.ItemSpacing.x * uiScale, style.ItemSpacing.y * uiScale));

	if (toolbar.BeginToolbar("gt2Toolbar"))
	{
		const char *playPauseIcon = IsPlaybackActive() ? ICON_FA_PAUSE : ICON_FA_PLAY;
		const char *playPauseTooltip = IsPlaybackActive() ? "Pause" : "Play current pattern from row 0";
		if (GT2ToolbarButton(toolbar, playPauseIcon, playPauseTooltip))
		{
			TriggerPlayPause();
		}
		if (GT2ToolbarButton(toolbar, ICON_FA_REPEAT, "Loop current pattern", IsLoopCurrentPatternEnabled()))
		{
			ToggleLoopCurrentPattern();
		}
		if (GT2ToolbarButton(toolbar, ICON_FA_STOP, "Stop"))
		{
			TriggerStop();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x * 4.0f, ImGui::GetStyle().ItemSpacing.y));
		if (GT2ToolbarButton(toolbar, ICON_FA_LOCATION_ARROW, "Follow pattern", IsFollowPatternEnabled()))
		{
			ToggleFollowPattern();
		}
		ImGui::PopStyleVar();

		if (GT2ToolbarButton(toolbar, ICON_FA_BELL_O, "Metronome (not implemented yet)", IsMetronomeEnabled()))
		{
			ToggleMetronome();
		}

		// New section: Undo / Redo
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x * 4.0f, ImGui::GetStyle().ItemSpacing.y));
		if (GT2ToolbarButton(toolbar, ICON_FA_UNDO, "Undo", false, CanUndo()))
		{
			TriggerUndo();
		}
		ImGui::PopStyleVar();
		if (GT2ToolbarButton(toolbar, ICON_FA_SHARE, "Redo", false, CanRedo()))
		{
			TriggerRedo();
		}

		// New section: row-highlight interval and (Renoise) edit step.
		// Each value gets ImGui ArrowButtons (decrement / increment) on its
		// left, then the number field.
		// Arrow buttons: full textbox height, narrow width, tight gap.
		const ImVec2 kArrowSize(ImGui::GetFontSize() + 2.0f, ImGui::GetFrameHeight());
		const ImVec2 kArrowGap(1.0f * uiScale, ImGui::GetStyle().ItemSpacing.y);

		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 4.0f);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Hl");
		ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kArrowGap);
		if (ImGui::ArrowButtonEx("##gt2hl_dec", ImGuiDir_Left,  kArrowSize)) stepsize--;
		ImGui::SameLine();
		if (ImGui::ArrowButtonEx("##gt2hl_inc", ImGuiDir_Right, kArrowSize)) stepsize++;
		ImGui::PopStyleVar();
		ImGui::SameLine();
		ImGui::SetNextItemWidth(46.0f * uiScale);
		ImGui::InputInt("##gt2hl", &stepsize, 0, 0);
		if (stepsize < 1) stepsize = 1;   // 0 would divide-by-zero in the row highlight
		ImGui::SetItemTooltip("Highlight row numbers every N rows");

		if (keypreset == KEY_RENOISE)
		{
			ImGui::SameLine();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Step");
			ImGui::SameLine();
			bool stepChanged = false;
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kArrowGap);
			if (ImGui::ArrowButtonEx("##gt2step_dec", ImGuiDir_Left,  kArrowSize)) { gt2RenoiseEditStep--; stepChanged = true; }
			ImGui::SameLine();
			if (ImGui::ArrowButtonEx("##gt2step_inc", ImGuiDir_Right, kArrowSize)) { gt2RenoiseEditStep++; stepChanged = true; }
			ImGui::PopStyleVar();
			ImGui::SameLine();
			ImGui::SetNextItemWidth(46.0f * uiScale);
			if (ImGui::InputInt("##gt2step", &gt2RenoiseEditStep, 0, 0)) stepChanged = true;
			if (gt2RenoiseEditStep < 0) gt2RenoiseEditStep = 0;
			if (stepChanged) PLUGIN_GoatTrackerSaveSettings();
			ImGui::SetItemTooltip("Edit step (Renoise cursor advance)");

			ImGui::SameLine();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Oct");
			ImGui::SameLine();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kArrowGap);
			if (ImGui::ArrowButtonEx("##gt2oct_dec", ImGuiDir_Left,  kArrowSize)) AdjustOctave(-1);
			ImGui::SameLine();
			if (ImGui::ArrowButtonEx("##gt2oct_inc", ImGuiDir_Right, kArrowSize)) AdjustOctave(1);
			ImGui::PopStyleVar();
			ImGui::SameLine();
			ImGui::SetNextItemWidth(46.0f * uiScale);
			int octaveEditValue = GetOctaveEditValue();
			if (ImGui::InputInt("##gt2oct", &octaveEditValue, 0, 0)) SetOctaveEditValue(octaveEditValue);
			ImGui::SetItemTooltip("Octave (Renoise note entry)");
		}

		toolbar.EndToolbar();
	}
	ImGui::PopStyleVar(2);

	PostRenderImGui();
}

bool CViewGT2Toolbar::KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	// While a toolbar input field (Hl / Step) is being edited, keystrokes
	// belong to ImGui; otherwise forward them to the tracker so note entry
	// continues seamlessly after a toolbar click.
	if (ImGui::GetIO().WantTextInput)
		return false;
	return GT2_HandleRenoiseOrForwardKeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}

bool CViewGT2Toolbar::KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	if (ImGui::GetIO().WantTextInput)
		return false;
	GT2_ForwardKeyUp(keyCode);
	return true;
}

bool CViewGT2Toolbar::KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	return KeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}
