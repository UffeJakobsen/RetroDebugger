#ifndef _C64URESTCLIENT_H_
#define _C64URESTCLIENT_H_

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct C64URestCommandResult
{
	bool success;
	int statusCode;
	std::string name;
	std::string detail;
	std::vector<uint8_t> body;

	C64URestCommandResult()
	: success(false), statusCode(0)
	{
	}
};

class C64UMemoryCache;

class C64URestClient
{
public:
	C64URestClient();
	~C64URestClient();

	void SetEndpoint(const std::string &host, int port, const std::string &password);
	void SetMemoryCache(C64UMemoryCache *cache);
	void Start();
	void Stop();

	// Existing schedule methods
	void ScheduleDeviceInfo();
	void ScheduleAuthenticate();
	void ScheduleReset(bool hardReset);
	void ScheduleReboot();
	void ScheduleLoadPrg(const std::string &path, bool autoRun);
	void ScheduleRunCrt(const std::string &path);
	void ScheduleMountDisk(const std::string &path, int driveId);
	void ScheduleRemoveDisk(int driveId);
	void ScheduleResetDisk(int driveId);

	// New schedule methods
	void ScheduleReadMemory(int address, int length);
	void ScheduleWriteMemory(int address, const uint8_t *data, int length);
	void ScheduleWriteMemoryHex(int address, const std::string &hexData);
	void SchedulePause();
	void ScheduleResume();
	void ScheduleStreamStart(const std::string &streamName, const std::string &localIp, int localPort);
	void ScheduleStreamStop(const std::string &streamName);
	void ScheduleGetDriveInfo();

	C64URestCommandResult GetLastResult() const;

	// Fixture mode: when true, skip real HTTP and return success
	void SetFixtureMode(bool enabled);
	bool IsFixtureMode() const;

	// Static URL formatters (testable without instance)
	static std::string FormatReadMemoryUrl(int address, int length);
	static std::string FormatWriteMemoryUrl(int address);
	static std::string FormatWriteMemoryHexUrl(int address, const std::string &hexData);
	static std::string FormatStreamStartUrl(const std::string &streamName, const std::string &localIp, int localPort);
	static std::string FormatStreamStopUrl(const std::string &streamName);
	static std::string FormatRunPrgUrl(const std::string &filePath);
	static std::string FormatRunCrtUrl(const std::string &filePath);
	static std::string FormatMountDiskUrl(int driveId, const std::string &imagePath, const std::string &mode);

	// Response parsers
	static bool ParseReadMemoryResponse(const std::string &responseBody, std::vector<uint8_t> &outData);
	static bool ParseDeviceInfoResponse(const std::string &responseBody, std::string &outBoardType, std::string &outVersion);

	// Password header helpers
	static std::string GetPasswordHeaderKey();
	static std::string GetPasswordHeaderValue(const std::string &password);

private:
	struct Command
	{
		std::string name;
		std::string url;
		std::string method;   // "GET", "PUT", "POST"
		std::vector<uint8_t> data;  // binary payload for POST (memory write)
	};

	void Enqueue(const std::string &name, const std::string &url, const std::string &method);
	void EnqueueWithData(const std::string &name, const std::string &url, const std::string &method,
						 const uint8_t *data, int dataLength);
	void WorkerLoop();
	void ExecuteHttpCommand(const Command &command);
	void StoreResult(bool success, int statusCode, const std::string &name,
					 const std::string &detail, const std::vector<uint8_t> &body = {});

	std::string host;
	std::string password;
	int port;
	C64UMemoryCache *memoryCache;

	std::atomic<bool> isRunning;
	std::atomic<bool> fixtureMode;
	std::thread workerThread;

	mutable std::mutex stateMutex;
	std::deque<Command> commands;
	C64URestCommandResult lastResult;
};

#endif
