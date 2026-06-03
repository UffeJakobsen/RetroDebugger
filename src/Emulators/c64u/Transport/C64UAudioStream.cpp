#include "C64UAudioStream.h"
#include "C64UMulticastHelper.h"
#include "../Audio/C64UAudioJitterBuffer.h"
#include "enet.h"
#include "DBG_Log.h"

#include <chrono>
#include <cmath>
#include <cstring>

C64UAudioStream::C64UAudioStream()
	: port(11001), fixtureMode(false), useMulticast(false), isRunning(false),
	  jitterBuffer(NULL), lastSeq(0), sequenceGapCount(0), packetsReceived(0),
	  udpSocket(ENET_SOCKET_NULL), fixturePhaseL(0.0f), fixturePhaseR(0.0f)
{
}

C64UAudioStream::~C64UAudioStream()
{
	Stop();
}

void C64UAudioStream::SetEndpoint(const std::string &host, int port)
{
	this->host = host;
	this->port = port;
}

void C64UAudioStream::Start(bool fixtureMode)
{
	Stop();
	this->fixtureMode = fixtureMode;
	lastSeq = 0;
	sequenceGapCount = 0;
	packetsReceived = 0;
	fixturePhaseL = 0.0f;
	fixturePhaseR = 0.0f;

	if (!fixtureMode)
	{
		ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
		if (sock == ENET_SOCKET_NULL)
		{
			LOGD("C64UAudioStream::Start: failed to create UDP socket");
			return;
		}

		enet_socket_set_option(sock, ENET_SOCKOPT_NONBLOCK, 1);
		enet_socket_set_option(sock, ENET_SOCKOPT_RCVBUF, 512 * 1024);

		ENetAddress bindAddr;
		bindAddr.host = ENET_HOST_ANY;
		bindAddr.port = (enet_uint16)port;
		if (enet_socket_bind(sock, &bindAddr) < 0)
		{
			LOGD("C64UAudioStream::Start: failed to bind to port %d", port);
			enet_socket_destroy(sock);
			return;
		}

		if (useMulticast)
		{
			C64UMulticastHelper::JoinMulticastGroup(sock, "239.0.1.65", "0.0.0.0");
		}

		udpSocket = sock;
	}

	isRunning = true;
	workerThread = std::thread(&C64UAudioStream::WorkerLoop, this);
}

void C64UAudioStream::Stop()
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

void C64UAudioStream::SetJitterBuffer(C64UAudioJitterBuffer *buffer)
{
	jitterBuffer = buffer;
}

void C64UAudioStream::SetUseMulticast(bool useMulticast)
{
	this->useMulticast = useMulticast;
}

uint64_t C64UAudioStream::GetSequenceGapCount() const
{
	return sequenceGapCount.load();
}

uint64_t C64UAudioStream::GetPacketsReceived() const
{
	return packetsReceived.load();
}

bool C64UAudioStream::IsRunning() const
{
	return isRunning.load();
}

// ---------------------------------------------------------------------------
// Static: Parse an audio packet
// Packet format: 2-byte seq (u16 LE) + i16 LE stereo interleaved samples
// Returns the number of stereo sample pairs decoded.
// ---------------------------------------------------------------------------

int C64UAudioStream::ParsePacket(const uint8_t *data, int dataLen,
								 float *outInterleavedLR, int maxStereoSamples)
{
	if (data == NULL || dataLen < HEADER_SIZE + 4)
		return 0;

	int payloadBytes = dataLen - HEADER_SIZE;
	// Each stereo pair is 4 bytes (2 x i16)
	int availablePairs = payloadBytes / 4;
	int pairsToRead = (availablePairs < maxStereoSamples) ? availablePairs : maxStereoSamples;

	for (int i = 0; i < pairsToRead; i++)
	{
		int offset = HEADER_SIZE + i * 4;
		int16_t sampleL = (int16_t)(data[offset] | (data[offset + 1] << 8));
		int16_t sampleR = (int16_t)(data[offset + 2] | (data[offset + 3] << 8));
		outInterleavedLR[i * 2]     = (float)sampleL / 32768.0f;
		outInterleavedLR[i * 2 + 1] = (float)sampleR / 32768.0f;
	}

	return pairsToRead;
}

// ---------------------------------------------------------------------------
// Worker thread entry point
// ---------------------------------------------------------------------------

void C64UAudioStream::WorkerLoop()
{
	if (fixtureMode)
	{
		while (isRunning)
		{
			GenerateFixtureAudio();
			std::this_thread::sleep_for(std::chrono::milliseconds(4));
		}
	}
	else
	{
		ReceiveLoop();
	}
}

// ---------------------------------------------------------------------------
// Real UDP receive loop
// ---------------------------------------------------------------------------

void C64UAudioStream::ReceiveLoop()
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

		if (received < HEADER_SIZE + 4)
			continue;

		// Extract sequence number
		uint16_t seq = (uint16_t)(recvBuf[0] | (recvBuf[1] << 8));

		// Sequence gap detection
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

		// Parse and write to jitter buffer
		float interleavedLR[1024];  // max 512 stereo pairs
		int pairsDecoded = ParsePacket(recvBuf, received, interleavedLR, 512);

		if (jitterBuffer != NULL && pairsDecoded > 0)
		{
			jitterBuffer->Write(interleavedLR, pairsDecoded);
		}

		packetsReceived.fetch_add(1);
	}
}

// ---------------------------------------------------------------------------
// Fixture mode: generate stereo sine wave (440Hz L, 880Hz R) at 47983 Hz
// ~192 samples every ~4ms
// ---------------------------------------------------------------------------

void C64UAudioStream::GenerateFixtureAudio()
{
	if (jitterBuffer == NULL)
		return;

	static const float SOURCE_RATE = 47983.0f;
	static const int SAMPLES_PER_TICK = 192;
	static const float FREQ_L = 440.0f;
	static const float FREQ_R = 880.0f;
	static const float TWO_PI = 6.283185307f;
	static const float AMPLITUDE = 0.5f;

	float interleavedLR[SAMPLES_PER_TICK * 2];

	float phaseIncL = TWO_PI * FREQ_L / SOURCE_RATE;
	float phaseIncR = TWO_PI * FREQ_R / SOURCE_RATE;

	for (int i = 0; i < SAMPLES_PER_TICK; i++)
	{
		interleavedLR[i * 2]     = AMPLITUDE * sinf(fixturePhaseL);
		interleavedLR[i * 2 + 1] = AMPLITUDE * sinf(fixturePhaseR);
		fixturePhaseL += phaseIncL;
		fixturePhaseR += phaseIncR;
	}

	// Wrap phases to prevent float precision degradation over time
	if (fixturePhaseL > TWO_PI)
		fixturePhaseL -= TWO_PI;
	if (fixturePhaseR > TWO_PI)
		fixturePhaseR -= TWO_PI;

	jitterBuffer->Write(interleavedLR, SAMPLES_PER_TICK);
	packetsReceived.fetch_add(1);
}

uint64_t C64UAudioStream::GetCurrentTimeMillis() const
{
	return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}
