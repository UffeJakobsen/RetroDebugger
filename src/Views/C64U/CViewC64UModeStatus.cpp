#include "CViewC64UModeStatus.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"

#include "imgui.h"

CViewC64UModeStatus::CViewC64UModeStatus(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
}

void CViewC64UModeStatus::Render()
{
}

void CViewC64UModeStatus::RenderImGui()
{
	PreRenderImGui();
	ImGui::Text("Mode: %s", debugInterface->GetModeString());
	ImGui::TextDisabled("Screen and trace remain separate additive workflows.");
	PostRenderImGui();
}
