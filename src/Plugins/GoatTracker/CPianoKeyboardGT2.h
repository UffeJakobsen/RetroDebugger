#ifndef _CPianoKeyboardGT2_H_
#define _CPianoKeyboardGT2_H_

#include "CPianoKeyboard.h"

class C64DebuggerPluginGoatTracker;

class CPianoKeyboardGT2 : public CPianoKeyboard, public CPianoKeyboardCallback
{
public:
	CPianoKeyboardGT2(const char *name, float posX, float posY, float posZ,
					   float sizeX, float sizeY, C64DebuggerPluginGoatTracker *plugin);
	virtual ~CPianoKeyboardGT2();

	virtual void Render();
	virtual void RenderImGui();
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);

	virtual void PianoKeyboardNotePressed(CPianoKeyboard *pianoKeyboard, CPianoKey *pianoKey);
	virtual void PianoKeyboardNoteReleased(CPianoKeyboard *pianoKeyboard, CPianoKey *pianoKey);

private:
	C64DebuggerPluginGoatTracker *plugin;

	int GT2NoteFromPianoKey(CPianoKey *pianoKey) const;
	void HighlightPianoKey(CPianoKey *pianoKey, int channel) const;
	void HighlightPianoNote(int note, int channel) const;
	void ApplyPlayerFeedback();
	bool IsRepeatKeyHandled(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper) const;
	bool RecordNote(int gt2Note, int *previewNote);
	void AdvanceAfterRecord();
};

#endif
