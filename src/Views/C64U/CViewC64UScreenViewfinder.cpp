#include "CViewC64UScreenViewfinder.h"

#include "CViewC64UScreen.h"
#include "../../Emulators/c64u/CDebugInterfaceC64U.h"

#include "imgui.h"

CViewC64UScreenViewfinder::CViewC64UScreenViewfinder(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CViewC64UScreen *viewScreen, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->viewScreen = viewScreen;
	this->debugInterface = debugInterface;
	this->zoom = 2.0f;
	imGuiNoWindowPadding = true;
	imGuiNoScrollbar = true;
}

bool CViewC64UScreenViewfinder::DoScrollWheel(float deltaX, float deltaY)
{
	zoom += deltaY * 0.1f;
	if (zoom < 1.0f)
		zoom = 1.0f;
	if (zoom > 8.0f)
		zoom = 8.0f;
	return true;
}

void CViewC64UScreenViewfinder::Render()
{
}

void CViewC64UScreenViewfinder::RenderImGui()
{
	PreRenderImGui();
	ImGui::Text("Zoom: %.1fx", zoom);
	ImGui::TextDisabled("Frames: %llu", (unsigned long long)debugInterface->GetVideoFrameCounter());
	ImGui::TextDisabled("Viewfinder follows the additive C64U screen stream.");
	PostRenderImGui();
}
