#include "C64UPort64Client.h"
#include "enet.h"
#include "DBG_Log.h"

#include <algorithm>
#include <chrono>
#include <cstring>

// Protocol constants
static const uint8_t CMD_HI             = 0xFF;
static const uint8_t CMD_AUTHENTICATE   = 0x1F;
static const uint8_t CMD_IDENTIFY       = 0x0E;
static const uint8_t CMD_VIDEO_START    = 0x20;
static const uint8_t CMD_DEBUG_START    = 0x22;
static const uint8_t CMD_VIDEO_STOP     = 0x30;
static const uint8_t CMD_AUDIO_STOP     = 0x31;
static const uint8_t CMD_DEBUG_STOP     = 0x32;
static const uint8_t CMD_DMA_LOAD       = 0x01;
static const uint8_t CMD_DMA_LOAD_RUN   = 0x02;
static const uint8_t CMD_KEYBOARD       = 0x03;
static const uint8_t CMD_RESET          = 0x04;
static const uint8_t CMD_DMA_WRITE      = 0x06;
static const uint8_t CMD_MOUNT_IMAGE    = 0x0A;
static const uint8_t CMD_RUN_IMAGE      = 0x0B;
static const uint8_t CMD_RUN_CRT        = 0x0D;

static const int KEYBOARD_MAX_LEN       = 10;
static const int CONNECTION_TIMEOUT_MS  = 3000;
static const int RECV_BUFFER_SIZE       = 256;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

C64UPort64Client::C64UPort64Client()
	: port(64), isRunning(false), fixtureMode(false)
{
}

C64UPort64Client::~C64UPort64Client()
{
	Stop();
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void C64UPort64Client::SetEndpoint(const std::string &host, int port)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	this->host = host;
	this->port = port;
}

void C64UPort64Client::SetPassword(const std::string &password)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	this->password = password;
}

void C64UPort64Client::SetFixtureMode(bool enabled)
{
	fixtureMode.store(enabled);
}

bool C64UPort64Client::IsFixtureMode() const
{
	return fixtureMode.load();
}

// ---------------------------------------------------------------------------
// Worker lifecycle
// ---------------------------------------------------------------------------

void C64UPort64Client::Start()
{
	if (isRunning.exchange(true))
		return;

	workerThread = std::thread(&C64UPort64Client::WorkerLoop, this);
}

void C64UPort64Client::Stop()
{
	if (!isRunning.exchange(false))
		return;

	if (workerThread.joinable())
		workerThread.join();

	std::lock_guard<std::mutex> lock(stateMutex);
	commands.clear();
}

// ---------------------------------------------------------------------------
// Schedule methods (public API -- enqueue pre-serialized packets)
// ---------------------------------------------------------------------------

void C64UPort64Client::ScheduleDeviceInfo()
{
	Enqueue("identify", "", SerializeIdentify(), true);
}

void C64UPort64Client::ScheduleLoadAndRunPrg(const std::string &path)
{
	// Note: the path variant reads from file then uses DmaLoadRun.
	// For now, enqueue a placeholder. Real file I/O is done by the caller.
	Enqueue("load-run-prg", path, {});
}

void C64UPort64Client::ScheduleRunCrt(const std::string &path)
{
	Enqueue("run-crt", path, {});
}

void C64UPort64Client::ScheduleMountDisk(const std::string &path, int driveId)
{
	Enqueue("mount-disk", std::to_string(driveId) + ":" + path, {});
}

void C64UPort64Client::ScheduleRemoveDisk(int driveId)
{
	Enqueue("remove-disk", std::to_string(driveId), {});
}

void C64UPort64Client::ScheduleResetDisk(int driveId)
{
	Enqueue("reset-disk", std::to_string(driveId), {});
}

void C64UPort64Client::ScheduleDmaLoad(const uint8_t *data, int size)
{
	Enqueue("dma-load", "", SerializeDmaLoad(data, size));
}

void C64UPort64Client::ScheduleDmaLoadRun(const uint8_t *data, int size)
{
	Enqueue("dma-load-run", "", SerializeDmaLoadRun(data, size));
}

void C64UPort64Client::ScheduleDmaWrite(uint16_t address, const uint8_t *data, int size)
{
	Enqueue("dma-write", "", SerializeDmaWrite(address, data, size));
}

void C64UPort64Client::ScheduleKeyboardInject(const std::string &text)
{
	Enqueue("keyboard-inject", text, SerializeKeyboardInject(text));
}

void C64UPort64Client::ScheduleReset()
{
	Enqueue("reset", "", SerializeReset());
}

void C64UPort64Client::ScheduleVideoStart(uint16_t bufferSize, const std::string &destination)
{
	Enqueue("video-start", destination, SerializeVideoStart(bufferSize, destination));
}

void C64UPort64Client::ScheduleVideoStop()
{
	Enqueue("video-stop", "", SerializeVideoStop());
}

void C64UPort64Client::ScheduleAudioStop()
{
	Enqueue("audio-stop", "", SerializeAudioStop());
}

void C64UPort64Client::ScheduleDebugStreamStart(const std::string &destination)
{
	Enqueue("debug-stream-start", destination, SerializeDebugStreamStart(0, destination));
}

void C64UPort64Client::ScheduleDebugStreamStop()
{
	Enqueue("debug-stream-stop", "", SerializeDebugStreamStop());
}

C64UPort64CommandResult C64UPort64Client::GetLastResult() const
{
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastResult;
}

// ---------------------------------------------------------------------------
// Static serializers -- pure functions, no side effects
// ---------------------------------------------------------------------------

std::vector<uint8_t> C64UPort64Client::BuildStandardPacket(uint8_t cmdLo,
														   const uint8_t *payload,
														   int payloadLen)
{
	// Standard 4-byte header: [cmd_lo, 0xFF, len_lo, len_hi] + payload
	uint16_t len = (uint16_t)payloadLen;
	std::vector<uint8_t> pkt;
	pkt.reserve(4 + payloadLen);
	pkt.push_back(cmdLo);
	pkt.push_back(CMD_HI);
	pkt.push_back((uint8_t)(len & 0xFF));
	pkt.push_back((uint8_t)((len >> 8) & 0xFF));
	if (payload && payloadLen > 0)
	{
		pkt.insert(pkt.end(), payload, payload + payloadLen);
	}
	return pkt;
}

std::vector<uint8_t> C64UPort64Client::Build24BitPacket(uint8_t cmdLo,
														const uint8_t *payload,
														int payloadLen)
{
	// 24-bit length header: [cmd_lo, 0xFF, len_lo, len_mid, len_hi] + payload
	uint32_t len = (uint32_t)payloadLen;
	std::vector<uint8_t> pkt;
	pkt.reserve(5 + payloadLen);
	pkt.push_back(cmdLo);
	pkt.push_back(CMD_HI);
	pkt.push_back((uint8_t)(len & 0xFF));
	pkt.push_back((uint8_t)((len >> 8) & 0xFF));
	pkt.push_back((uint8_t)((len >> 16) & 0xFF));
	if (payload && payloadLen > 0)
	{
		pkt.insert(pkt.end(), payload, payload + payloadLen);
	}
	return pkt;
}

std::vector<uint8_t> C64UPort64Client::SerializeAuthenticate(const std::string &password)
{
	return BuildStandardPacket(CMD_AUTHENTICATE,
							  (const uint8_t *)password.data(),
							  (int)password.size());
}

std::vector<uint8_t> C64UPort64Client::SerializeIdentify()
{
	return BuildStandardPacket(CMD_IDENTIFY, nullptr, 0);
}

std::vector<uint8_t> C64UPort64Client::SerializeVideoStart(uint16_t bufferSize,
															const std::string &destination)
{
	// Payload: [bufsz_lo, bufsz_hi, "IP:PORT"]
	std::vector<uint8_t> payload;
	payload.push_back((uint8_t)(bufferSize & 0xFF));
	payload.push_back((uint8_t)((bufferSize >> 8) & 0xFF));
	payload.insert(payload.end(), destination.begin(), destination.end());
	return BuildStandardPacket(CMD_VIDEO_START, payload.data(), (int)payload.size());
}

std::vector<uint8_t> C64UPort64Client::SerializeVideoStop()
{
	return BuildStandardPacket(CMD_VIDEO_STOP, nullptr, 0);
}

std::vector<uint8_t> C64UPort64Client::SerializeAudioStop()
{
	return BuildStandardPacket(CMD_AUDIO_STOP, nullptr, 0);
}

std::vector<uint8_t> C64UPort64Client::SerializeDmaLoad(const uint8_t *data, int size)
{
	return BuildStandardPacket(CMD_DMA_LOAD, data, size);
}

std::vector<uint8_t> C64UPort64Client::SerializeDmaLoadRun(const uint8_t *data, int size)
{
	return BuildStandardPacket(CMD_DMA_LOAD_RUN, data, size);
}

std::vector<uint8_t> C64UPort64Client::SerializeDmaWrite(uint16_t address,
														  const uint8_t *data, int size)
{
	// Payload: [addr_lo, addr_hi, data...]
	std::vector<uint8_t> payload;
	payload.reserve(2 + size);
	payload.push_back((uint8_t)(address & 0xFF));
	payload.push_back((uint8_t)((address >> 8) & 0xFF));
	if (data && size > 0)
	{
		payload.insert(payload.end(), data, data + size);
	}
	return BuildStandardPacket(CMD_DMA_WRITE, payload.data(), (int)payload.size());
}

std::vector<uint8_t> C64UPort64Client::SerializeKeyboardInject(const std::string &text)
{
	int len = (int)text.size();
	if (len > KEYBOARD_MAX_LEN)
	{
		len = KEYBOARD_MAX_LEN;
	}
	return BuildStandardPacket(CMD_KEYBOARD, (const uint8_t *)text.data(), len);
}

std::vector<uint8_t> C64UPort64Client::SerializeReset()
{
	return BuildStandardPacket(CMD_RESET, nullptr, 0);
}

std::vector<uint8_t> C64UPort64Client::SerializeDebugStreamStart(uint16_t bufferSize,
																 const std::string &destination)
{
	// Same payload format as video start: [bufsz_lo, bufsz_hi, "IP:PORT"]
	std::vector<uint8_t> payload;
	payload.push_back((uint8_t)(bufferSize & 0xFF));
	payload.push_back((uint8_t)((bufferSize >> 8) & 0xFF));
	payload.insert(payload.end(), destination.begin(), destination.end());
	return BuildStandardPacket(CMD_DEBUG_START, payload.data(), (int)payload.size());
}

std::vector<uint8_t> C64UPort64Client::SerializeDebugStreamStop()
{
	return BuildStandardPacket(CMD_DEBUG_STOP, nullptr, 0);
}

std::vector<uint8_t> C64UPort64Client::SerializeMountImage(const uint8_t *data, int size)
{
	return Build24BitPacket(CMD_MOUNT_IMAGE, data, size);
}

std::vector<uint8_t> C64UPort64Client::SerializeRunImage(const uint8_t *data, int size)
{
	return Build24BitPacket(CMD_RUN_IMAGE, data, size);
}

std::vector<uint8_t> C64UPort64Client::SerializeRunCrt(const uint8_t *data, int size)
{
	return Build24BitPacket(CMD_RUN_CRT, data, size);
}

// ---------------------------------------------------------------------------
// Static response parsers
// ---------------------------------------------------------------------------

std::string C64UPort64Client::ParseIdentifyResponse(const uint8_t *data, int len)
{
	// Pascal-string format: [len_byte, chars...]
	if (!data || len < 1)
	{
		return "";
	}
	int strLen = data[0];
	if (strLen > len - 1)
	{
		strLen = len - 1;
	}
	return std::string((const char *)&data[1], strLen);
}

bool C64UPort64Client::ParseAuthResponse(const uint8_t *data, int len)
{
	if (!data || len < 1)
	{
		return false;
	}
	return data[0] == 0x01;
}

// ---------------------------------------------------------------------------
// Internal queue / worker
// ---------------------------------------------------------------------------

void C64UPort64Client::Enqueue(const std::string &name, const std::string &detail,
							   const std::vector<uint8_t> &packet, bool expectsResponse)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	commands.push_back({name, detail, packet, expectsResponse});
}

bool C64UPort64Client::SendPacketAndReceive(const std::vector<uint8_t> &packet,
											std::vector<uint8_t> &response)
{
	std::string currentHost;
	int currentPort;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		currentHost = host;
		currentPort = port;
	}

	ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
	if (sock == ENET_SOCKET_NULL)
	{
		LOGD("C64UPort64Client: failed to create socket");
		return false;
	}

	enet_socket_set_option(sock, ENET_SOCKOPT_SNDTIMEO, CONNECTION_TIMEOUT_MS);
	enet_socket_set_option(sock, ENET_SOCKOPT_RCVTIMEO, CONNECTION_TIMEOUT_MS);

	ENetAddress addr;
	if (enet_address_set_host(&addr, currentHost.c_str()) != 0)
	{
		LOGD("C64UPort64Client: failed to resolve host %s", currentHost.c_str());
		enet_socket_destroy(sock);
		return false;
	}
	addr.port = (uint16_t)currentPort;

	if (enet_socket_connect(sock, &addr) != 0)
	{
		LOGD("C64UPort64Client: failed to connect to %s:%d", currentHost.c_str(), currentPort);
		enet_socket_destroy(sock);
		return false;
	}

	// If password is set, authenticate first
	std::string currentPassword;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		currentPassword = password;
	}

	if (!currentPassword.empty())
	{
		std::vector<uint8_t> authPacket = SerializeAuthenticate(currentPassword);
		ENetBuffer sendBuf;
		sendBuf.data = (void *)authPacket.data();
		sendBuf.dataLength = authPacket.size();
		if (enet_socket_send(sock, NULL, &sendBuf, 1) < 0)
		{
			LOGD("C64UPort64Client: failed to send auth packet");
			enet_socket_destroy(sock);
			return false;
		}

		uint8_t authResp[RECV_BUFFER_SIZE];
		ENetBuffer authRecvBuf;
		authRecvBuf.data = authResp;
		authRecvBuf.dataLength = sizeof(authResp);
		int authReceived = enet_socket_receive(sock, NULL, &authRecvBuf, 1);
		if (authReceived < 1 || !ParseAuthResponse(authResp, authReceived))
		{
			LOGD("C64UPort64Client: authentication failed");
			enet_socket_destroy(sock);
			return false;
		}
	}

	// Send the actual command
	ENetBuffer sendBuf;
	sendBuf.data = (void *)packet.data();
	sendBuf.dataLength = packet.size();
	if (enet_socket_send(sock, NULL, &sendBuf, 1) < 0)
	{
		LOGD("C64UPort64Client: failed to send command packet");
		enet_socket_destroy(sock);
		return false;
	}

	// Receive response
	uint8_t recvBuf[RECV_BUFFER_SIZE];
	ENetBuffer recvEnetBuf;
	recvEnetBuf.data = recvBuf;
	recvEnetBuf.dataLength = sizeof(recvBuf);
	int received = enet_socket_receive(sock, NULL, &recvEnetBuf, 1);
	if (received > 0)
	{
		response.assign(recvBuf, recvBuf + received);
	}

	enet_socket_destroy(sock);
	return true;
}

bool C64UPort64Client::SendPacket(const std::vector<uint8_t> &packet)
{
	std::string currentHost;
	int currentPort;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		currentHost = host;
		currentPort = port;
	}

	ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
	if (sock == ENET_SOCKET_NULL)
	{
		LOGD("C64UPort64Client: failed to create socket");
		return false;
	}

	enet_socket_set_option(sock, ENET_SOCKOPT_SNDTIMEO, CONNECTION_TIMEOUT_MS);
	enet_socket_set_option(sock, ENET_SOCKOPT_RCVTIMEO, CONNECTION_TIMEOUT_MS);

	ENetAddress addr;
	if (enet_address_set_host(&addr, currentHost.c_str()) != 0)
	{
		LOGD("C64UPort64Client: failed to resolve host %s", currentHost.c_str());
		enet_socket_destroy(sock);
		return false;
	}
	addr.port = (uint16_t)currentPort;

	if (enet_socket_connect(sock, &addr) != 0)
	{
		LOGD("C64UPort64Client: failed to connect to %s:%d", currentHost.c_str(), currentPort);
		enet_socket_destroy(sock);
		return false;
	}

	// If password is set, authenticate first
	std::string currentPassword;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		currentPassword = password;
	}

	if (!currentPassword.empty())
	{
		std::vector<uint8_t> authPacket = SerializeAuthenticate(currentPassword);
		ENetBuffer authSendBuf;
		authSendBuf.data = (void *)authPacket.data();
		authSendBuf.dataLength = authPacket.size();
		if (enet_socket_send(sock, NULL, &authSendBuf, 1) < 0)
		{
			LOGD("C64UPort64Client: failed to send auth packet");
			enet_socket_destroy(sock);
			return false;
		}

		uint8_t authResp[RECV_BUFFER_SIZE];
		ENetBuffer authRecvBuf;
		authRecvBuf.data = authResp;
		authRecvBuf.dataLength = sizeof(authResp);
		int authReceived = enet_socket_receive(sock, NULL, &authRecvBuf, 1);
		if (authReceived < 1 || !ParseAuthResponse(authResp, authReceived))
		{
			LOGD("C64UPort64Client: authentication failed");
			enet_socket_destroy(sock);
			return false;
		}
	}

	// Send the command
	ENetBuffer sendBuf;
	sendBuf.data = (void *)packet.data();
	sendBuf.dataLength = packet.size();
	int sent = enet_socket_send(sock, NULL, &sendBuf, 1);

	enet_socket_destroy(sock);
	return sent >= 0;
}

void C64UPort64Client::WorkerLoop()
{
	while (isRunning)
	{
		Command command;
		bool hasCommand = false;

		{
			std::lock_guard<std::mutex> lock(stateMutex);
			if (!commands.empty())
			{
				command = commands.front();
				commands.pop_front();
				hasCommand = true;
			}
		}

		if (!hasCommand)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		bool success = false;
		std::string detail;

		if (fixtureMode.load())
		{
			// Fixture mode: skip real network, log and return success
			LOGD("C64UPort64Client [fixture]: %s %s (%d bytes)",
				 command.name.c_str(), command.detail.c_str(), (int)command.payload.size());
			success = true;
			detail = "fixture: " + command.name;
		}
		else if (command.payload.empty())
		{
			// No serialized packet -- legacy path-based commands not yet implemented
			detail = "TCP64 not yet implemented for path-based: " + command.detail;
			success = false;
		}
		else if (command.expectsResponse)
		{
			std::vector<uint8_t> response;
			success = SendPacketAndReceive(command.payload, response);
			if (success && command.name == "identify" && !response.empty())
			{
				detail = ParseIdentifyResponse(response.data(), (int)response.size());
			}
			else
			{
				detail = success ? "ok" : "send/receive failed";
			}
		}
		else
		{
			success = SendPacket(command.payload);
			detail = success ? "ok" : "send failed";
		}

		{
			std::lock_guard<std::mutex> lock(stateMutex);
			lastResult.success = success;
			lastResult.name = command.name;
			lastResult.detail = detail;
		}
	}
}
