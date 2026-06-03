#ifndef _C64UVIDEOSTREAM_H_
#define _C64UVIDEOSTREAM_H_

#include "C64UNybbleLUT.h"
#include "enet.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class CImageData;

// Parsed representation of the 12-byte UDP video packet header.
struct C64UVideoPacketHeader
{
	uint16_t seq;
	uint16_t frame;
	uint16_t lineNum;        // lower 15 bits of line_raw
	bool frameDone;          // bit 15 of line_raw
	uint16_t pixelsInLine;
	uint8_t linesInPacket;
	uint8_t bpp;
	uint16_t encoding;
};

class C64UVideoStream
{
public:
	static const int FRAME_WIDTH = 384;
	static const int FRAME_HEIGHT = 272;
	static const int BYTES_PER_LINE = 192;   // 384 / 2
	static const int HEADER_SIZE = 12;

	C64UVideoStream();
	~C64UVideoStream();

	void SetEndpoint(const std::string &host, int port);
	void Start(bool generateFixtureFrames);
	void Stop();
	void SetUseMulticast(bool useMulticast);

	bool CopyLatestFrameToImage(CImageData *imageData);
	void RebuildLUT(const uint8_t palette[16][3]);
	bool HasTimedOut(uint64_t timeoutMillis) const;
	uint64_t GetMillisecondsSinceLastFrame() const;
	uint64_t GetFrameCounter() const;
	int GetFrameWidth() const;
	int GetFrameHeight() const;
	uint64_t GetSequenceGapCount() const;

	// ---------------------------------------------------------------
	// Static methods: testable without network or instance state.
	// ---------------------------------------------------------------
	static bool ParsePacketHeader(const uint8_t *data, int dataLen,
								  C64UVideoPacketHeader &outHeader);
	static void DecodeLine(const uint8_t *nybbleData, int bytesPerLine,
						   const C64UNybbleLUT &lut, uint8_t *outRgba);

private:
	void WorkerLoop();
	void GenerateFixtureFrame();
	void ReceiveLoop();
	uint64_t GetCurrentTimeMillis() const;

	std::string host;
	int port;
	bool fixtureMode;
	bool useMulticast;

	std::atomic<bool> isRunning;
	std::thread workerThread;

	C64UNybbleLUT nybbleLUT;

	// Double-buffered frame storage: backBuffer is written by the worker,
	// frontBuffer is read by the renderer (under lock).
	mutable std::mutex frameMutex;
	uint8_t backBuffer[FRAME_WIDTH * FRAME_HEIGHT * 4];
	uint8_t frontBuffer[FRAME_WIDTH * FRAME_HEIGHT * 4];
	std::atomic<uint64_t> lastFrameTimeMillis;
	std::atomic<uint64_t> frameCounter;
	bool hasFreshFrame;

	// Sequence tracking
	uint16_t lastSeq;
	std::atomic<uint64_t> sequenceGapCount;

	// ENet UDP socket handle (ENET_SOCKET_NULL when not connected).
	ENetSocket udpSocket;
};

#endif
