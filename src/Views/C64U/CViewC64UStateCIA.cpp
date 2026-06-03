#include "CViewC64UStateCIA.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/State/C64ULogicalStateCache.h"

#include "imgui.h"

CViewC64UStateCIA::CViewC64UStateCIA(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
}

void CViewC64UStateCIA::Render()
{
}

void CViewC64UStateCIA::RenderImGui()
{
	PreRenderImGui();

	C64ULogicalStateCache *stateCache = debugInterface->GetLogicalStateCache();
	if (stateCache == NULL)
	{
		ImGui::TextDisabled("State not available");
		PostRenderImGui();
		return;
	}

	C64UCiaState cia1 = stateCache->GetCia1State();
	C64UCiaState cia2 = stateCache->GetCia2State();

	ImGui::Text("CIA1 ($DC00-$DC0F) read-only");
	ImGui::Separator();
	ImGui::Text("$DC00:");
	for (int i = 0; i < 16; i++)
	{
		ImGui::SameLine();
		ImGui::Text("%02X", cia1.registers[i]);
	}

	ImGui::Spacing();
	ImGui::Text("CIA2 ($DD00-$DD0F) read-only");
	ImGui::Separator();
	ImGui::Text("$DD00:");
	for (int i = 0; i < 16; i++)
	{
		ImGui::SameLine();
		ImGui::Text("%02X", cia2.registers[i]);
	}

	PostRenderImGui();
}
