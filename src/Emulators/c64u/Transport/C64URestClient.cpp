#include "C64URestClient.h"
#include "../State/C64UMemoryCache.h"

// Ultimate 64 uses plain HTTP on the local network; disable mbedTLS HTTPS
// support to avoid requiring mbedTLS headers in c64d's build target.
#define MT_ENABLE_MBEDTLS 0
#include "httplib.h"

#include "DBG_Log.h"

#include <chrono>
#include <cstdio>
#include <sstream>

#define LOGU(...) LOGD(__VA_ARGS__)

C64URestClient::C64URestClient()
	: port(80), memoryCache(nullptr), isRunning(false), fixtureMode(false)
{
}

void C64URestClient::SetMemoryCache(C64UMemoryCache *cache)
{
	memoryCache = cache;
}

C64URestClient::~C64URestClient()
{
	Stop();
}

void C64URestClient::SetEndpoint(const std::string &host, int port, const std::string &password)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	this->host = host;
	this->port = port;
	this->password = password;
}

void C64URestClient::Start()
{
	if (isRunning.exchange(true))
		return;

	workerThread = std::thread(&C64URestClient::WorkerLoop, this);
}

void C64URestClient::Stop()
{
	if (!isRunning.exchange(false))
		return;

	if (workerThread.joinable())
		workerThread.join();

	std::lock_guard<std::mutex> lock(stateMutex);
	commands.clear();
}

// --- Schedule methods (existing) ---

void C64URestClient::ScheduleDeviceInfo()
{
	Enqueue("device-info", "/v1/info", "GET");
}

void C64URestClient::ScheduleAuthenticate()
{
	// Authentication is handled via X-Password header on every request.
	// This schedules a device-info GET to verify the password works.
	Enqueue("auth", "/v1/info", "GET");
}

void C64URestClient::ScheduleReset(bool hardReset)
{
	if (hardReset)
	{
		Enqueue("hard-reset", "/v1/machine:reboot", "PUT");
	}
	else
	{
		Enqueue("soft-reset", "/v1/machine:reset", "PUT");
	}
}

void C64URestClient::ScheduleReboot()
{
	Enqueue("reboot", "/v1/machine:reboot", "PUT");
}

void C64URestClient::ScheduleLoadPrg(const std::string &path, bool autoRun)
{
	Enqueue(autoRun ? "run-prg" : "load-prg", FormatRunPrgUrl(path), "PUT");
}

void C64URestClient::ScheduleRunCrt(const std::string &path)
{
	Enqueue("run-crt", FormatRunCrtUrl(path), "PUT");
}

void C64URestClient::ScheduleMountDisk(const std::string &path, int driveId)
{
	Enqueue("mount-disk", FormatMountDiskUrl(driveId, path, "rw"), "PUT");
}

void C64URestClient::ScheduleRemoveDisk(int driveId)
{
	char url[128];
	snprintf(url, sizeof(url), "/v1/drives/%c:remove", 'a' + driveId);
	Enqueue("remove-disk", url, "PUT");
}

void C64URestClient::ScheduleResetDisk(int driveId)
{
	// Reset disk by removing and re-mounting is not directly supported;
	// use remove as the closest equivalent
	char url[128];
	snprintf(url, sizeof(url), "/v1/drives/%c:remove", 'a' + driveId);
	Enqueue("reset-disk", url, "PUT");
}

// --- New schedule methods ---

void C64URestClient::ScheduleReadMemory(int address, int length)
{
	Enqueue("read-memory", FormatReadMemoryUrl(address, length), "GET");
}

void C64URestClient::ScheduleWriteMemory(int address, const uint8_t *data, int length)
{
	EnqueueWithData("write-memory", FormatWriteMemoryUrl(address), "POST", data, length);
}

void C64URestClient::ScheduleWriteMemoryHex(int address, const std::string &hexData)
{
	Enqueue("write-memory-hex", FormatWriteMemoryHexUrl(address, hexData), "PUT");
}

void C64URestClient::SchedulePause()
{
	Enqueue("pause", "/v1/machine:pause", "PUT");
}

void C64URestClient::ScheduleResume()
{
	Enqueue("resume", "/v1/machine:resume", "PUT");
}

void C64URestClient::ScheduleStreamStart(const std::string &streamName, const std::string &localIp, int localPort)
{
	Enqueue("stream-start-" + streamName, FormatStreamStartUrl(streamName, localIp, localPort), "PUT");
}

void C64URestClient::ScheduleStreamStop(const std::string &streamName)
{
	Enqueue("stream-stop-" + streamName, FormatStreamStopUrl(streamName), "PUT");
}

void C64URestClient::ScheduleGetDriveInfo()
{
	Enqueue("get-drives", "/v1/drives", "GET");
}

// --- Result access ---

C64URestCommandResult C64URestClient::GetLastResult() const
{
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastResult;
}

// --- Fixture mode ---

void C64URestClient::SetFixtureMode(bool enabled)
{
	fixtureMode.store(enabled);
}

bool C64URestClient::IsFixtureMode() const
{
	return fixtureMode.load();
}

// --- Static URL formatters ---

std::string C64URestClient::FormatReadMemoryUrl(int address, int length)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "/v1/machine:readmem?address=%X&length=%d", address, length);
	return std::string(buf);
}

std::string C64URestClient::FormatWriteMemoryUrl(int address)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "/v1/machine:writemem?address=%X", address);
	return std::string(buf);
}

std::string C64URestClient::FormatWriteMemoryHexUrl(int address, const std::string &hexData)
{
	// Validate: only hex characters allowed
	std::string validated;
	validated.reserve(hexData.size());
	for (char c : hexData)
	{
		if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
			validated.push_back(c);
		else
			return std::string();  // invalid input
	}
	// Truncate to 256 hex chars (128 bytes) if longer
	if (validated.size() > 256)
		validated.resize(256);

	if (validated.empty())
		return std::string();

	char buf[512];
	snprintf(buf, sizeof(buf), "/v1/machine:writemem?address=%X&data=%s", address, validated.c_str());
	return std::string(buf);
}

std::string C64URestClient::FormatStreamStartUrl(const std::string &streamName, const std::string &localIp, int localPort)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "/v1/streams/%s:start?ip=%s:%d", streamName.c_str(), localIp.c_str(), localPort);
	return std::string(buf);
}

std::string C64URestClient::FormatStreamStopUrl(const std::string &streamName)
{
	return std::string("/v1/streams/") + streamName + ":stop";
}

std::string C64URestClient::FormatRunPrgUrl(const std::string &filePath)
{
	return std::string("/v1/runners:run_prg?file=") + filePath;
}

std::string C64URestClient::FormatRunCrtUrl(const std::string &filePath)
{
	return std::string("/v1/runners:run_crt?file=") + filePath;
}

std::string C64URestClient::FormatMountDiskUrl(int driveId, const std::string &imagePath, const std::string &mode)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "/v1/drives/%c:mount?image=", 'a' + driveId);
	return std::string(buf) + imagePath + "&mode=" + mode;
}

// --- Response parsers ---

bool C64URestClient::ParseReadMemoryResponse(const std::string &responseBody, std::vector<uint8_t> &outData)
{
	if (responseBody.empty())
		return false;

	outData.assign(responseBody.begin(), responseBody.end());
	return true;
}

bool C64URestClient::ParseDeviceInfoResponse(const std::string &responseBody, std::string &outBoardType, std::string &outVersion)
{
	// Simple JSON parsing for {"board": "...", "version": "..."} without external JSON lib.
	// Looks for "board" and "version" keys in the response body.

	auto extractValue = [&](const std::string &json, const std::string &key) -> std::string
	{
		std::string searchKey = "\"" + key + "\"";
		size_t keyPos = json.find(searchKey);
		if (keyPos == std::string::npos)
			return "";

		size_t colonPos = json.find(':', keyPos + searchKey.length());
		if (colonPos == std::string::npos)
			return "";

		size_t quoteStart = json.find('"', colonPos + 1);
		if (quoteStart == std::string::npos)
			return "";

		size_t quoteEnd = json.find('"', quoteStart + 1);
		if (quoteEnd == std::string::npos)
			return "";

		return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
	};

	outBoardType = extractValue(responseBody, "board");
	outVersion = extractValue(responseBody, "version");

	return !outBoardType.empty() || !outVersion.empty();
}

// --- Password header helpers ---

std::string C64URestClient::GetPasswordHeaderKey()
{
	return "X-Password";
}

std::string C64URestClient::GetPasswordHeaderValue(const std::string &password)
{
	return password;
}

// --- Queue management ---

void C64URestClient::Enqueue(const std::string &name, const std::string &url, const std::string &method)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	Command cmd;
	cmd.name = name;
	cmd.url = url;
	cmd.method = method;
	commands.push_back(std::move(cmd));
}

void C64URestClient::EnqueueWithData(const std::string &name, const std::string &url, const std::string &method,
									 const uint8_t *data, int dataLength)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	Command cmd;
	cmd.name = name;
	cmd.url = url;
	cmd.method = method;
	if (data && dataLength > 0)
	{
		cmd.data.assign(data, data + dataLength);
	}
	commands.push_back(std::move(cmd));
}

// --- Worker thread ---

void C64URestClient::WorkerLoop()
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

		if (fixtureMode.load())
		{
			// In fixture mode, simulate success without making real HTTP calls
			LOGU("C64URestClient::WorkerLoop: fixture mode, simulating %s %s",
				 command.method.c_str(), command.url.c_str());
			StoreResult(true, 200, command.name, "fixture: " + command.url);
			continue;
		}

		ExecuteHttpCommand(command);
	}
}

void C64URestClient::ExecuteHttpCommand(const Command &command)
{
	std::string currentHost;
	int currentPort;
	std::string currentPassword;

	{
		std::lock_guard<std::mutex> lock(stateMutex);
		currentHost = host;
		currentPort = port;
		currentPassword = password;
	}

	LOGU("C64URestClient::ExecuteHttpCommand: %s %s -> %s:%d",
		 command.method.c_str(), command.url.c_str(), currentHost.c_str(), currentPort);

	httplib::Client client(currentHost, currentPort);
	client.set_connection_timeout(5);
	client.set_read_timeout(5);

	httplib::Headers headers;
	if (!currentPassword.empty())
	{
		headers.insert({GetPasswordHeaderKey(), GetPasswordHeaderValue(currentPassword)});
	}

	httplib::Result result;

	if (command.method == "GET")
	{
		result = client.Get(command.url, headers);
	}
	else if (command.method == "PUT")
	{
		// PUT with empty body and Connection: close for stream commands
		headers.insert({"Connection", "close"});
		result = client.Put(command.url, headers, "", 0, "application/octet-stream");
	}
	else if (command.method == "POST")
	{
		// POST with binary data (memory write)
		const char *bodyPtr = command.data.empty() ? "" : reinterpret_cast<const char *>(command.data.data());
		size_t bodyLen = command.data.size();
		result = client.Post(command.url, headers, bodyPtr, bodyLen, "application/octet-stream");
	}
	else
	{
		LOGU("C64URestClient::ExecuteHttpCommand: unknown method '%s'", command.method.c_str());
		StoreResult(false, 0, command.name, "Unknown HTTP method: " + command.method);
		return;
	}

	if (!result)
	{
		LOGU("C64URestClient::ExecuteHttpCommand: connection failed for %s", command.url.c_str());
		StoreResult(false, 0, command.name, "Connection failed");
		return;
	}

	int status = result->status;
	bool success = (status >= 200 && status < 300);

	std::vector<uint8_t> responseBody;
	if (!result->body.empty())
	{
		responseBody.assign(result->body.begin(), result->body.end());
	}

	// Check for application-level errors in JSON responses.
	// The Ultimate 64 API may return HTTP 200 with an "errors" array in the JSON body.
	if (success && !result->body.empty())
	{
		auto ct = result->get_header_value("Content-Type");
		if (ct.find("application/json") != std::string::npos)
		{
			// Simple check for non-empty "errors" array without a full JSON parser.
			// Looks for "errors" : [ ... ] where the array is non-empty.
			size_t errPos = result->body.find("\"errors\"");
			if (errPos != std::string::npos)
			{
				size_t bracketPos = result->body.find('[', errPos);
				if (bracketPos != std::string::npos)
				{
					// Skip whitespace after '['
					size_t afterBracket = bracketPos + 1;
					while (afterBracket < result->body.size() &&
						   (result->body[afterBracket] == ' ' || result->body[afterBracket] == '\t' ||
							result->body[afterBracket] == '\n' || result->body[afterBracket] == '\r'))
					{
						afterBracket++;
					}
					// If the next char is NOT ']', the array is non-empty => error
					if (afterBracket < result->body.size() && result->body[afterBracket] != ']')
					{
						success = false;
						LOGU("C64URestClient::ExecuteHttpCommand: %s %s -> 200 but JSON errors array is non-empty",
							 command.method.c_str(), command.url.c_str());
					}
				}
			}
		}
	}

	LOGU("C64URestClient::ExecuteHttpCommand: %s %s -> %d %s",
		 command.method.c_str(), command.url.c_str(), status, success ? "OK" : "FAIL");

	StoreResult(success, status, command.name, result->body, responseBody);

	// Feed read-memory responses back into the memory cache
	if (success && command.name == "read-memory" && memoryCache != nullptr && !responseBody.empty())
	{
		// Parse address from URL: "/v1/machine:readmem?address=XXXX&length=NNN"
		size_t addrPos = command.url.find("address=");
		if (addrPos != std::string::npos)
		{
			int address = (int)strtol(command.url.c_str() + addrPos + 8, nullptr, 16);
			memoryCache->UpdatePageFromNetwork(address, responseBody.data(), (int)responseBody.size());
		}
	}
}

void C64URestClient::StoreResult(bool success, int statusCode, const std::string &name,
								 const std::string &detail, const std::vector<uint8_t> &body)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	lastResult.success = success;
	lastResult.statusCode = statusCode;
	lastResult.name = name;
	lastResult.detail = detail;
	lastResult.body = body;
}
