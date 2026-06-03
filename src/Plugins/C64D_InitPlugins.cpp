#include "C64D_InitPlugins.h"
#include "CViewC64.h"
#include "CPluginsManager.h"
#include "CDebuggerEmulatorPlugin.h"

#include "C64DebuggerPluginDummy.h"
#include "C64DebuggerPluginTemplate.h"
#include "C64DebuggerPluginCrtMaker.h"
#include "C64DebuggerPluginGoatTracker.h"
#include "FlameTiles/C64DebuggerPluginFlameTiles.h"
#include "C64DebuggerPluginDNDK.h"
//#include "C64DebuggerPluginCommando.h"
//#include "C64DebuggerPluginShowPic.h"
//#include "C64DebuggerPluginParallax.h"
//#include "C64DebuggerPluginRasterBars.h"
//#include "C64DebuggerPluginPicFader.h"
//#include "C64DebuggerPluginConvolution.h"

static void GoatTrackerActivate()
{
	if (pluginGoatTracker == NULL)
	{
		PLUGIN_GoatTrackerInit();
	}
	else
	{
		viewC64->RegisterEmulatorPlugin(pluginGoatTracker);
	}
	PLUGIN_GoatTrackerSetVisible(true);
}

static void GoatTrackerDeactivate()
{
	if (pluginGoatTracker != NULL)
	{
		PLUGIN_GoatTrackerSetVisible(false);
		CDebuggerEmulatorPlugin::UnregisterPlugin(pluginGoatTracker);
	}
}

static void FlameActivate()
{
	if (pluginFlameTiles == NULL)
	{
		PLUGIN_FlameTilesInit();
	}
	else
	{
		viewC64->RegisterEmulatorPlugin(pluginFlameTiles);
	}
	PLUGIN_FlameTilesSetVisible(true);
}

static void FlameDeactivate()
{
	if (pluginFlameTiles != NULL)
	{
		PLUGIN_FlameTilesSetVisible(false);
		CDebuggerEmulatorPlugin::UnregisterPlugin(pluginFlameTiles);
	}
}

static void DdnkActivate()
{
	if (pluginDDNK == NULL)
	{
		PLUGIN_DdnkInit();
	}
	else
	{
		viewC64->RegisterEmulatorPlugin(pluginDDNK);
	}
	PLUGIN_DdnkSetVisible(true);
}

static void DdnkDeactivate()
{
	if (pluginDDNK != NULL)
	{
		PLUGIN_DdnkSetVisible(false);
		CDebuggerEmulatorPlugin::UnregisterPlugin(pluginDDNK);
	}
}

void C64D_InitPlugins()
{
	gPluginsManager = new CPluginsManager();

	// GoatTracker shows a generic open/close toggle in the Plugins menu; its
	// own commands live in a dedicated top-level menu contributed via the
	// plugin menu API (CDebuggerEmulatorPlugin::GetMainMenuName).
	gPluginsManager->DeclarePlugin("GoatTracker", "Goat Tracker 2",   GoatTrackerActivate, GoatTrackerDeactivate, true);
	PLUGIN_GoatTrackerRestoreSettings();
	gPluginsManager->DeclarePlugin("DNDK",        "DNDK Trainer",     DdnkActivate,        DdnkDeactivate);
//	gPluginsManager->DeclarePlugin("FlameTiles",  "Candle Flame (tile-based, abandoned)", FlameActivate, FlameDeactivate);

	if (crtMakerConfigFilePath != NULL)
	{
		C64DebuggerPluginCrtMaker *plugin = new C64DebuggerPluginCrtMaker();
		viewC64->RegisterEmulatorPlugin(plugin);
	}
}
