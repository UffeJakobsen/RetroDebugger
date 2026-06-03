#include "CViewC64UDisassembly.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "CViewDisassembly.h"
#include "CDebugSymbols.h"
#include "CDebugSymbolsC64.h"

#include "imgui.h"

CViewC64UDisassembly::CViewC64UDisassembly(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
	this->viewDisassembly = NULL;

	if (debugInterface->symbolsC64 != NULL && debugInterface->dataAdapterC64 != NULL)
	{
		viewDisassembly = new CViewDisassembly("C64U Disassembly Inner", posX, posY, posZ, sizeX, sizeY,
											   debugInterface->symbolsC64, NULL);
	}
}

CViewC64UDisassembly::~CViewC64UDisassembly()
{
	if (viewDisassembly != NULL)
	{
		delete viewDisassembly;
	}
}

void CViewC64UDisassembly::Render()
{
}

void CViewC64UDisassembly::RenderImGui()
{
	PreRenderImGui();
	if (viewDisassembly != NULL)
	{
		viewDisassembly->RenderImGui();
	}
	else
	{
		ImGui::TextDisabled("Disassembly not available (no data adapter)");
	}
	PostRenderImGui();
}

void CViewC64UDisassembly::DoLogic()
{
	if (viewDisassembly != NULL)
	{
		viewDisassembly->DoLogic();
	}
}
