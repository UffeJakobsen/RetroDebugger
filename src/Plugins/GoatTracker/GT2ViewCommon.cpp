#include "GT2ViewCommon.h"
#include "C64DebuggerPluginGoatTracker.h"
#include "CViewC64GoatTracker.h"
#include "CViewGT2Patterns.h"
#include "CViewGT2Instrument.h"
#include "CGT2RenoiseInput.h"
#include "CGuiEvent.h"
#include "CGuiView.h"
#include "CGuiMain.h"
#include "SYS_Defs.h"
#include "SYS_KeyCodes.h"
#include "imgui.h"

extern "C" {
#include "gcommon.h"
#include "gconsole.h"
#include "goattrk2.h"
}

CViewC64GoatTracker *gt2MainView = NULL;

int GT2_NumChannels()
{
	return MAX_CHN;
}

void GT2_ForwardKeyDown(u32 keyCode)
{
	// Renoise overlays own their keys end-to-end. Pattern view forwards
	// here when nothing matched — under KEY_RENOISE this must be a no-op,
	// otherwise the key leaks through to native gpattern.c (e.g. Space →
	// recordmode toggle, the original bug that triggered the routing
	// rewrite). Views that genuinely delegate field editing to native GT2
	// (Instrument, Tables) MUST use GT2_ForwardKeyDownToNative instead;
	// this function is the "I'm an overlay and shouldn't reach native"
	// fallback.
	if (keypreset == KEY_RENOISE) return;
	GT2_ForwardKeyDownToNative(keyCode);
}

void GT2_ForwardKeyDownToNative(u32 keyCode)
{
	// Unconditional native-side delivery. Bypasses the KEY_RENOISE gate.
	// Use only from views whose field editing is implemented by native
	// GT2 (gt2/ginstr.c, gt2/gtable.c, …) — those handlers ARE the design
	// in their context, no matter which keyboard preset is active.
	if (!gt2MainView) return;
	CGuiEventKeyboard *ev = new CGuiEventKeyboard(GUI_EVENT_KEYBOARD_KEY_DOWN, keyCode, keyCode);
	gt2MainView->AddEvent(ev);
}

void GT2_ForwardKeyUp(u32 keyCode)
{
	if (pluginGoatTracker && pluginGoatTracker->renoiseInput
		&& pluginGoatTracker->renoiseInput->HandleKeyUp(keyCode, false, false, false, false))
	{
		return;
	}

	// Same rationale as GT2_ForwardKeyDown — native GT2 must not see the
	// matching key-up either, or its sticky-key state (notesonkbd, etc.)
	// drifts out of sync with what Renoise actually played.
	if (keypreset == KEY_RENOISE) return;
	if (!gt2MainView) return;
	CGuiEventKeyboard *ev = new CGuiEventKeyboard(GUI_EVENT_KEYBOARD_KEY_UP, keyCode, keyCode);
	gt2MainView->AddEvent(ev);
}

bool GT2_HandleRenoiseOrForwardKeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	// Global undo / redo — one shared history, works from every GT2 view
	// regardless of the keyboard preset.
	if ((isControl || isSuper) && !isAlt && pluginGoatTracker && pluginGoatTracker->viewPatterns)
	{
		if (!isShift && (keyCode == 'z' || keyCode == 'Z' || keyCode == SDLK_z))
		{
			pluginGoatTracker->viewPatterns->UndoPatternEdit();
			return true;
		}
		if (keyCode == 'y' || keyCode == 'Y' || keyCode == SDLK_y)
		{
			pluginGoatTracker->viewPatterns->RedoPatternEdit();
			return true;
		}
	}

	// Sustain column edit (when the cursor is parked there) — must run
	// before renoiseInput / HandleArpKey / native GT2 so the hex digit
	// goes to CMD_SETSR instead of the instrument byte.
	if (pluginGoatTracker && pluginGoatTracker->viewPatterns
		&& pluginGoatTracker->viewPatterns->HandleSustainColumnKey(keyCode, isShift, isAlt, isControl, isSuper))
	{
		return true;
	}

	if (pluginGoatTracker && pluginGoatTracker->renoiseInput
		&& pluginGoatTracker->renoiseInput->HandleKey(keyCode, isShift, isAlt, isControl, isSuper))
	{
		return true;
	}

	// Enter on an instrument's table-pointer field navigates the ImGui
	// table view instead of dropping into the native GT2 legacy editor.
	if (keyCode == MTKEY_ENTER && pluginGoatTracker && pluginGoatTracker->viewInstrument
		&& pluginGoatTracker->viewInstrument->HandleInstrumentTablePointerEnter(
			isShift, isAlt, isControl, isSuper))
	{
		return true;
	}

	if (pluginGoatTracker && pluginGoatTracker->viewPatterns
		&& pluginGoatTracker->viewPatterns->HandleArpKey(keyCode, isShift, isAlt, isControl, isSuper))
	{
		return true;
	}

	if (pluginGoatTracker && pluginGoatTracker->viewPatterns && !GT2_IsModifierKey(keyCode))
	{
		pluginGoatTracker->viewPatterns->eparpcol = -1;
	}

	GT2_ForwardKeyDown(keyCode);
	return true;
}

bool GT2_HandleRenoiseOrForwardKeyDownToNative(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	// Renoise sees the key first — Undo/Redo, transport, mute/solo must
	// keep working from instrument/tables views too. If nothing in the
	// Renoise stack consumed it, forward UNCONDITIONALLY to native GT2:
	// instrument/tables field editing is implemented over there and
	// that's intentional. Mirrors the head of GT2_HandleRenoiseOrForwardKeyDown
	// minus the final GT2_ForwardKeyDown gate.
	if ((isControl || isSuper) && !isAlt && pluginGoatTracker && pluginGoatTracker->viewPatterns)
	{
		if (!isShift && (keyCode == 'z' || keyCode == 'Z' || keyCode == SDLK_z))
		{
			pluginGoatTracker->viewPatterns->UndoPatternEdit();
			return true;
		}
		if (keyCode == 'y' || keyCode == 'Y' || keyCode == SDLK_y)
		{
			pluginGoatTracker->viewPatterns->RedoPatternEdit();
			return true;
		}
	}
	if (pluginGoatTracker && pluginGoatTracker->renoiseInput
		&& pluginGoatTracker->renoiseInput->HandleKey(keyCode, isShift, isAlt, isControl, isSuper))
	{
		return true;
	}
	GT2_ForwardKeyDownToNative(keyCode);
	return true;
}

bool GT2_IsModifierKey(u32 keyCode)
{
	return keyCode == MTKEY_LSHIFT
		|| keyCode == MTKEY_RSHIFT
		|| keyCode == MTKEY_LALT
		|| keyCode == MTKEY_RALT
		|| keyCode == MTKEY_LCONTROL
		|| keyCode == MTKEY_RCONTROL
		|| keyCode == MTKEY_LSUPER
		|| keyCode == MTKEY_RSUPER;
}

void GT2_ForwardMouseDown(float viewX, float viewY, float viewW, float viewH, float clickX, float clickY)
{
	if (!gt2MainView) return;
	// Convert ImGui window coords to GT2 pixel coords (full 800x592 screen)
	float xp = ((clickX - viewX) / viewW) * (float)(MAX_COLUMNS * 8);
	float yp = ((clickY - viewY) / viewH) * (float)(MAX_ROWS * 16);
	CGuiEventMouse *ev = new CGuiEventMouse(GUI_EVENT_MOUSE_LEFT_BUTTON_DOWN, (unsigned int)xp, (unsigned int)yp);
	gt2MainView->AddEvent(ev);
}

void GT2_ForwardMouseUp(float viewX, float viewY, float viewW, float viewH, float clickX, float clickY)
{
	if (!gt2MainView) return;
	float xp = ((clickX - viewX) / viewW) * (float)(MAX_COLUMNS * 8);
	float yp = ((clickY - viewY) / viewH) * (float)(MAX_ROWS * 16);
	CGuiEventMouse *ev = new CGuiEventMouse(GUI_EVENT_MOUSE_LEFT_BUTTON_UP, (unsigned int)xp, (unsigned int)yp);
	gt2MainView->AddEvent(ev);
}

void GT2_PropagateChildWindowFocus(CGuiView *view)
{
	if (view == NULL) return;
	// ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) returns true if
	// the outer window OR any of its currently-open child windows holds
	// focus. CGuiView::PreRenderImGui only checks the outer window, so
	// without this nudge the engine's focusedView never advances to a GT2
	// view whose interactive widgets all live inside BeginChild blocks
	// (mixer channel strips, instrument knob row, etc.). Without that, the
	// next KeyDown dispatched by CGuiMain goes to whatever view WAS focused
	// before, so Space / Ctrl+arrow shortcuts silently route to the wrong
	// renoiseInput target.
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
	{
		guiMain->SetInternalViewFocus(view);
	}
}
