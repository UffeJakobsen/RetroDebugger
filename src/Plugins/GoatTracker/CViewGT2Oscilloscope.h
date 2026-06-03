#ifndef _CViewGT2Oscilloscope_H_
#define _CViewGT2Oscilloscope_H_

#include "SYS_Defs.h"
#include "CGuiView.h"

class CViewWaveform;

// Triggered-oscilloscope view of the three SID voices, fed by the
// per-voice waveform buffers GT2 fills directly from its own reSID
// instance (see CGT2VoiceWaveforms + gsid.cpp). The buffers live in
// the plugin so this view only references them — it does not own the
// data.
class CViewGT2Oscilloscope : public CGuiView
{
public:
	CViewGT2Oscilloscope(const char *name, float posX, float posY, float posZ,
						 float sizeX, float sizeY);
	virtual ~CViewGT2Oscilloscope();

	virtual void RenderImGui();
	virtual bool HasContextMenuItems() { return true; }
	virtual void RenderContextMenuItems();
	// Snapshots the producer-side waveform buffers into the render-side
	// arrays. Called from RenderImGui every paint (so the view is
	// self-driven and not dependent on the VICE-specific DoFrame chain
	// which only fires while the C64 emulator is refreshing the screen).
	// Public so tests can exercise the same path the production render
	// uses without standing up a full ImGui frame.
	void EnsureWaveformsSnapshotForRender();
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);

private:
	// Three voice bands + a mix band; the mix band is a diagnostic
	// reference fed from the audio buffer GT2 already produces (the same
	// data that drives the mixer's VU). Keeping it on screen makes it
	// obvious whether the rendering path is alive even if voice_output()
	// happens to be returning zeros for the per-voice bands.
	CViewWaveform *viewChannel[3];
	CViewWaveform *viewMix;
};

// User-tunable oscilloscope stroke thickness — exposed so the right-click
// context menu can adjust it live and the value persists via the GT2
// settings hjson. Floor 0.5 px, ceiling 20 px. Multiplied by the GT2 UI
// zoom at render time so the stroke scales with the rest of the layout.
extern float gt2OscStrokeThickness;

#endif
