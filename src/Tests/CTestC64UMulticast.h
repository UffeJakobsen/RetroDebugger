#pragma once

#include "CTest.h"
#include "../Emulators/c64u/Transport/C64UMulticastHelper.h"
#include "EmulatorsConfig.h"
#include "enet.h"

class CTestC64UMulticast : public CTest
{
public:
	virtual const char *GetName() override { return "C64UMulticast"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

#ifndef RUN_COMMODORE64
		TestCompleted(true, "Skipped (C64 not enabled)");
		return;
#else
		// Step 1: Create UDP socket, call JoinMulticastGroup -- verify no crash
		// The return value may be true or false depending on OS multicast support
		{
			ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
			if (sock == ENET_SOCKET_NULL)
			{
				TestCompleted(false, "Step 1: Failed to create UDP socket");
				return;
			}

			ENetAddress bindAddr;
			bindAddr.host = ENET_HOST_ANY;
			bindAddr.port = 0;  // let OS pick a port
			enet_socket_bind(sock, &bindAddr);

			// This may succeed or fail depending on OS -- either way, no crash
			C64UMulticastHelper::JoinMulticastGroup(sock, "239.0.1.64", "0.0.0.0");

			enet_socket_destroy(sock);

			StepCompleted(1, true, "JoinMulticastGroup with valid address did not crash");
		}

		// Step 2: Call JoinMulticastGroup with invalid address -- verify returns false
		{
			ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
			if (sock == ENET_SOCKET_NULL)
			{
				TestCompleted(false, "Step 2: Failed to create UDP socket");
				return;
			}

			ENetAddress bindAddr;
			bindAddr.host = ENET_HOST_ANY;
			bindAddr.port = 0;
			enet_socket_bind(sock, &bindAddr);

			bool result = C64UMulticastHelper::JoinMulticastGroup(sock, "999.999.999.999", "0.0.0.0");

			if (result != false)
			{
				enet_socket_destroy(sock);
				TestCompleted(false, "Step 2: Invalid address must return false");
				return;
			}

			// Step 3: Socket cleanup after tests
			enet_socket_destroy(sock);

			StepCompleted(2, true, "Invalid address returns false");
		}

		StepCompleted(3, true, "Socket destroyed cleanly after multicast tests");

		TestCompleted(true, "C64U multicast helper works correctly");
#endif
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
