#ifndef _CViewGT2Mixer_H_
#define _CViewGT2Mixer_H_

#include "SYS_Defs.h"
#include "CGuiView.h"

class CGT2AudioMixer;

class CViewGT2Mixer : public CGuiView
{
public:
	CViewGT2Mixer(const char *name, float posX, float posY, float posZ,
				  float sizeX, float sizeY, CGT2AudioMixer *mixer);
	virtual ~CViewGT2Mixer();

	virtual void RenderImGui();
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);
	virtual bool KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);

	CGT2AudioMixer *mixer;

private:
	// Render one channel strip (shared between per-channel and master bus)
	void RenderChannelStrip(int channelIndex, const char *label, bool isMaster);

	// Index of the channel whose effects panel is currently expanded (-1 = none, -2 = master)
	int expandedEffectsChannel;
};

#endif
