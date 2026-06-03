#include "C64UVideoStream.h"
#include "C64UMulticastHelper.h"
#include "enet.h"
#include "CImageData.h"
#include "DBG_Log.h"

#include <chrono>
#include <cstring>

C64UVideoStream::C64UVideoStream()
	: port(11000), fixtureMode(false), useMulticast(false), isRunning(false),
	  lastFrameTimeMillis(0), frameCounter(0), hasFreshFrame(false),
	  lastSeq(0), sequenceGapCount(0), udpSocket(ENET_SOCKET_NULL)
{
	memset(backBuffer, 0, sizeof(backBuffer));
	memset(frontBuffer, 0, sizeof(frontBuffer));
}

C64UVideoStream::~C64UVideoStream()
{
	Stop();
}

void C64UVideoStream::SetEndpoint(const std::string &host, int port)
{
	std::lock_guard<std::mutex> lock(frameMutex);
	this->host = host;
	this->port = port;
}

void C64UVideoStream::Start(bool generateFixtureFrames)
{
	Stop();
	fixtureMode = generateFixtureFrames;
	frameCounter = 0;
	lastFrameTimeMillis = 0;
	hasFreshFrame = false;
	lastSeq = 0;
	sequenceGapCount = 0;

	nybbleLUT.Build();

	if (!fixtureMode)
	{
		ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
		if (sock == ENET_SOCKET_NULL)
		{
			LOGD("C64UVideoStream::Start: failed to create UDP socket");
			return;
		}

		enet_socket_set_option(sock, ENET_SOCKOPT_NONBLOCK, 1);
		enet_socket_set_option(sock, ENET_SOCKOPT_RCVBUF, 1024 * 1024);

		ENetAddress bindAddr;
		bindAddr.host = ENET_HOST_ANY;
		bindAddr.port = (enet_uint16)port;
		if (enet_socket_bind(sock, &bindAddr) < 0)
		{
			LOGD("C64UVideoStream::Start: failed to bind to port %d", port);
			enet_socket_destroy(sock);
			return;
		}

		if (useMulticast)
		{
			C64UMulticastHelper::JoinMulticastGroup(sock, "239.0.1.64", "0.0.0.0");
		}

		udpSocket = sock;
	}

	isRunning = true;
	workerThread = std::thread(&C64UVideoStream::WorkerLoop, this);
}

void C64UVideoStream::Stop()
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

void C64UVideoStream::SetUseMulticast(bool useMulticast)
{
	this->useMulticast = useMulticast;
}

bool C64UVideoStream::CopyLatestFrameToImage(CImageData *imageData)
{
	if (imageData == NULL || imageData->resultData == NULL)
		return false;

	std::lock_guard<std::mutex> lock(frameMutex);
	if (!hasFreshFrame)
		return false;

	// Supersample factor: base texture is 512, so factor = imageData->width / 512
	int factor = imageData->width / 512;
	if (factor < 1) factor = 1;

	uint8_t *dest = (uint8_t *)imageData->resultData;
	const int destStride = imageData->width * 4;

	if (factor == 1)
	{
		// Fast path: direct 1:1 copy
		const int srcStride = FRAME_WIDTH * 4;
		const int copyWidth = (srcStride < destStride) ? srcStride : destStride;
		for (int y = 0; y < FRAME_HEIGHT && y < imageData->height; y++)
		{
			memcpy(dest + y * destStride, frontBuffer + y * srcStride, copyWidth);
		}
	}
	else
	{
		// Nearest-neighbor upscale: each source pixel becomes factor x factor
		const int srcStride = FRAME_WIDTH * 4;
		int destH = imageData->height;

		for (int sy = 0; sy < FRAME_HEIGHT; sy++)
		{
			const uint8_t *srcRow = frontBuffer + sy * srcStride;

			// Build one upscaled row
			int dy0 = sy * factor;
			if (dy0 >= destH) break;

			uint8_t *destRow = dest + dy0 * destStride;
			for (int sx = 0; sx < FRAME_WIDTH; sx++)
			{
				const uint8_t *srcPixel = srcRow + sx * 4;
				uint8_t *dp = destRow + sx * factor * 4;
				for (int fx = 0; fx < factor; fx++)
				{
					dp[0] = srcPixel[0];
					dp[1] = srcPixel[1];
					dp[2] = srcPixel[2];
					dp[3] = srcPixel[3];
					dp += 4;
				}
			}

			// Duplicate this row (factor - 1) more times
			for (int fy = 1; fy < factor; fy++)
			{
				int dy = dy0 + fy;
				if (dy >= destH) break;
				memcpy(dest + dy * destStride, destRow, FRAME_WIDTH * factor * 4);
			}
		}
	}

	hasFreshFrame = false;
	return true;
}

void C64UVideoStream::RebuildLUT(const uint8_t palette[16][3])
{
	std::lock_guard<std::mutex> lock(frameMutex);
	nybbleLUT.BuildFromPalette(palette);
}

bool C64UVideoStream::HasTimedOut(uint64_t timeoutMillis) const
{
	uint64_t lastFrameTime = lastFrameTimeMillis.load();
	if (lastFrameTime == 0)
		return true;

	return (GetCurrentTimeMillis() - lastFrameTime) > timeoutMillis;
}

uint64_t C64UVideoStream::GetMillisecondsSinceLastFrame() const
{
	uint64_t lastFrameTime = lastFrameTimeMillis.load();
	if (lastFrameTime == 0)
		return 0;

	return GetCurrentTimeMillis() - lastFrameTime;
}

uint64_t C64UVideoStream::GetFrameCounter() const
{
	return frameCounter.load();
}

int C64UVideoStream::GetFrameWidth() const
{
	return FRAME_WIDTH;
}

int C64UVideoStream::GetFrameHeight() const
{
	return FRAME_HEIGHT;
}

uint64_t C64UVideoStream::GetSequenceGapCount() const
{
	return sequenceGapCount.load();
}

// ---------------------------------------------------------------------------
// Static: Parse the 12-byte packet header
// ---------------------------------------------------------------------------

bool C64UVideoStream::ParsePacketHeader(const uint8_t *data, int dataLen,
										C64UVideoPacketHeader &outHeader)
{
	if (data == NULL || dataLen < HEADER_SIZE)
		return false;

	outHeader.seq           = (uint16_t)(data[0] | (data[1] << 8));
	outHeader.frame         = (uint16_t)(data[2] | (data[3] << 8));
	uint16_t lineRaw        = (uint16_t)(data[4] | (data[5] << 8));
	outHeader.lineNum       = lineRaw & 0x7FFF;
	outHeader.frameDone     = (lineRaw & 0x8000) != 0;
	outHeader.pixelsInLine  = (uint16_t)(data[6] | (data[7] << 8));
	outHeader.linesInPacket = data[8];
	outHeader.bpp           = data[9];
	outHeader.encoding      = (uint16_t)(data[10] | (data[11] << 8));

	return true;
}

// ---------------------------------------------------------------------------
// Static: Decode one line of packed nybble data into RGBA
// ---------------------------------------------------------------------------

void C64UVideoStream::DecodeLine(const uint8_t *nybbleData, int bytesPerLine,
								 const C64UNybbleLUT &lut, uint8_t *outRgba)
{
	for (int i = 0; i < bytesPerLine; i++)
	{
		const uint8_t *entry = lut.table[nybbleData[i]];
		uint8_t *dst = outRgba + i * 8;
		dst[0] = entry[0];
		dst[1] = entry[1];
		dst[2] = entry[2];
		dst[3] = entry[3];
		dst[4] = entry[4];
		dst[5] = entry[5];
		dst[6] = entry[6];
		dst[7] = entry[7];
	}
}

// ---------------------------------------------------------------------------
// Worker thread entry point
// ---------------------------------------------------------------------------

void C64UVideoStream::WorkerLoop()
{
	while (isRunning)
	{
		if (fixtureMode)
		{
			GenerateFixtureFrame();
			std::this_thread::sleep_for(std::chrono::milliseconds(33));
		}
		else
		{
			ReceiveLoop();
		}
	}
}

// ---------------------------------------------------------------------------
// Real UDP receive loop
// ---------------------------------------------------------------------------

void C64UVideoStream::ReceiveLoop()
{
	// Maximum UDP packet: header + 2 lines of pixel data (generous)
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
			// 0 = would-block (non-blocking socket), -1 = error
			// Sleep briefly to avoid busy-spinning when no data
			std::this_thread::sleep_for(std::chrono::microseconds(200));
			continue;
		}

		C64UVideoPacketHeader header;
		if (!ParsePacketHeader(recvBuf, received, header))
			continue;

		// Sequence gap detection
		if (lastSeq != 0)
		{
			uint16_t expected = lastSeq + 1;
			if (header.seq != expected && header.seq > lastSeq)
			{
				uint64_t gap = (uint64_t)(header.seq - lastSeq - 1);
				sequenceGapCount.fetch_add(gap);
			}
		}
		lastSeq = header.seq;

		// Decode lines into backBuffer
		int payloadOffset = HEADER_SIZE;
		for (int lineIdx = 0; lineIdx < header.linesInPacket; lineIdx++)
		{
			int lineNum = header.lineNum + lineIdx;
			if (lineNum < 0 || lineNum >= FRAME_HEIGHT)
			{
				payloadOffset += BYTES_PER_LINE;
				continue;
			}

			int bytesAvailable = received - payloadOffset;
			if (bytesAvailable < BYTES_PER_LINE)
				break;

			uint8_t *destLine = backBuffer + lineNum * FRAME_WIDTH * 4;
			DecodeLine(recvBuf + payloadOffset, BYTES_PER_LINE, nybbleLUT, destLine);
			payloadOffset += BYTES_PER_LINE;
		}

		// Frame completion
		if (header.frameDone)
		{
			std::lock_guard<std::mutex> lock(frameMutex);
			memcpy(frontBuffer, backBuffer, sizeof(frontBuffer));
			hasFreshFrame = true;
			frameCounter.fetch_add(1);
			lastFrameTimeMillis = GetCurrentTimeMillis();
		}
	}
}

// ---------------------------------------------------------------------------
// Fixture mode: generate synthetic frames
// ---------------------------------------------------------------------------

void C64UVideoStream::GenerateFixtureFrame()
{
	uint64_t frameNumber = frameCounter.fetch_add(1) + 1;

	for (int y = 0; y < FRAME_HEIGHT; y++)
	{
		for (int x = 0; x < FRAME_WIDTH; x++)
		{
			size_t offset = ((size_t)y * (size_t)FRAME_WIDTH + (size_t)x) * 4;
			uint8_t r = (uint8_t)((x + frameNumber) & 0xff);
			uint8_t g = (uint8_t)((y * 2 + frameNumber) & 0xff);
			uint8_t b = (uint8_t)(((x / 8) ^ (y / 8) ^ (int)frameNumber) & 0xff);
			backBuffer[offset + 0] = r;
			backBuffer[offset + 1] = g;
			backBuffer[offset + 2] = b;
			backBuffer[offset + 3] = 0xff;
		}
	}

	{
		std::lock_guard<std::mutex> lock(frameMutex);
		memcpy(frontBuffer, backBuffer, sizeof(frontBuffer));
		hasFreshFrame = true;
	}
	lastFrameTimeMillis = GetCurrentTimeMillis();
}

uint64_t C64UVideoStream::GetCurrentTimeMillis() const
{
	return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}
