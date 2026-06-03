#include "CViewC64UBusTrace.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/Trace/C64UTraceBuffer.h"
#include "../../Emulators/c64u/Trace/C64UDebugEntry.h"

#include "imgui.h"

CViewC64UBusTrace::CViewC64UBusTrace(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
	this->autoScroll = true;
}

void CViewC64UBusTrace::Render()
{
}

void CViewC64UBusTrace::RenderImGui()
{
	PreRenderImGui();

	C64UTraceBuffer *traceBuffer = debugInterface->GetTraceBuffer();
	uint64_t totalEntries = traceBuffer ? traceBuffer->GetEntryCount() : 0;
	if (!traceBuffer || totalEntries == 0)
	{
		ImGui::TextDisabled("No trace data. Enter Trace Mode to capture bus activity.");
		PostRenderImGui();
		return;
	}

	ImGui::Checkbox("Auto-scroll", &autoScroll);
	ImGui::SameLine();
	ImGui::Text("Entries: %llu", (unsigned long long)totalEntries);

	if (ImGui::BeginTable("BusTrace", 6, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersV))
	{
		ImGui::TableSetupColumn("Cycle", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 50);
		ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed, 30);
		ImGui::TableSetupColumn("R/W", ImGuiTableColumnFlags_WidthFixed, 25);
		ImGui::TableSetupColumn("Src", ImGuiTableColumnFlags_WidthFixed, 35);
		ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		uint64_t startIdx = (totalEntries > 200) ? totalEntries - 200 : 0;

		for (uint64_t i = startIdx; i < totalEntries; i++)
		{
			C64UDebugEntry entry = traceBuffer->GetEntry(i);
			ImGui::TableNextRow();

			if (!entry.rw)
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(80, 20, 20, 255));

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%llu", (unsigned long long)i);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("$%04X", entry.address);
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%02X", entry.data);
			ImGui::TableSetColumnIndex(3);
			ImGui::Text(entry.rw ? "R" : "W");
			ImGui::TableSetColumnIndex(4);
			ImGui::Text(entry.GetPhi2() ? "CPU" : "VIC");
			ImGui::TableSetColumnIndex(5);
			if (entry.address >= 0xD000 && entry.address <= 0xDFFF)
				ImGui::TextDisabled("I/O");
			else if (!entry.rw)
				ImGui::TextDisabled("Write");
		}

		if (autoScroll)
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndTable();
	}

	PostRenderImGui();
}
