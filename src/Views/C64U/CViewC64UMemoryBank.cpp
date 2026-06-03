#include "CViewC64UMemoryBank.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/State/C64ULogicalStateCache.h"

#include "imgui.h"

CViewC64UMemoryBank::CViewC64UMemoryBank(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
}

void CViewC64UMemoryBank::Render()
{
}

void CViewC64UMemoryBank::RenderImGui()
{
	PreRenderImGui();

	C64ULogicalStateCache *stateCache = debugInterface->GetLogicalStateCache();
	if (stateCache == NULL)
	{
		ImGui::TextDisabled("Bank state not available");
		PostRenderImGui();
		return;
	}

	C64UBankState bank = stateCache->GetBankState();

	ImGui::Text("C64U Memory Bank (read-only)");
	ImGui::Separator();
	ImGui::Text("Processor Port $01: %02X", bank.processorPort01);
	ImGui::Text("EXROM: %s  GAME: %s", bank.exrom ? "HIGH" : "LOW", bank.game ? "HIGH" : "LOW");
	ImGui::Separator();
	ImGui::Text("Screen:  $%04X", bank.screenAddress);
	ImGui::Text("Charset: $%04X", bank.charsetAddress);
	ImGui::Text("Bitmap:  $%04X", bank.bitmapAddress);

	PostRenderImGui();
}
