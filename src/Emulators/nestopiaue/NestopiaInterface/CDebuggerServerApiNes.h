#ifndef _CDebuggerServerApiNes_h_
#define _CDebuggerServerApiNes_h_

#include "CDebuggerServerApi.h"

class CDebugInterfaceNes;

class CDebuggerServerApiNes : public CDebuggerServerApi
{
public:
	CDebuggerServerApiNes(CDebugInterface *debugInterface);

	CDebugInterfaceNes *debugInterfaceNes;

	virtual void RegisterEndpoints(CDebuggerServer *server);
};

#endif
