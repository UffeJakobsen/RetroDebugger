#include "CViewC64UTimeline.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/Trace/C64UTraceBuffer.h"
#include "../../Emulators/c64u/Trace/C64UDebugEntry.h"

#include "imgui.h"

CViewC64UTimeline::CViewC64UTimeline(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
	this->cyclesPerPixel = 1.0f;
}

void CViewC64UTimeline::Render()
{
}

void CViewC64UTimeline::RenderImGui()
{
	PreRenderImGui();

	C64UTraceBuffer *traceBuffer = debugInterface->GetTraceBuffer();
	uint64_t totalEntries = traceBuffer ? traceBuffer->GetEntryCount() : 0;
	if (!traceBuffer || totalEntries == 0)
	{
		ImGui::TextDisabled("No trace data.");
		PostRenderImGui();
		return;
	}

	ImGui::SliderFloat("Zoom", &cyclesPerPixel, 0.1f, 100.0f, "%.1f cycles/px");

	ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	if (canvasSize.y < 100) canvasSize.y = 100;

	ImDrawList *drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(20, 20, 30, 255));

	float laneHeight = canvasSize.y / 4.0f;
	int visibleCycles = (int)(canvasSize.x * cyclesPerPixel);
	uint64_t startIdx = (totalEntries > (uint64_t)visibleCycles) ? totalEntries - visibleCycles : 0;

	for (int px = 0; px < (int)canvasSize.x && (startIdx + (uint64_t)(px * cyclesPerPixel)) < totalEntries; px++)
	{
		uint64_t idx = startIdx + (uint64_t)(px * cyclesPerPixel);
		C64UDebugEntry entry = traceBuffer->GetEntry(idx);
		float x = canvasPos.x + px;

		// Address lane
		float addrY = canvasPos.y + laneHeight - (entry.address / 65535.0f) * laneHeight;
		drawList->AddLine(ImVec2(x, canvasPos.y + laneHeight), ImVec2(x, addrY), IM_COL32(100, 150, 255, 200));

		// Data lane
		float dataY = canvasPos.y + laneHeight + laneHeight - (entry.data / 255.0f) * laneHeight;
		drawList->AddLine(ImVec2(x, canvasPos.y + 2 * laneHeight), ImVec2(x, dataY), IM_COL32(150, 255, 100, 200));

		// R/W lane
		float rwY = canvasPos.y + 2 * laneHeight + (entry.rw ? 0 : laneHeight);
		drawList->AddRectFilled(ImVec2(x, canvasPos.y + 2 * laneHeight), ImVec2(x + 1, rwY + 2),
			entry.rw ? IM_COL32(80, 200, 80, 200) : IM_COL32(200, 80, 80, 200));

		// IRQ/NMI lane
		float sigY = canvasPos.y + 3 * laneHeight;
		if (!entry.GetIrq()) drawList->AddRectFilled(ImVec2(x, sigY), ImVec2(x + 1, sigY + laneHeight * 0.5f), IM_COL32(255, 255, 0, 200));
		if (!entry.GetNmi()) drawList->AddRectFilled(ImVec2(x, sigY + laneHeight * 0.5f), ImVec2(x + 1, sigY + laneHeight), IM_COL32(255, 128, 0, 200));
	}

	// Lane labels
	drawList->AddText(canvasPos, IM_COL32(200, 200, 200, 255), "ADDR");
	drawList->AddText(ImVec2(canvasPos.x, canvasPos.y + laneHeight), IM_COL32(200, 200, 200, 255), "DATA");
	drawList->AddText(ImVec2(canvasPos.x, canvasPos.y + 2 * laneHeight), IM_COL32(200, 200, 200, 255), "R/W");
	drawList->AddText(ImVec2(canvasPos.x, canvasPos.y + 3 * laneHeight), IM_COL32(200, 200, 200, 255), "IRQ/NMI");

	// Make the canvas interactive (for future pan/zoom)
	ImGui::Dummy(canvasSize);

	PostRenderImGui();
}
