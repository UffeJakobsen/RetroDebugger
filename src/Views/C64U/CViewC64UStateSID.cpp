#include "CViewC64UStateSID.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/State/C64ULogicalStateCache.h"

#include "imgui.h"

CViewC64UStateSID::CViewC64UStateSID(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
}

void CViewC64UStateSID::Render()
{
}

void CViewC64UStateSID::RenderImGui()
{
	PreRenderImGui();

	C64ULogicalStateCache *stateCache = debugInterface->GetLogicalStateCache();
	if (stateCache == NULL)
	{
		ImGui::TextDisabled("State not available");
		PostRenderImGui();
		return;
	}

	C64USidState sid = stateCache->GetSidState();

	ImGui::Text("SID ($D400-$D41F) read-only");
	ImGui::Separator();

	ImGui::Text("$D400:");
	for (int i = 0; i < 16; i++)
	{
		ImGui::SameLine();
		ImGui::Text("%02X", sid.registers[i]);
	}
	ImGui::Text("$D410:");
	for (int i = 0; i < 16; i++)
	{
		ImGui::SameLine();
		ImGui::Text("%02X", sid.registers[16 + i]);
	}

	PostRenderImGui();
}
