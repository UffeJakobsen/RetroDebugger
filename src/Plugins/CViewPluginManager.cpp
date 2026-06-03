#include "CViewPluginManager.h"
#include "CPluginsManager.h"
#include "imgui.h"
#include <string>

CViewPluginManager::CViewPluginManager(float posX, float posY, float posZ, float sizeX, float sizeY)
: CGuiView(posX, posY, posZ, sizeX, sizeY)
{
	this->name = "Plugin Manager";
}

CViewPluginManager::~CViewPluginManager()
{
}

void CViewPluginManager::RenderImGui()
{
	PreRenderImGui();

	for (auto &entry : gPluginsManager->entries)
	{
		ImGui::Text("%-30s", entry.displayName);
		ImGui::SameLine();
		std::string btnLabel = entry.isActive
			? (std::string("Deactivate##") + entry.name)
			: (std::string("Activate##")   + entry.name);
		if (ImGui::Button(btnLabel.c_str(), ImVec2(100, 0)))
		{
			if (entry.isActive)
				gPluginsManager->Deactivate(entry.name);
			else
				gPluginsManager->Activate(entry.name);
		}
	}

	PostRenderImGui();
}
