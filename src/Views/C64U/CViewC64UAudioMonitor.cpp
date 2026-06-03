#include "CViewC64UAudioMonitor.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/Audio/C64UAudioJitterBuffer.h"
#include "../../Emulators/c64u/Audio/CAudioChannelC64U.h"
#include "../../Emulators/c64u/Transport/C64UAudioStream.h"

#include "imgui.h"

#include <cmath>
#include <cstring>
#include <algorithm>

CViewC64UAudioMonitor::CViewC64UAudioMonitor(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
}

void CViewC64UAudioMonitor::Render()
{
}

void CViewC64UAudioMonitor::RenderImGui()
{
	PreRenderImGui();

	C64UAudioJitterBuffer *jitterBuf = debugInterface->GetAudioJitterBuffer();
	C64UAudioStream *audioStream = debugInterface->GetAudioStream();
	CAudioChannelC64U *audioChannel = debugInterface->GetAudioChannel();

	if (!audioStream || !audioStream->IsRunning() || !jitterBuf)
	{
		ImGui::TextDisabled("Audio stream not active");
		PostRenderImGui();
		return;
	}

	// Peek recent samples for visualization (~960 samples = 20ms at 48kHz)
	static const int PEEK_SAMPLES = 960;
	float peekL[PEEK_SAMPLES];
	float peekR[PEEK_SAMPLES];
	int peeked = jitterBuf->PeekRecentSamples(peekL, peekR, PEEK_SAMPLES);

	// --- VU Meters ---
	float rmsL = 0.0f, rmsR = 0.0f;
	float peakL = 0.0f, peakR = 0.0f;

	if (peeked > 0)
	{
		for (int i = 0; i < peeked; i++)
		{
			rmsL += peekL[i] * peekL[i];
			rmsR += peekR[i] * peekR[i];
			float absL = fabsf(peekL[i]);
			float absR = fabsf(peekR[i]);
			if (absL > peakL) peakL = absL;
			if (absR > peakR) peakR = absR;
		}
		rmsL = sqrtf(rmsL / (float)peeked);
		rmsR = sqrtf(rmsR / (float)peeked);
	}

	// Convert to dB (clamped to -60..0 dB range, normalized to 0..1 for bars)
	auto toNormDb = [](float linear) -> float
	{
		if (linear < 0.001f) return 0.0f;
		float db = 20.0f * log10f(linear);
		if (db < -60.0f) db = -60.0f;
		return (db + 60.0f) / 60.0f;
	};

	float rmsLNorm = toNormDb(rmsL);
	float rmsRNorm = toNormDb(rmsR);
	float peakLNorm = toNormDb(peakL);
	float peakRNorm = toNormDb(peakR);

	// Draw VU meter bars using ImDrawList
	ImDrawList *drawList = ImGui::GetWindowDrawList();
	ImVec2 cursorPos = ImGui::GetCursorScreenPos();
	float barWidth = 20.0f;
	float barHeight = 120.0f;
	float spacing = 8.0f;

	auto drawVUBar = [&](float x, float y, float rmsNorm, float peakNorm, const char *label)
	{
		// Background
		drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + barWidth, y + barHeight), IM_COL32(40, 40, 40, 255));

		// RMS bar
		float rmsHeight = rmsNorm * barHeight;
		float rmsTop = y + barHeight - rmsHeight;

		// Color based on level: green < -12dB (0.8 norm), yellow < -3dB (0.95 norm), red above
		ImU32 barColor;
		if (rmsNorm < 0.8f)
			barColor = IM_COL32(0, 200, 0, 255);
		else if (rmsNorm < 0.95f)
			barColor = IM_COL32(200, 200, 0, 255);
		else
			barColor = IM_COL32(200, 0, 0, 255);

		drawList->AddRectFilled(ImVec2(x, rmsTop), ImVec2(x + barWidth, y + barHeight), barColor);

		// Peak indicator line
		float peakY = y + barHeight - peakNorm * barHeight;
		drawList->AddLine(ImVec2(x, peakY), ImVec2(x + barWidth, peakY), IM_COL32(255, 255, 255, 200), 2.0f);

		// Label
		drawList->AddText(ImVec2(x + 2, y + barHeight + 2), IM_COL32(200, 200, 200, 255), label);
	};

	drawVUBar(cursorPos.x, cursorPos.y, rmsLNorm, peakLNorm, "L");
	drawVUBar(cursorPos.x + barWidth + spacing, cursorPos.y, rmsRNorm, peakRNorm, "R");

	// Reserve space for VU meters
	ImGui::Dummy(ImVec2(barWidth * 2 + spacing, barHeight + 18));

	// --- Waveform ---
	if (peeked > 0)
	{
		ImGui::Separator();
		ImGui::Text("Waveform");

		ImVec2 wavePos = ImGui::GetCursorScreenPos();
		float waveWidth = ImGui::GetContentRegionAvail().x;
		float waveHeight = 80.0f;

		drawList->AddRectFilled(wavePos, ImVec2(wavePos.x + waveWidth, wavePos.y + waveHeight), IM_COL32(20, 20, 30, 255));

		// L channel in top half, R in bottom half
		float halfHeight = waveHeight * 0.5f;
		float midL = wavePos.y + halfHeight * 0.5f;
		float midR = wavePos.y + halfHeight + halfHeight * 0.5f;

		// Draw L channel
		float step = (float)peeked / waveWidth;
		for (float x = 0; x < waveWidth - 1; x += 1.0f)
		{
			int idx0 = (int)(x * step);
			int idx1 = (int)((x + 1.0f) * step);
			if (idx0 >= peeked) idx0 = peeked - 1;
			if (idx1 >= peeked) idx1 = peeked - 1;

			float y0L = midL - peekL[idx0] * halfHeight * 0.45f;
			float y1L = midL - peekL[idx1] * halfHeight * 0.45f;
			drawList->AddLine(ImVec2(wavePos.x + x, y0L), ImVec2(wavePos.x + x + 1.0f, y1L), IM_COL32(100, 200, 100, 255));

			float y0R = midR - peekR[idx0] * halfHeight * 0.45f;
			float y1R = midR - peekR[idx1] * halfHeight * 0.45f;
			drawList->AddLine(ImVec2(wavePos.x + x, y0R), ImVec2(wavePos.x + x + 1.0f, y1R), IM_COL32(100, 100, 200, 255));
		}

		// Center lines
		drawList->AddLine(ImVec2(wavePos.x, midL), ImVec2(wavePos.x + waveWidth, midL), IM_COL32(80, 80, 80, 100));
		drawList->AddLine(ImVec2(wavePos.x, midR), ImVec2(wavePos.x + waveWidth, midR), IM_COL32(80, 80, 80, 100));

		ImGui::Dummy(ImVec2(waveWidth, waveHeight));
	}

	// --- Stats ---
	ImGui::Separator();

	uint64_t packets = audioStream->GetPacketsReceived();
	uint64_t seqGaps = audioStream->GetSequenceGapCount();
	float fillMs = jitterBuf->GetFillLevelMs(47983.0f);
	int underflows = jitterBuf->GetUnderflowCount();
	int overflows = jitterBuf->GetOverflowCount();

	ImGui::Text("Packets: %llu", (unsigned long long)packets);
	ImGui::SameLine();
	ImGui::Text("  Seq gaps: %llu", (unsigned long long)seqGaps);

	// Buffer fill with color coding
	ImU32 fillColor;
	if (fillMs >= 100.0f && fillMs <= 300.0f)
		fillColor = IM_COL32(0, 200, 0, 255);
	else if (fillMs > 300.0f && fillMs <= 400.0f)
		fillColor = IM_COL32(200, 200, 0, 255);
	else
		fillColor = IM_COL32(200, 0, 0, 255);

	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(fillColor), "Buffer: %.1f ms", fillMs);
	ImGui::SameLine();
	ImGui::Text("  Under: %d  Over: %d", underflows, overflows);

	// --- Mute Button ---
	if (audioChannel)
	{
		bool muted = audioChannel->isMuted;
		if (ImGui::Checkbox("Mute##C64UAudio", &muted))
		{
			audioChannel->isMuted = muted;
		}
	}

	PostRenderImGui();
}
