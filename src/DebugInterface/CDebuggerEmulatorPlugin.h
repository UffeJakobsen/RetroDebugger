#ifndef _CDebuggerEmulatorPlugin_H_
#define _CDebuggerEmulatorPlugin_H_

#include "SYS_Defs.h"
#include "CDebuggerApi.h"
#include <list>
#include <string>

// this plugin abstract class is intended for additional work done with emulator
// example: start vice emulator and do something with it, like own function for painting

class CDebugInterface;
class CSlrString;

class CDebuggerEmulatorPlugin
{
public:
	CDebuggerEmulatorPlugin();
	CDebuggerEmulatorPlugin(u8 emulatorType);
	virtual ~CDebuggerEmulatorPlugin();
	
	u8 emulatorType;
	virtual void SetEmulatorType(u8 emulatorType);
	virtual CDebugInterface *GetDebugInterface();
	
	virtual void Init();
	virtual void DoFrame();
	virtual bool HandleOpenFileShortcut();
	virtual void AddOpenFileExtensions(std::list<std::string> *extensions, bool isKeyboardShortcut);
	virtual bool OpenFile(CSlrString *path, bool isKeyboardShortcut);
	
	// returns key
	virtual u32 KeyDown(u32 keyCode);
	virtual u32 KeyUp(u32 keyCode);
	
	// returns is consumed
	virtual bool ScreenMouseDown(float x, float y);
	virtual bool ScreenMouseUp(float x, float y);

	// Generic plugin top-level menu. CMainMenuBar iterates every registered
	// plugin and, for each that returns a non-NULL name, shows a top-level
	// menu populated by RenderMainMenuImGui(). Default: no menu.
	virtual const char *GetMainMenuName();
	virtual void RenderMainMenuImGui();

	static void RegisterPlugin(CDebuggerEmulatorPlugin *plugin);
	static void UnregisterPlugin(CDebuggerEmulatorPlugin *plugin);
};

#endif
