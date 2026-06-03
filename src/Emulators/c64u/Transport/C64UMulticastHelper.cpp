#include "C64UMulticastHelper.h"
#include "DBG_Log.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

bool C64UMulticastHelper::JoinMulticastGroup(ENetSocket socket, const char *groupAddr, const char *interfaceAddr)
{
	if (groupAddr == NULL || interfaceAddr == NULL)
		return false;

	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = inet_addr(groupAddr);
	mreq.imr_interface.s_addr = inet_addr(interfaceAddr);

	// inet_addr returns INADDR_NONE on invalid address
	if (mreq.imr_multiaddr.s_addr == INADDR_NONE)
	{
		LOGD("C64UMulticastHelper::JoinMulticastGroup: invalid group address '%s'", groupAddr);
		return false;
	}

#ifdef _WIN32
	int result = setsockopt((SOCKET)socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq));
#else
	int result = setsockopt((int)socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq));
#endif

	if (result != 0)
	{
		LOGD("C64UMulticastHelper::JoinMulticastGroup: setsockopt failed for '%s' (result=%d)", groupAddr, result);
		return false;
	}

	LOGD("C64UMulticastHelper::JoinMulticastGroup: joined '%s' on interface '%s'", groupAddr, interfaceAddr);
	return true;
}
