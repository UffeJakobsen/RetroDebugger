#include "CViewGT2Oscilloscope.h"
#include "GT2ViewCommon.h"
#include "GT2RenderHelper.h"
#include "CViewWaveform.h"
#include "CWaveformData.h"
#include "CGT2VoiceWaveforms.h"
#include "imgui.h"
#include <algorithm>

// Live-tunable from the view's right-click context menu; persisted by
// PLUGIN_GoatTrackerSaveSettings / LoadSettings.
float gt2OscStrokeThickness = 3.0f;

CViewGT2Oscilloscope::CViewGT2Oscilloscope(const char *name, float posX, float posY, float posZ,
										   float sizeX, float sizeY)
: CGuiView(posX, posY, posZ, sizeX, sizeY)
{
	this->name = name;
	imGuiNoScrollbar = true;
	for (int i = 0; i < 3; i++)
	{
		// Bind directly to GT2's per-voice waveform buffers — gsid.cpp
		// pushes per-sample voice values into these on every SID clock,
		// and the plugin calls CopySampleData + CalculateTriggerPos
		// once per frame. The buffers exist as soon as the plugin's
		// Init has run, which happens before the views are constructed.
		viewChannel[i] = new CViewWaveform(
			"CViewGT2Oscilloscope::Channel",
			0, 0, 0, 0, 0,
			gt2VoiceWaveform[i]);
	}
	// Mix band bound to the buffer that gets the raw audio samples — the
	// same data path that feeds the mixer. If this band moves while the
	// per-voice bands stay flat we know voice_output() is the failure
	// point, not the rendering / binding chain.
	viewMix = new CViewWaveform(
		"CViewGT2Oscilloscope::Mix",
		0, 0, 0, 0, 0,
		gt2MixWaveform);
}

CViewGT2Oscilloscope::~CViewGT2Oscilloscope()
{
	for (int i = 0; i < 3; i++)
		delete viewChannel[i];
	delete viewMix;
}

// Stride between samples drawn — must match CWaveformData::CalculateTriggerPos,
// which uses `jac = waveformDataLength / 8` as a half-window and renders 2*jac
// samples (i.e. waveformDataLength/4) stretched across the band width. Drawing
// directly here lets us pick a thicker stroke + scale it with the GT2 UI zoom;
// the bundled CWaveformData::Render hardcodes AddLine without a thickness.
static void GT2_RenderOscBand(CWaveformData *wf, float x, float y, float w, float h,
								ImU32 fg, ImU32 bg, float thickness)
{
	ImDrawList *dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), bg);
	if (wf == NULL || wf->waveformDataRender == NULL || wf->waveformDataLength <= 0
		|| w <= 0.0f || h <= 0.0f)
	{
		return;
	}

	int jac = wf->waveformDataLength / 8;
	if (jac < 1) jac = 1;
	float step = (float)(jac * 2) / w;
	float samplePos = (float)wf->waveformTriggerPos - (float)jac;

	auto clampSample = [&](int sp) {
		if (sp < 0) return 0;
		if (sp >= wf->waveformDataLength) return wf->waveformDataLength - 1;
		return sp;
	};

	float prevX = x;
	float prevY = y + (((float)wf->waveformDataRender[clampSample((int)samplePos)] + 32767.0f) / 65536.0f) * h;
	samplePos += step;

	for (float px = 1.0f; px <= w; px += 1.0f)
	{
		short value = wf->waveformDataRender[clampSample((int)samplePos)];
		float curX = x + px;
		float curY = y + (((float)value + 32767.0f) / 65536.0f) * h;
		dl->AddLine(ImVec2(prevX, prevY), ImVec2(curX, curY), fg, thickness);
		prevX = curX;
		prevY = curY;
		samplePos += step;
	}
}

void CViewGT2Oscilloscope::EnsureWaveformsSnapshotForRender()
{
	// Self-driven snapshot: GT2 audio runs on its own thread independently
	// of VICE, so the plugin's DoFrame (which is what calls
	// GT2_VoiceWaveforms_UpdatePerFrame) only fires when the C64 emulator
	// is refreshing — which often isn't the case while GT2 plays in
	// isolation. Pulling the snapshot from inside the oscilloscope's
	// render keeps the band's render array in sync regardless of
	// whatever else the emulator's main loop is doing.
	GT2_VoiceWaveforms_UpdatePerFrame();
}

void CViewGT2Oscilloscope::RenderImGui()
{
	PreRenderImGui();
	EnsureWaveformsSnapshotForRender();
	ImVec2 origin = ImGui::GetCursorScreenPos();
	ImVec2 avail  = ImGui::GetContentRegionAvail();

	// Four equal-height bands: voice 0, 1, 2, mix. We render directly via
	// ImDrawList instead of delegating to CViewWaveform::Render — that path
	// uses the engine's GL backend (BlitRectangle + 1px AddLine) which we
	// cannot ask for thicker / zoom-aware strokes. Doing it here also keeps
	// everything in the ImGui draw layer so the bands compose cleanly with
	// the surrounding view chrome.
	float h = avail.y / 4.0f;
	if (h < 1.0f) h = 1.0f;

	// Stroke thickness is user-tunable (right-click context menu) and
	// scales with GT2's UI zoom. Floored at 0.5 px (anything smaller is
	// invisible).
	float uiScale = GT2EffectiveUIScale();
	float thickness = std::max(0.5f, gt2OscStrokeThickness * uiScale);

	ImU32 bgActive = IM_COL32(40, 0, 0, 255);
	ImU32 bgMuted  = IM_COL32(60, 60, 60, 255);
	ImU32 fgActive = IM_COL32(238, 255, 255, 255);
	ImU32 fgMuted  = IM_COL32(76, 76, 76, 255);

	for (int i = 0; i < 3; i++)
	{
		CWaveformData *wf = gt2VoiceWaveform[i];
		bool muted = (wf && wf->isMuted);
		GT2_RenderOscBand(wf, origin.x, origin.y + (float)i * h,
						   avail.x, h,
						   muted ? fgMuted : fgActive,
						   muted ? bgMuted : bgActive,
						   thickness);
	}
	{
		CWaveformData *wf = gt2MixWaveform;
		bool muted = (wf && wf->isMuted);
		GT2_RenderOscBand(wf, origin.x, origin.y + 3.0f * h,
						   avail.x, h,
						   muted ? fgMuted : fgActive,
						   muted ? bgMuted : bgActive,
						   thickness);
	}

	PostRenderImGui();
}

void CViewGT2Oscilloscope::RenderContextMenuItems()
{
	// Live slider on the stroke thickness so the value the renderer is
	// using is observable + adjustable without rebuilding. Range 0.5–20 px
	// matches the floor/ceiling enforced by the render path.
	float v = gt2OscStrokeThickness;
	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::SliderFloat("Stroke thickness", &v, 0.5f, 20.0f, "%.1f px"))
	{
		if (v < 0.5f) v = 0.5f;
		if (v > 20.0f) v = 20.0f;
		gt2OscStrokeThickness = v;
	}
	ImGui::TextDisabled("multiplied by UI zoom at render time");
}

bool CViewGT2Oscilloscope::KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	return GT2_HandleRenoiseOrForwardKeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}

bool CViewGT2Oscilloscope::KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	GT2_ForwardKeyUp(keyCode);
	return true;
}

bool CViewGT2Oscilloscope::KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	return KeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}
