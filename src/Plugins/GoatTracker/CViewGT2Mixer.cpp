#include "CViewGT2Mixer.h"
#include "CGT2AudioMixer.h"
#include "GT2ViewCommon.h"
#include "GT2RenderHelper.h"
#include "CGT2EffectBiquadFilter.h"
#include "IGT2AudioEffect.h"
#include "imgui.h"
#include <cstdio>
#include <cmath>

extern "C" {
#include "gsid.h"
}

// Available effect types for the "Add Effect" dropdown
static const char* gEffectNames[] = { "Biquad Filter" };
enum { GT2_EFFECT_BIQUAD_FILTER = 0 };
static const int gNumEffects = 1;
static const int gNumGT2SidVoices = 3;

CViewGT2Mixer::CViewGT2Mixer(const char *name, float posX, float posY, float posZ,
							   float sizeX, float sizeY, CGT2AudioMixer *mixer)
: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->mixer = mixer;
	imGuiNoScrollbar = true;
	expandedEffectsChannel = -1;
}

CViewGT2Mixer::~CViewGT2Mixer()
{
}

// Render a VU bar using ImGui draw list. height in pixels, value in [0,1].
static void RenderVUBar(float value, float width, float height, bool effectivelyMuted)
{
	ImVec2 cursor = ImGui::GetCursorScreenPos();
	ImDrawList *dl = ImGui::GetWindowDrawList();

	// Background
	dl->AddRectFilled(cursor,
					  ImVec2(cursor.x + width, cursor.y + height),
					  IM_COL32(40, 40, 40, 255));

	// Filled portion (bottom-up)
	float filled = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
	float fillH  = filled * height;
	// Green -> yellow -> red colour split
	ImU32 barColor;
	if (effectivelyMuted)
		barColor = IM_COL32(90, 90, 90, 180);
	else if (value < 0.7f)
		barColor = IM_COL32(50, 200, 50, 255);
	else if (value < 0.9f)
		barColor = IM_COL32(220, 200, 30, 255);
	else
		barColor = IM_COL32(220, 50, 50, 255);

	dl->AddRectFilled(ImVec2(cursor.x,         cursor.y + height - fillH),
					  ImVec2(cursor.x + width,  cursor.y + height),
					  barColor);

	// Border
	dl->AddRect(cursor, ImVec2(cursor.x + width, cursor.y + height),
				IM_COL32(80, 80, 80, 255));

	ImGui::Dummy(ImVec2(width, height));
}

void CViewGT2Mixer::RenderChannelStrip(int channelIndex, const char *label, bool isMaster)
{
	float uiScale = GT2EffectiveUIScale();
	ImGui::SetWindowFontScale(uiScale);
	GT2MixerChannel &ch = isMaster ? mixer->masterBus : mixer->channels[channelIndex];
	bool effectivelyMuted = !isMaster && mixer->IsChannelEffectivelyMuted(channelIndex);

	// Push a unique ID scope for all widgets in this strip
	if (isMaster)
		ImGui::PushID("master");
	else
		ImGui::PushID(channelIndex);

	// Use actual available height from the child window
	float availH = ImGui::GetContentRegionAvail().y;

	// Layout: distribute available height among elements
	// Fixed elements: label, mute/solo (non-master), FX button
	// Plus item spacing between each element
	float lineH = ImGui::GetTextLineHeightWithSpacing();
	float spacing = ImGui::GetStyle().ItemSpacing.y;
	// Elements: label + VU + slider + [mute/solo] + FX = 4 or 5 items, so 3 or 4 gaps
	int numGaps = isMaster ? 3 : 4;
	float fixedH = lineH; // label
	if (!isMaster) fixedH += lineH; // mute/solo
	fixedH += lineH; // FX button
	fixedH += numGaps * spacing;
	float flexH = availH - fixedH;
	if (flexH < 40.0f * uiScale) flexH = 40.0f * uiScale;
	float vuHeight = flexH * 0.5f;
	float sliderHeight = flexH * 0.5f;

	float stripWidth = ImGui::GetContentRegionAvail().x;
	const float vuGap = 2.0f * uiScale;
	float vuWidth = (stripWidth - vuGap) * 0.5f;
	if (vuWidth < 6.0f * uiScale) vuWidth = 6.0f * uiScale;

	// Channel name label
	if (effectivelyMuted)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::Text("%s", label);
	if (effectivelyMuted)
		ImGui::PopStyleColor();

	// VU meters (L + R side by side)
	RenderVUBar(ch.peekL, vuWidth, vuHeight, effectivelyMuted);
	ImGui::SameLine(0.0f, vuGap);
	RenderVUBar(ch.peekR, vuWidth, vuHeight, effectivelyMuted);

	// Volume fader (vertical)
	ImGui::SetNextItemWidth(stripWidth);
	char sliderLabel[32];
	snprintf(sliderLabel, sizeof(sliderLabel), "##vol%s", isMaster ? "m" : label);
	if (effectivelyMuted)
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
	ImGui::VSliderFloat(sliderLabel, ImVec2(stripWidth, sliderHeight), &ch.volume, 0.0f, 1.0f, "");
	if (effectivelyMuted)
		ImGui::PopStyleVar();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Volume: %.2f", ch.volume);

	// Mute / Solo toggle buttons
	if (!isMaster)
	{
		float btnW = (stripWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		bool wasMuted = ch.mute;
		if (wasMuted)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
		}
		if (ImGui::Button("M", ImVec2(btnW, 0)))
			ch.mute = !ch.mute;
		if (wasMuted)
			ImGui::PopStyleColor(2);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mute");

		ImGui::SameLine();

		bool wasSolo = ch.solo;
		if (wasSolo)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.9f, 1.0f));
		}
		if (ImGui::Button("S", ImVec2(btnW, 0)))
			ch.solo = !ch.solo;
		if (wasSolo)
			ImGui::PopStyleColor(2);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Solo");
	}

	// Effects section toggle button
	int effectsTrigger = isMaster ? -2 : channelIndex;
	bool effectsOpen   = (expandedEffectsChannel == effectsTrigger);
	if (ImGui::Button(effectsOpen ? "FX^" : "FX"))
		expandedEffectsChannel = effectsOpen ? -1 : effectsTrigger;

	ImGui::PopID();
}

void CViewGT2Mixer::RenderImGui()
{
	PreRenderImGui();
	float uiScale = GT2EffectiveUIScale();
	ImGui::SetWindowFontScale(uiScale);
	ImGuiStyle &style = ImGui::GetStyle();
	ImVec2 framePadding(style.FramePadding.x * uiScale, style.FramePadding.y * uiScale);
	ImVec2 itemSpacing(style.ItemSpacing.x * uiScale, style.ItemSpacing.y * uiScale);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePadding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, itemSpacing);

	if (mixer == nullptr)
	{
		ImGui::TextDisabled("No mixer");
		ImGui::PopStyleVar(2);
		PostRenderImGui();
		return;
	}

	// Sync VU levels and volume flags with GT2 SID.
	for (int v = 0; v < gNumGT2SidVoices && v < GT2_MIXER_MAX_CHANNELS; v++)
	{
		mixer->channels[v].peekL = gt2_voice_level[v];
		mixer->channels[v].peekR = gt2_voice_level[v];
		gt2_voice_volume[v] = mixer->channels[v].volume;
	}
	mixer->ApplyVoiceMutes(gt2_voice_mute, gNumGT2SidVoices);
	gt2_master_volume = mixer->masterVolume;

	// Compute available space for dynamic layout
	ImVec2 avail = ImGui::GetContentRegionAvail();

	// Title
	ImGui::Text("GT2 Audio Mixer");
	ImGui::Separator();

	// Remaining height after title + separator + optional effects panel
	float titleH = ImGui::GetCursorPosY();
	float remainH = avail.y - titleH;
	// Reserve space for effects panel if open
	float effectsPanelH = (expandedEffectsChannel != -1) ? 150.0f * uiScale : 0.0f;
	float stripAreaH = remainH - effectsPanelH;
	if (stripAreaH < 80.0f * uiScale) stripAreaH = 80.0f * uiScale;

	// Horizontal layout: channel strips side by side
	int numChannels = mixer->numActiveChannels;
	if (numChannels < 1)  numChannels = 1;
	if (numChannels > GT2_MIXER_MAX_CHANNELS) numChannels = GT2_MIXER_MAX_CHANNELS;

	float channelStripW = 55.0f * uiScale;
	float masterStripW  = 60.0f * uiScale;

	for (int c = 0; c < numChannels; c++)
	{
		if (c > 0)
			ImGui::SameLine(0.0f, 8.0f * uiScale);

		// Each channel strip in its own child region
		char childId[32];
		snprintf(childId, sizeof(childId), "##ch%d", c);
		ImGui::BeginChild(childId, ImVec2(channelStripW, stripAreaH), true, ImGuiWindowFlags_NoScrollbar);

		char chLabel[16];
		snprintf(chLabel, sizeof(chLabel), "CH%d", c + 1);
		RenderChannelStrip(c, chLabel, false);

		ImGui::EndChild();
	}

	// Master bus strip
	ImGui::SameLine(0.0f, 16.0f * uiScale);
	ImGui::BeginChild("##chmaster", ImVec2(masterStripW, stripAreaH), true, ImGuiWindowFlags_NoScrollbar);
	RenderChannelStrip(-1, "MSTR", true);
	ImGui::EndChild();

	// Effects panel — shown below the strip row when a channel is expanded
	if (expandedEffectsChannel != -1)
	{
		ImGui::Separator();

		GT2MixerChannel *ch = nullptr;
		const char *panelTitle = "";
		if (expandedEffectsChannel == -2)
		{
			ch = &mixer->masterBus;
			panelTitle = "Master Bus Effects";
		}
		else if (expandedEffectsChannel >= 0 && expandedEffectsChannel < GT2_MIXER_MAX_CHANNELS)
		{
			ch = &mixer->channels[expandedEffectsChannel];
			char buf[32];
			snprintf(buf, sizeof(buf), "CH%d Effects", expandedEffectsChannel + 1);
			panelTitle = buf;
		}

		if (ch != nullptr)
		{
			ImGui::Text("%s", panelTitle);

			// List existing effects
			int removeIdx = -1;
			for (int i = 0; i < (int)ch->effects.size(); i++)
			{
				IGT2AudioEffect *fx = ch->effects[i];
				ImGui::PushID(i);

				bool open = ImGui::TreeNode(fx->GetName());
				ImGui::SameLine();
				if (ImGui::SmallButton("Reset"))
					fx->Reset();
				ImGui::SameLine();
				if (ImGui::SmallButton("X"))
					removeIdx = i;

				if (open)
				{
					fx->RenderParamsImGui();
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			if (removeIdx >= 0)
			{
				delete ch->effects[removeIdx];
				ch->effects.erase(ch->effects.begin() + removeIdx);
			}

			// Add Effect combo
			ImGui::Separator();
			static int selectedEffect = 0;
			char effectButton[64];
			snprintf(effectButton, sizeof(effectButton), "%s##fxtype", gEffectNames[selectedEffect]);
			if (ImGui::Button(effectButton, ImVec2(120.0f * uiScale, 0.0f)))
				ImGui::OpenPopup("##fxtype_popup");

			// Combo popups are not part of GT2 zoom; restore app-wide style while open.
			ImGui::PopStyleVar(2);
			if (ImGui::BeginPopup("##fxtype_popup"))
			{
				for (int i = 0; i < gNumEffects; i++)
				{
					bool selected = (selectedEffect == i);
					if (ImGui::Selectable(gEffectNames[i], selected))
						selectedEffect = i;
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndPopup();
			}
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePadding);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, itemSpacing);
			ImGui::SameLine();
			if (ImGui::Button("Add Effect"))
			{
				if (selectedEffect == GT2_EFFECT_BIQUAD_FILTER)
					ch->effects.push_back(new CGT2EffectBiquadFilter());
			}
		}
	}

	mixer->ApplyVoiceMutes(gt2_voice_mute, gNumGT2SidVoices);
	ImGui::PopStyleVar(2);
	GT2_PropagateChildWindowFocus(this);
	PostRenderImGui();
}

bool CViewGT2Mixer::KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	// Forward keystrokes to the tracker so editing continues while the mixer
	// window holds focus.
	return GT2_HandleRenoiseOrForwardKeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}

bool CViewGT2Mixer::KeyUp(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	GT2_ForwardKeyUp(keyCode);
	return true;
}

bool CViewGT2Mixer::KeyDownRepeat(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	return KeyDown(keyCode, isShift, isAlt, isControl, isSuper);
}
