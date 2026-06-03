#include "C64ULocalIpDetect.h"
#include "enet.h"
#include "DBG_Log.h"

std::string C64ULocalIpDetect::Detect(const std::string &remoteHost, int remotePort)
{
	if (remoteHost.empty() || remotePort <= 0)
		return std::string();

	ENetAddress remoteAddr;
	if (enet_address_set_host_ip(&remoteAddr, remoteHost.c_str()) < 0)
	{
		// Try DNS resolution as fallback
		if (enet_address_set_host(&remoteAddr, remoteHost.c_str()) < 0)
		{
			LOGD("C64ULocalIpDetect::Detect: cannot resolve host '%s'", remoteHost.c_str());
			return std::string();
		}
	}
	remoteAddr.port = (enet_uint16)remotePort;

	ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
	if (sock == ENET_SOCKET_NULL)
	{
		LOGD("C64ULocalIpDetect::Detect: cannot create UDP socket");
		return std::string();
	}

	if (enet_socket_connect(sock, &remoteAddr) < 0)
	{
		LOGD("C64ULocalIpDetect::Detect: connect failed");
		enet_socket_destroy(sock);
		return std::string();
	}

	ENetAddress localAddr;
	if (enet_socket_get_address(sock, &localAddr) < 0)
	{
		LOGD("C64ULocalIpDetect::Detect: getsockname failed");
		enet_socket_destroy(sock);
		return std::string();
	}

	char buf[64];
	if (enet_address_get_host_ip(&localAddr, buf, sizeof(buf)) < 0)
	{
		LOGD("C64ULocalIpDetect::Detect: cannot format local address");
		enet_socket_destroy(sock);
		return std::string();
	}

	enet_socket_destroy(sock);
	return std::string(buf);
}
