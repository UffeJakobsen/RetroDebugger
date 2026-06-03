#ifndef _CViewGT2Toolbar_H_
#define _CViewGT2Toolbar_H_

#include "SYS_Defs.h"
#include "CGuiView.h"
#include "ImGuiToolbar.h"

class C64DebuggerPluginGoatTracker;

class CViewGT2Toolbar : public CGuiView
{
public:
	CViewGT2Toolbar(const char *name, float posX, float posY, float posZ,
					  float sizeX, float sizeY, C64DebuggerPluginGoatTracker *plugin);
	virtual ~CViewGT2Toolbar();

	virtual void RenderImGui();
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);

	bool TriggerPlayPause();
	bool TriggerStop();
	bool TriggerUndo();
	bool TriggerRedo();
	bool ToggleLoopCurrentPattern();
	bool ToggleFollowPattern();
	bool ToggleMetronome();
	void AdjustOctave(int delta);
	void SetOctave(int octave);
	int GetOctaveEditValue() const;
	void SetOctaveEditValue(int octave);
	bool CanUndo() const;
	bool CanRedo() const;
	bool IsPlaybackActive() const;
	bool IsLoopCurrentPatternEnabled() const;
	bool IsFollowPatternEnabled() const;
	bool IsMetronomeEnabled() const;

	C64DebuggerPluginGoatTracker *plugin;
	ImGuiToolbar toolbar;
};

#endif
