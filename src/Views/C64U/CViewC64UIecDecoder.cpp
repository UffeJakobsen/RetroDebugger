#include "CViewC64UIecDecoder.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/Trace/C64UTraceBuffer.h"
#include "../../Emulators/c64u/Trace/C64UDebugEntry.h"

#include "imgui.h"

CViewC64UIecDecoder::CViewC64UIecDecoder(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
}

void CViewC64UIecDecoder::Render()
{
}

void CViewC64UIecDecoder::RenderImGui()
{
	PreRenderImGui();

	C64UTraceBuffer *traceBuffer = debugInterface->GetTraceBuffer();
	uint64_t totalEntries = traceBuffer ? traceBuffer->GetEntryCount() : 0;
	if (!traceBuffer || totalEntries == 0)
	{
		ImGui::TextDisabled("No trace data. Use 1541 or 6510+1541 trace mode.");
		PostRenderImGui();
		return;
	}

	ImGui::Text("IEC Bus Activity (1541 mode)");
	ImGui::Separator();

	if (ImGui::BeginTable("IecBus", 5, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Cycle", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("ATN", ImGuiTableColumnFlags_WidthFixed, 30);
		ImGui::TableSetupColumn("CLK", ImGuiTableColumnFlags_WidthFixed, 30);
		ImGui::TableSetupColumn("DATA", ImGuiTableColumnFlags_WidthFixed, 35);
		ImGui::TableSetupColumn("Bus Data", ImGuiTableColumnFlags_WidthFixed, 50);
		ImGui::TableHeadersRow();

		uint64_t start = (totalEntries > 500) ? totalEntries - 500 : 0;
		bool lastAtn = false, lastClk = false, lastData = false;
		int shown = 0;

		for (uint64_t i = start; i < totalEntries && shown < 100; i++)
		{
			C64UDebugEntry entry = traceBuffer->GetEntry(i);
			bool atn = entry.GetAtn();
			bool clk = entry.GetIecClock();
			bool data = entry.GetIecData();

			if (i == start || atn != lastAtn || clk != lastClk || data != lastData)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::Text("%llu", (unsigned long long)i);
				ImGui::TableSetColumnIndex(1); ImGui::Text(atn ? "H" : "L");
				ImGui::TableSetColumnIndex(2); ImGui::Text(clk ? "H" : "L");
				ImGui::TableSetColumnIndex(3); ImGui::Text(data ? "H" : "L");
				ImGui::TableSetColumnIndex(4); ImGui::Text("%02X", entry.data);
				lastAtn = atn; lastClk = clk; lastData = data;
				shown++;
			}
		}
		ImGui::EndTable();
	}

	PostRenderImGui();
}
