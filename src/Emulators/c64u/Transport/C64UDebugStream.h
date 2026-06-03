#ifndef _C64UDEBUGSTREAM_H_
#define _C64UDEBUGSTREAM_H_

#include "enet.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

class C64UTraceBuffer;

class C64UDebugStream
{
public:
	static const int HEADER_SIZE = 4;            // 2-byte seq LE + 2-byte reserved
	static const int ENTRIES_PER_PACKET = 360;
	static const int PACKET_SIZE = HEADER_SIZE + ENTRIES_PER_PACKET * 4;  // 1444

	C64UDebugStream();
	~C64UDebugStream();

	void SetEndpoint(const std::string &host, int port);
	void Start(bool fixtureMode);
	void Stop();
	void SetTraceBuffer(C64UTraceBuffer *buffer);

	uint64_t GetSequenceGapCount() const;
	bool IsRunning() const;

	// ---------------------------------------------------------------
	// Static methods: testable without network or instance state.
	// ---------------------------------------------------------------
	static bool ParsePacket(const uint8_t *data, int dataLen,
							uint16_t &outSeq, uint32_t *outEntries, int maxEntries,
							int &outEntryCount);

private:
	void WorkerLoop();
	void GenerateFixtureData();
	uint64_t GetCurrentTimeMillis() const;

	std::string host;
	int port;
	bool fixtureMode;
	std::atomic<bool> isRunning;
	std::thread workerThread;
	ENetSocket udpSocket;
	C64UTraceBuffer *traceBuffer;
	std::atomic<uint64_t> sequenceGapCount;
	uint16_t lastSeq;
};

#endif
