#ifndef _C64UPORT64CLIENT_H_
#define _C64UPORT64CLIENT_H_

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct C64UPort64CommandResult
{
	bool success;
	std::string name;
	std::string detail;

	C64UPort64CommandResult()
	: success(false)
	{
	}
};

class C64UPort64Client
{
public:
	C64UPort64Client();
	~C64UPort64Client();

	void SetEndpoint(const std::string &host, int port);
	void SetPassword(const std::string &password);
	void Start();
	void Stop();

	// Schedule methods (enqueue commands for the worker thread)
	void ScheduleDeviceInfo();
	void ScheduleLoadAndRunPrg(const std::string &path);
	void ScheduleRunCrt(const std::string &path);
	void ScheduleMountDisk(const std::string &path, int driveId);
	void ScheduleRemoveDisk(int driveId);
	void ScheduleResetDisk(int driveId);
	void ScheduleDmaLoad(const uint8_t *data, int size);
	void ScheduleDmaLoadRun(const uint8_t *data, int size);
	void ScheduleDmaWrite(uint16_t address, const uint8_t *data, int size);
	void ScheduleKeyboardInject(const std::string &text);
	void ScheduleReset();
	void ScheduleVideoStart(uint16_t bufferSize, const std::string &destination);
	void ScheduleVideoStop();
	void ScheduleAudioStop();
	void ScheduleDebugStreamStart(const std::string &destination);
	void ScheduleDebugStreamStop();

	C64UPort64CommandResult GetLastResult() const;

	// Fixture mode: when true, skip real network and return success
	void SetFixtureMode(bool enabled);
	bool IsFixtureMode() const;

	// ---------------------------------------------------------------
	// Static serializers: pure functions producing wire-format packets.
	// Testable without any network or instance state.
	// ---------------------------------------------------------------

	// Standard 4-byte header commands (cmd_lo, cmd_hi=0xFF, len_lo, len_hi)
	static std::vector<uint8_t> SerializeAuthenticate(const std::string &password);
	static std::vector<uint8_t> SerializeIdentify();
	static std::vector<uint8_t> SerializeVideoStart(uint16_t bufferSize, const std::string &destination);
	static std::vector<uint8_t> SerializeVideoStop();
	static std::vector<uint8_t> SerializeAudioStop();
	static std::vector<uint8_t> SerializeDebugStreamStart(uint16_t bufferSize, const std::string &destination);
	static std::vector<uint8_t> SerializeDebugStreamStop();
	static std::vector<uint8_t> SerializeDmaLoad(const uint8_t *data, int size);
	static std::vector<uint8_t> SerializeDmaLoadRun(const uint8_t *data, int size);
	static std::vector<uint8_t> SerializeDmaWrite(uint16_t address, const uint8_t *data, int size);
	static std::vector<uint8_t> SerializeKeyboardInject(const std::string &text);
	static std::vector<uint8_t> SerializeReset();

	// 24-bit length commands (cmd_lo, cmd_hi=0xFF, len_lo, len_mid, len_hi) -- 5-byte header
	static std::vector<uint8_t> SerializeMountImage(const uint8_t *data, int size);
	static std::vector<uint8_t> SerializeRunImage(const uint8_t *data, int size);
	static std::vector<uint8_t> SerializeRunCrt(const uint8_t *data, int size);

	// ---------------------------------------------------------------
	// Static response parsers
	// ---------------------------------------------------------------
	static std::string ParseIdentifyResponse(const uint8_t *data, int len);
	static bool ParseAuthResponse(const uint8_t *data, int len);

private:
	struct Command
	{
		std::string name;
		std::string detail;
		std::vector<uint8_t> payload;  // pre-serialized wire packet
		bool expectsResponse;
	};

	void Enqueue(const std::string &name, const std::string &detail,
				 const std::vector<uint8_t> &packet, bool expectsResponse = false);
	void WorkerLoop();
	bool SendPacketAndReceive(const std::vector<uint8_t> &packet,
							 std::vector<uint8_t> &response);
	bool SendPacket(const std::vector<uint8_t> &packet);

	// Helpers for building wire-format packets
	static std::vector<uint8_t> BuildStandardPacket(uint8_t cmdLo, const uint8_t *payload, int payloadLen);
	static std::vector<uint8_t> Build24BitPacket(uint8_t cmdLo, const uint8_t *payload, int payloadLen);

	std::string host;
	std::string password;
	int port;
	std::atomic<bool> isRunning;
	std::atomic<bool> fixtureMode;
	std::thread workerThread;

	mutable std::mutex stateMutex;
	std::deque<Command> commands;
	C64UPort64CommandResult lastResult;
};

#endif
