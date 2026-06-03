#include "CPluginsManager.h"
#include "CViewC64.h"

CPluginsManager *gPluginsManager = NULL;

CPluginsManager::CPluginsManager()
{
}

void CPluginsManager::DeclarePlugin(const char *name, const char *displayName,
                                     void (*activateFn)(), void (*deactivateFn)(),
                                     bool showInMenu)
{
	CPluginEntry entry;
	entry.name        = name;
	entry.displayName = displayName;
	entry.activateFn  = activateFn;
	entry.deactivateFn = deactivateFn;
	entry.isActive    = false;
	entry.showInMenu  = showInMenu;
	entries.push_back(entry);
}

void CPluginsManager::Activate(const char *name)
{
	for (auto &entry : entries)
	{
		if (strcmp(entry.name, name) == 0)
		{
			if (!entry.isActive)
			{
				entry.isActive = true;
				entry.activateFn();
				SaveConfig();
			}
			return;
		}
	}
}

void CPluginsManager::Deactivate(const char *name)
{
	for (auto &entry : entries)
	{
		if (strcmp(entry.name, name) == 0)
		{
			if (entry.isActive)
			{
				entry.isActive = false;
				entry.deactivateFn();
				SaveConfig();
			}
			return;
		}
	}
}

bool CPluginsManager::IsActive(const char *name)
{
	for (const auto &entry : entries)
	{
		if (strcmp(entry.name, name) == 0)
			return entry.isActive;
	}
	return false;
}

void CPluginsManager::SaveConfig()
{
	for (auto &entry : entries)
	{
		char key[256];
		snprintf(key, sizeof(key), "%sActive", entry.name);
		viewC64->config->SetBool(key, &entry.isActive);
	}
}

void CPluginsManager::RestoreConfig()
{
	for (auto &entry : entries)
	{
		char key[256];
		snprintf(key, sizeof(key), "%sActive", entry.name);
		bool wasActive = viewC64->config->GetBool(key, false);
		if (wasActive)
		{
			Activate(entry.name);
		}
	}
}
