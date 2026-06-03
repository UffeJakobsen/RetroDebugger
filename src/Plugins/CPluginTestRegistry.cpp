#include "CPluginTestRegistry.h"

namespace
{
	struct Registrar
	{
		C64D_PluginTestRegisterFn fn;
		bool isDefault;
	};

	// Function-local static avoids any static-init-order dependency: the vector
	// is constructed on the first C64D_AddPluginTestRegistrar() call (during
	// static init, before main()).
	std::vector<Registrar> &registrars()
	{
		static std::vector<Registrar> r;
		return r;
	}

	bool &includeOptional()
	{
		static bool v = false;
		return v;
	}
}

int C64D_AddPluginTestRegistrar(C64D_PluginTestRegisterFn fn, bool isDefault)
{
	if (fn != NULL)
	{
		Registrar r = { fn, isDefault };
		registrars().push_back(r);
	}
	return 0;
}

void C64D_SetIncludeOptionalPluginTests(bool include)
{
	includeOptional() = include;
}

void C64D_RegisterPluginTests(std::vector<std::unique_ptr<CTest> > &tests)
{
	for (size_t i = 0; i < registrars().size(); i++)
	{
		if (registrars()[i].isDefault || includeOptional())
			registrars()[i].fn(tests);
	}
}
