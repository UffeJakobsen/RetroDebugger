#include "CPianoKeyboardGT2.h"
#include "C64DebuggerPluginGoatTracker.h"
#include "CViewGT2Patterns.h"
#include "GT2ViewCommon.h"

extern "C" {
#include "gcommon.h"
#include "gplay.h"
extern unsigned char pattern[MAX_PATT][MAX_PATTROWS * 4 + 4];
extern unsigned char arpdata[MAX_PATT][MAX_CHN][MAX_PATTROWS][MAX_ARP_COLS];
extern int pattlen[MAX_PATT];
extern int epnum[MAX_CHN];
extern int eppos, epcolumn, epchn;
extern int editmode, recordmode, autoadvance;
extern int einum;
extern unsigned keypreset;
extern int gt2RenoiseEditStep;
void gt2advanceeditstep(void);
extern int numarpcolumns;
}

#define EDIT_PATTERN 0
#define KEY_RENOISE 4

CPianoKeyboardGT2::CPianoKeyboardGT2(const char *name, float posX, float posY, float posZ,
									 float sizeX, float sizeY, C64DebuggerPluginGoatTracker *plugin)
: CPianoKeyboard(name, posX, posY, posZ, sizeX, sizeY, this)
{
	this->plugin = plugin;
}

CPianoKeyboardGT2::~CPianoKeyboardGT2()
{
}

void CPianoKeyboardGT2::Render()
{
	ApplyPlayerFeedback();
	CPianoKeyboard::Render();
}

void CPianoKeyboardGT2::RenderImGui()
{
	PreRenderImGui();
	Render();
	PostRenderImGui();
	CPianoKeyboard::DoLogic();
}

bool CPianoKeyboardGT2::KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	if (!HasFocus()) return false;
	return GT2_HandleRenoiseOrForwardKeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}

bool CPianoKeyboardGT2::KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	if (!HasFocus()) return false;
	GT2_ForwardKeyUp(keyCode);
	return true;
}

bool CPianoKeyboardGT2::KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	if (!HasFocus()) return false;
	return IsRepeatKeyHandled(keyCode, isShift, isAlt, isControl, isSuper);
}

bool CPianoKeyboardGT2::IsRepeatKeyHandled(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper) const
{
	if ((keyCode == '[' || keyCode == ']')
		&& isShift == false && isAlt == false && isControl == false && isSuper == false)
		return true;

	for (std::list<CPianoNoteKeyCode *>::const_iterator it = notesKeyCodes.begin(); it != notesKeyCodes.end(); it++)
	{
		CPianoNoteKeyCode *noteKeyCode = *it;
		if (keyCode == noteKeyCode->keyCode
			&& isShift == noteKeyCode->isShift
			&& isAlt == noteKeyCode->isAlt
			&& isControl == noteKeyCode->isControl
			&& isSuper == noteKeyCode->isSuper)
			return true;
	}

	return false;
}

int CPianoKeyboardGT2::GT2NoteFromPianoKey(CPianoKey *pianoKey) const
{
	if (pianoKey == NULL) return -1;
	int gt2Note = FIRSTNOTE + pianoKey->keyNote;
	if (gt2Note < FIRSTNOTE || gt2Note > LASTNOTE) return -1;
	return gt2Note;
}

void CPianoKeyboardGT2::HighlightPianoKey(CPianoKey *pianoKey, int channel) const
{
	if (pianoKey == NULL) return;

	switch (channel)
	{
	case 0:
		pianoKey->cr = 1.0f;
		pianoKey->cg = 0.0f;
		pianoKey->cb = 0.0f;
		break;
	case 1:
		pianoKey->cr = 0.0f;
		pianoKey->cg = 1.0f;
		pianoKey->cb = 0.0f;
		break;
	default:
		pianoKey->cr = pianoKey->isBlackKey ? 0.2f : 0.0f;
		pianoKey->cg = pianoKey->isBlackKey ? 0.2f : 0.0f;
		pianoKey->cb = 1.0f;
		break;
	}
}

void CPianoKeyboardGT2::HighlightPianoNote(int note, int channel) const
{
	if (note < 0 || note >= pianoKeys.size()) return;
	HighlightPianoKey(pianoKeys[note], channel);
}

void CPianoKeyboardGT2::ApplyPlayerFeedback()
{
	if (doKeysFadeOut == false)
	{
		for (std::vector<CPianoKey *>::iterator it = pianoKeys.begin(); it != pianoKeys.end(); it++)
		{
			CPianoKey *key = *it;
			if (key->isBlackKey)
			{
				key->cr = key->cg = key->cb = 0.0f;
			}
			else
			{
				key->cr = key->cg = key->cb = 1.0f;
			}
		}
	}

	for (int channel = 0; channel < MAX_CHN; channel++)
	{
		if (chn[channel].mute)
			continue;

		if (chn[channel].gate == 0xff)
		{
			for (int i = 0; i < chn[channel].arpcount && i < MAX_ARP_COLS + 1; i++)
			{
				HighlightPianoNote(chn[channel].arpnotes[i], channel);
			}
		}

		if (chn[channel].newnote >= FIRSTNOTE && chn[channel].newnote <= LASTNOTE)
		{
			HighlightPianoNote(chn[channel].newnote - FIRSTNOTE, channel);
		}
	}
}

void CPianoKeyboardGT2::AdvanceAfterRecord()
{
	if (keypreset == KEY_RENOISE)
	{
		gt2advanceeditstep();
	}
	else if (autoadvance < 2)
	{
		eppos++;
		if (eppos > pattlen[epnum[epchn]])
			eppos = 0;
	}
}

bool CPianoKeyboardGT2::RecordNote(int gt2Note, int *previewNote)
{
	if (previewNote != NULL) *previewNote = gt2Note;
	if (editmode != EDIT_PATTERN || !recordmode || plugin == NULL || plugin->viewPatterns == NULL)
		return false;

	if (epchn < 0 || epchn >= MAX_CHN)
		return false;

	int pattNum = epnum[epchn];
	if (pattNum < 0 || pattNum >= MAX_PATT)
		return false;

	int eparpcol = plugin->viewPatterns->eparpcol;
	bool shouldAdvance = false;
	bool changed = false;

	if (eparpcol >= 0)
	{
		if (eparpcol >= numarpcolumns || eparpcol >= MAX_ARP_COLS)
			return false;

		int baseAtRow = 0;
		if (eppos >= 0 && eppos <= pattlen[pattNum])
		{
			unsigned char base = pattern[pattNum][eppos * 4];
			if (base >= FIRSTNOTE && base <= LASTNOTE)
				baseAtRow = base;
		}

		if (eppos >= 0 && eppos < pattlen[pattNum])
		{
			plugin->viewPatterns->BeginPatternUndoStep();
			arpdata[pattNum][epchn][eppos][eparpcol] = (unsigned char)gt2Note;
			chn[epchn].arpcolnotes[eparpcol] = (unsigned char)(gt2Note - FIRSTNOTE);
			plugin->viewPatterns->CommitPatternUndoStep();
			changed = true;
		}
		shouldAdvance = true;
		if (previewNote != NULL && baseAtRow != 0) *previewNote = baseAtRow;
	}
	else if (epcolumn == 0)
	{
		if (eppos >= 0 && eppos < pattlen[pattNum])
		{
			plugin->viewPatterns->BeginPatternUndoStep();
			pattern[pattNum][eppos * 4] = (unsigned char)gt2Note;
			pattern[pattNum][eppos * 4 + 1] = (unsigned char)einum;
			plugin->viewPatterns->CommitPatternUndoStep();
			changed = true;
		}
		shouldAdvance = true;
	}

	if (shouldAdvance)
		AdvanceAfterRecord();

	return changed;
}

void CPianoKeyboardGT2::PianoKeyboardNotePressed(CPianoKeyboard *pianoKeyboard, CPianoKey *pianoKey)
{
	int gt2Note = GT2NoteFromPianoKey(pianoKey);
	if (gt2Note < 0) return;

	HighlightPianoKey(pianoKey, epchn);

	int previewNote = gt2Note;
	RecordNote(gt2Note, &previewNote);
	playtestnote(previewNote, einum, epchn);
}

void CPianoKeyboardGT2::PianoKeyboardNoteReleased(CPianoKeyboard *pianoKeyboard, CPianoKey *pianoKey)
{
	int gt2Note = GT2NoteFromPianoKey(pianoKey);
	if (gt2Note < 0) return;
	if (epchn >= 0 && epchn < MAX_CHN)
		releasenote(epchn);
}
