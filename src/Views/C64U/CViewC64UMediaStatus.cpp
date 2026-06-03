#include "CViewC64UMediaStatus.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/Transport/C64URestClient.h"

#include "imgui.h"
#include "SYS_Main.h"

CViewC64UMediaStatus::CViewC64UMediaStatus(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
	this->lastDriveInfoFetchTime = 0;
}

void CViewC64UMediaStatus::Render()
{
}

void CViewC64UMediaStatus::RenderImGui()
{
	PreRenderImGui();
	ImGui::TextWrapped("%s", debugInterface->GetMediaStatusString());

	// Periodically request drive info (every 5 seconds)
	uint64_t now = SYS_GetCurrentTimeInMillis();
	if (now - lastDriveInfoFetchTime > 5000)
	{
		debugInterface->ScheduleGetDriveInfo();
		lastDriveInfoFetchTime = now;
	}

	PostRenderImGui();
}
