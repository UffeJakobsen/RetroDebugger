#ifndef _CDebuggerServerApiAtari_h_
#define _CDebuggerServerApiAtari_h_

#include "CDebuggerServerApi.h"

class CDebugInterfaceAtari;

class CDebuggerServerApiAtari : public CDebuggerServerApi
{
public:
	CDebuggerServerApiAtari(CDebugInterface *debugInterface);

	CDebugInterfaceAtari *debugInterfaceAtari;

	virtual void RegisterEndpoints(CDebuggerServer *server);
};

#endif
