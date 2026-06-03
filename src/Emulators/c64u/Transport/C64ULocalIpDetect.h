#ifndef _C64ULOCALIPDETECT_H_
#define _C64ULOCALIPDETECT_H_

#include <string>

// Detect the local IP address that would be used to reach a given remote host.
// Uses a connected-but-unsent UDP socket trick: create DGRAM, connect to remote
// (no data sent for UDP), getsockname to read the local address.

class C64ULocalIpDetect
{
public:
	// Returns the local IP string (e.g. "192.168.1.42") that routes to
	// remoteHost:remotePort.  Returns empty string on failure.
	static std::string Detect(const std::string &remoteHost, int remotePort);
};

#endif
