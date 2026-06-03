#include "C64UDebugStream.h"
#include "../Trace/C64UTraceBuffer.h"
#include "enet.h"
#include "DBG_Log.h"

#include <chrono>
#include <cstring>

C64UDebugStream::C64UDebugStream()
	: port(11002), fixtureMode(false), isRunning(false),
	  udpSocket(ENET_SOCKET_NULL), traceBuffer(NULL),
	  sequenceGapCount(0), lastSeq(0)
{
}

C64UDebugStream::~C64UDebugStream()
{
	Stop();
}

void C64UDebugStream::SetEndpoint(const std::string &host, int port)
{
	this->host = host;
	this->port = port;
}

void C64UDebugStream::Start(bool fixtureMode)
{
	Stop();
	this->fixtureMode = fixtureMode;
	lastSeq = 0;
	sequenceGapCount = 0;

	if (!fixtureMode)
	{
		ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
		if (sock == ENET_SOCKET_NULL)
		{
			LOGD("C64UDebugStream::Start: failed to create UDP socket");
			return;
		}

		enet_socket_set_option(sock, ENET_SOCKOPT_NONBLOCK, 1);
		enet_socket_set_option(sock, ENET_SOCKOPT_RCVBUF, 1024 * 1024);

		ENetAddress bindAddr;
		bindAddr.host = ENET_HOST_ANY;
		bindAddr.port = (enet_uint16)port;
		if (enet_socket_bind(sock, &bindAddr) < 0)
		{
			LOGD("C64UDebugStream::Start: failed to bind to port %d", port);
			enet_socket_destroy(sock);
			return;
		}

		udpSocket = sock;
	}

	isRunning = true;
	workerThread = std::thread(&C64UDebugStream::WorkerLoop, this);
}

void C64UDebugStream::Stop()
{
	if (!isRunning.exchange(false))
		return;

	if (workerThread.joinable())
		workerThread.join();

	if (udpSocket != ENET_SOCKET_NULL)
	{
		enet_socket_destroy(udpSocket);
		udpSocket = ENET_SOCKET_NULL;
	}
}

void C64UDebugStream::SetTraceBuffer(C64UTraceBuffer *buffer)
{
	traceBuffer = buffer;
}

uint64_t C64UDebugStream::GetSequenceGapCount() const
{
	return sequenceGapCount.load();
}

bool C64UDebugStream::IsRunning() const
{
	return isRunning.load();
}

// ---------------------------------------------------------------------------
// Static: Parse a debug stream packet
// ---------------------------------------------------------------------------

bool C64UDebugStream::ParsePacket(const uint8_t *data, int dataLen,
								  uint16_t &outSeq, uint32_t *outEntries, int maxEntries,
								  int &outEntryCount)
{
	if (data == NULL || dataLen < PACKET_SIZE)
		return false;

	outSeq = (uint16_t)(data[0] | (data[1] << 8));

	int available = (dataLen - HEADER_SIZE) / 4;
	outEntryCount = (available < maxEntries) ? available : maxEntries;
	if (outEntryCount > ENTRIES_PER_PACKET)
		outEntryCount = ENTRIES_PER_PACKET;

	for (int i = 0; i < outEntryCount; i++)
	{
		int offset = HEADER_SIZE + i * 4;
		outEntries[i] = (uint32_t)data[offset]
					  | ((uint32_t)data[offset + 1] << 8)
					  | ((uint32_t)data[offset + 2] << 16)
					  | ((uint32_t)data[offset + 3] << 24);
	}

	return true;
}

// ---------------------------------------------------------------------------
// Worker thread entry point
// ---------------------------------------------------------------------------

void C64UDebugStream::WorkerLoop()
{
	if (fixtureMode)
	{
		while (isRunning)
		{
			GenerateFixtureData();
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
		}
	}
	else
	{
		uint8_t recvBuf[2048];

		while (isRunning)
		{
			ENetBuffer enetBuf;
			enetBuf.data = recvBuf;
			enetBuf.dataLength = sizeof(recvBuf);
			ENetAddress senderAddr;

			int received = enet_socket_receive(udpSocket, &senderAddr, &enetBuf, 1);

			if (received <= 0)
			{
				std::this_thread::sleep_for(std::chrono::microseconds(200));
				continue;
			}

			if (received < PACKET_SIZE)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			uint16_t seq;
			uint32_t entries[ENTRIES_PER_PACKET];
			int entryCount = 0;

			if (!ParsePacket(recvBuf, received, seq, entries, ENTRIES_PER_PACKET, entryCount))
				continue;

			// Sequence gap detection (uint16 arithmetic handles wrap correctly)
			if (lastSeq != 0)
			{
				uint16_t expected = (uint16_t)(lastSeq + 1);
				if (seq != expected)
				{
					uint16_t gap = (uint16_t)(seq - expected);
					if (gap < 0x8000)  // forward gap, not massive reordering
						sequenceGapCount.fetch_add(gap);
				}
			}
			lastSeq = seq;

			// Append to trace buffer
			if (traceBuffer != NULL && entryCount > 0)
			{
				traceBuffer->Append(entries, entryCount);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Fixture mode: generate synthetic NOP-loop pattern
// ---------------------------------------------------------------------------

void C64UDebugStream::GenerateFixtureData()
{
	if (traceBuffer == NULL)
		return;

	uint32_t entries[ENTRIES_PER_PACKET];

	// Generate a repeating NOP-loop pattern:
	// NOP at $0810 (opcode fetch): PHI2=1, R/W=read, data=0xEA, addr=$0810
	// NOP cycle 2 at $0811: PHI2=1, R/W=read, data=0xEA, addr=$0811
	for (int i = 0; i < ENTRIES_PER_PACKET; i += 2)
	{
		// First cycle: opcode fetch at $0810
		entries[i] = (1u << 31) | (1u << 24) | (0xEAu << 16) | 0x0810u;

		// Second cycle: operand read at $0811
		if (i + 1 < ENTRIES_PER_PACKET)
		{
			entries[i + 1] = (1u << 31) | (1u << 24) | (0xEAu << 16) | 0x0811u;
		}
	}

	traceBuffer->Append(entries, ENTRIES_PER_PACKET);
}

uint64_t C64UDebugStream::GetCurrentTimeMillis() const
{
	return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}
