#ifndef _C64UMULTICASTHELPER_H_
#define _C64UMULTICASTHELPER_H_

#include "enet.h"

class C64UMulticastHelper
{
public:
	// Join a multicast group on the given ENet socket.
	// groupAddr: multicast group address (e.g., "239.0.1.64")
	// interfaceAddr: local interface address (e.g., "0.0.0.0" for any)
	// Returns true on success, false on failure.
	static bool JoinMulticastGroup(ENetSocket socket, const char *groupAddr, const char *interfaceAddr);
};

#endif
