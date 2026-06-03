#ifndef _C64UTELNETCLIENT_H_
#define _C64UTELNETCLIENT_H_

#include <string>
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstdint>

class C64UTelnetClient
{
public:
	C64UTelnetClient();
	~C64UTelnetClient();

	bool Connect(const std::string &host, int port = 23);
	void Disconnect();
	bool IsConnected() const;

	void Send(const uint8_t *data, size_t len);
	void SetDataCallback(std::function<void(const uint8_t*, size_t)> callback);

	// Exposed for testing
	static void StripIAC(const uint8_t *input, size_t inputLen,
						 std::vector<uint8_t> &cleanOutput,
						 std::vector<std::vector<uint8_t>> &iacCommands);

private:
	void ReadThread();
	void ProcessRecvBuffer(const uint8_t *buffer, int len);
	void HandleIACCommand(uint8_t cmd, uint8_t option);
	void HandleSubnegotiation(const std::vector<uint8_t> &data);
	void SendIAC(uint8_t cmd, uint8_t option);
	void CloseSocket();

	int sock;
	std::atomic<bool> connected;
	std::atomic<bool> shouldStop;
	std::thread readThread;
	std::mutex sendMutex;
	std::function<void(const uint8_t*, size_t)> dataCallback;

	// IAC parser state
	enum IACState { STATE_NORMAL, STATE_IAC, STATE_OPTION_WILL, STATE_OPTION_WONT,
					STATE_OPTION_DO, STATE_OPTION_DONT, STATE_SUBNEG, STATE_SUBNEG_IAC };
	IACState iacState;
	std::vector<uint8_t> subnegBuffer;

	// Telnet protocol constants
	static constexpr uint8_t IAC = 0xFF;
	static constexpr uint8_t WILL = 0xFB;
	static constexpr uint8_t WONT = 0xFC;
	static constexpr uint8_t DO_CMD = 0xFD;
	static constexpr uint8_t DONT = 0xFE;
	static constexpr uint8_t SB = 0xFA;
	static constexpr uint8_t SE = 0xF0;
	// Options
	static constexpr uint8_t OPT_ECHO = 1;
	static constexpr uint8_t OPT_SGA = 3;    // Suppress Go Ahead
	static constexpr uint8_t OPT_TTYPE = 24;
	static constexpr uint8_t OPT_NAWS = 31;

	static constexpr int RECV_BUFFER_SIZE = 4096;
	static constexpr int TIMEOUT_SECONDS = 15;
};

#endif
