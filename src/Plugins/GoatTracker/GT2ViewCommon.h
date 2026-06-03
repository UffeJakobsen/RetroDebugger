#ifndef _GT2ViewCommon_H_
#define _GT2ViewCommon_H_

#include "SYS_Defs.h"

// Shared keyboard/mouse forwarding for all GT2 ImGui views.
// Include this in each GT2 view .cpp and call GT2_ForwardKeyDown etc.

class CViewC64GoatTracker;
class CGuiView;

// Set by plugin init, used by all GT2 views to forward events
extern CViewC64GoatTracker *gt2MainView;

// Number of song channels. Today this is the compile-time MAX_CHN (3); it is
// routed through one accessor so a future SID-stereo change (6 channels) only
// has to update this single place.
int GT2_NumChannels();

void GT2_ForwardKeyDown(u32 keyCode);
void GT2_ForwardKeyUp(u32 keyCode);
// Same as GT2_ForwardKeyDown but ignores the keypreset gate. Used by
// views (Instrument, Tables, …) whose field editing relies on native
// GT2 handlers (gt2/ginstr.c, gt2/gtable.c) — those handlers are the
// design source of truth for that context, regardless of which keyboard
// preset the user picked.
void GT2_ForwardKeyDownToNative(u32 keyCode);
bool GT2_HandleRenoiseOrForwardKeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
// Variant that lets renoiseInput see the key first (so undo/redo, mute,
// etc. still work) but falls back to GT2_ForwardKeyDownToNative — i.e.
// unconditionally to native GT2 — when the renoise handler didn't
// consume it. For views that delegate field editing to native GT2.
bool GT2_HandleRenoiseOrForwardKeyDownToNative(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
bool GT2_IsModifierKey(u32 keyCode);
void GT2_ForwardMouseDown(float viewX, float viewY, float viewW, float viewH, float clickX, float clickY);
void GT2_ForwardMouseUp(float viewX, float viewY, float viewW, float viewH, float clickX, float clickY);

// When a GT2 view uses ImGui::BeginChild internally, the click that gives the
// child window focus only updates ImGui's NavWindow to the child — CGuiView's
// PreRenderImGui checks ImGui::IsWindowFocused() against the OUTER window and
// so misses the click. Without this propagation, the engine never marks the
// outer view as focusedView, so GT2 shortcuts (Space, Ctrl+arrow, undo/redo,
// etc.) never reach renoiseInput while the user is interacting with sliders
// or buttons in the child. Call this right before PostRenderImGui() in any
// GT2 view that opens BeginChild blocks.
void GT2_PropagateChildWindowFocus(CGuiView *view);

#endif
