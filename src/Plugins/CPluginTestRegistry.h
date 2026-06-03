#pragma once

// Self-registration registry for plugin CTestSuite tests.
//
// Each plugin's src/Plugins/<Plugin>/tests/<Plugin>Tests.cpp defines its
// <Plugin>_RegisterTests() and registers it here at static-init time. There is
// deliberately NO central list of plugins (that would duplicate
// C64D_InitPlugins.cpp): the core suite calls C64D_RegisterPluginTests() once
// and every plugin that linked in its <Plugin>Tests.cpp is picked up. Adding a
// plugin's tests is fully local to that plugin; removing a plugin's source
// folder simply stops its self-registration — nothing else to edit.
//
// Default vs optional:
//   C64D_REGISTER_PLUGIN_TESTS(fn)          -> default: runs in the normal suite
//                                              (kept plugins, e.g. GoatTracker).
//   C64D_REGISTER_PLUGIN_TESTS_OPTIONAL(fn) -> optional: NOT in the default
//                                              suite; included only when opted in
//                                              (--all-plugin-tests, or when a
//                                              specific test is run by name).
// The removable plugins (slated for deletion before release) register optional,
// so the default suite stays core + GoatTracker only.

#include "CTest.h"
#include <memory>
#include <vector>

typedef void (*C64D_PluginTestRegisterFn)(std::vector<std::unique_ptr<CTest> > &tests);

// Register a plugin registrar (isDefault=false => optional). Returns int so it
// can seed a static initializer.
int C64D_AddPluginTestRegistrar(C64D_PluginTestRegisterFn fn, bool isDefault);

// When true, C64D_RegisterPluginTests() also runs the optional registrars.
// Default is false (core + default plugins only).
void C64D_SetIncludeOptionalPluginTests(bool include);

// Invoke the registered plugin registrars (default ones always; optional ones
// only when C64D_SetIncludeOptionalPluginTests(true) was called).
void C64D_RegisterPluginTests(std::vector<std::unique_ptr<CTest> > &tests);

// Place at file scope in a <Plugin>Tests.cpp, after the registrar definition.
#define C64D_REGISTER_PLUGIN_TESTS(fn) \
	static int s_##fn##_registered = C64D_AddPluginTestRegistrar(&fn, true)
#define C64D_REGISTER_PLUGIN_TESTS_OPTIONAL(fn) \
	static int s_##fn##_registered = C64D_AddPluginTestRegistrar(&fn, false)
