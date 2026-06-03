#ifndef _CPluginsManager_h_
#define _CPluginsManager_h_

#include "SYS_Defs.h"
#include <list>
#include <cstring>

struct CPluginEntry {
	const char *name;         // internal key, e.g. "Remapper"
	const char *displayName;  // shown in UI, e.g. "Texture Remapper"
	void (*activateFn)();
	void (*deactivateFn)();
	bool isActive;
	// When true, the Plugins menu renders a generic toggle item for this plugin.
	// Set false when the plugin owns a custom submenu (e.g. GoatTracker) or has no UI.
	bool showInMenu;
};

class CPluginsManager
{
public:
	CPluginsManager();

	void DeclarePlugin(const char *name, const char *displayName,
	                   void (*activateFn)(), void (*deactivateFn)(),
	                   bool showInMenu = true);

	void Activate(const char *name);
	void Deactivate(const char *name);
	bool IsActive(const char *name);

	void SaveConfig();
	void RestoreConfig();

	std::list<CPluginEntry> entries;
};

extern CPluginsManager *gPluginsManager;

#endif //_CPluginsManager_h_
