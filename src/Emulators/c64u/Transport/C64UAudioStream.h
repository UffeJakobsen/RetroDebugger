#ifndef _C64UAUDIOSTREAM_H_
#define _C64UAUDIOSTREAM_H_

#include "enet.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

class C64UAudioJitterBuffer;

class C64UAudioStream
{
public:
	static const int HEADER_SIZE = 2;   // 2-byte sequence number (u16 LE)

	C64UAudioStream();
	~C64UAudioStream();

	void SetEndpoint(const std::string &host, int port);
	void Start(bool fixtureMode);
	void Stop();
	void SetJitterBuffer(C64UAudioJitterBuffer *buffer);
	void SetUseMulticast(bool useMulticast);

	uint64_t GetSequenceGapCount() const;
	uint64_t GetPacketsReceived() const;
	bool IsRunning() const;

	// ---------------------------------------------------------------
	// Static methods: testable without network or instance state.
	// ---------------------------------------------------------------

	// Parse a raw audio packet: 2-byte seq header + i16 LE stereo interleaved samples.
	// Converts i16 to float [-1.0, ~0.99997].
	// Returns the number of stereo sample pairs decoded.
	static int ParsePacket(const uint8_t *data, int dataLen,
						   float *outInterleavedLR, int maxStereoSamples);

private:
	void WorkerLoop();
	void GenerateFixtureAudio();
	void ReceiveLoop();
	uint64_t GetCurrentTimeMillis() const;

	std::string host;
	int port;
	bool fixtureMode;
	bool useMulticast;

	std::atomic<bool> isRunning;
	std::thread workerThread;

	C64UAudioJitterBuffer *jitterBuffer;

	// Sequence tracking
	uint16_t lastSeq;
	std::atomic<uint64_t> sequenceGapCount;
	std::atomic<uint64_t> packetsReceived;

	// ENet UDP socket handle (ENET_SOCKET_NULL when not connected).
	ENetSocket udpSocket;

	// Fixture mode: sine wave phase accumulators
	float fixturePhaseL;
	float fixturePhaseR;
};

#endif
