#include "C64UTelnetClient.h"
#include "DBG_Log.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#define CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#define CLOSE_SOCKET ::close
#endif

#define LOGTELNET(...) LOGD(__VA_ARGS__)

C64UTelnetClient::C64UTelnetClient()
	: sock(-1), connected(false), shouldStop(false), iacState(STATE_NORMAL)
{
}

C64UTelnetClient::~C64UTelnetClient()
{
	Disconnect();
}

bool C64UTelnetClient::Connect(const std::string &host, int port)
{
	if (connected.load())
		return false;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	struct hostent *he = gethostbyname(host.c_str());
	if (!he)
	{
		LOGTELNET("C64UTelnetClient::Connect: gethostbyname failed for %s", host.c_str());
		return false;
	}
	memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		LOGTELNET("C64UTelnetClient::Connect: socket() failed");
		return false;
	}

	// Set recv timeout
#ifdef _WIN32
	DWORD timeout = TIMEOUT_SECONDS * 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
#else
	struct timeval tv;
	tv.tv_sec = TIMEOUT_SECONDS;
	tv.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

	if (::connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		LOGTELNET("C64UTelnetClient::Connect: connect() failed for %s:%d", host.c_str(), port);
		CLOSE_SOCKET(sock);
		sock = -1;
		return false;
	}

	shouldStop = false;
	connected = true;
	iacState = STATE_NORMAL;

	LOGTELNET("C64UTelnetClient::Connect: connected to %s:%d", host.c_str(), port);

	// Start read thread
	readThread = std::thread(&C64UTelnetClient::ReadThread, this);

	return true;
}

void C64UTelnetClient::Disconnect()
{
	if (!connected.load() && !readThread.joinable())
		return;

	shouldStop = true;
	connected = false;

	CloseSocket();

	if (readThread.joinable())
	{
		readThread.join();
	}

	LOGTELNET("C64UTelnetClient::Disconnect: disconnected");
}

bool C64UTelnetClient::IsConnected() const
{
	return connected.load();
}

void C64UTelnetClient::Send(const uint8_t *data, size_t len)
{
	if (!connected.load() || sock < 0)
		return;

	std::lock_guard<std::mutex> lock(sendMutex);
	send(sock, (const char *)data, (int)len, 0);
}

void C64UTelnetClient::SetDataCallback(std::function<void(const uint8_t*, size_t)> callback)
{
	dataCallback = callback;
}

void C64UTelnetClient::ReadThread()
{
	uint8_t buffer[RECV_BUFFER_SIZE];
	while (!shouldStop.load())
	{
		int n = (int)recv(sock, (char *)buffer, RECV_BUFFER_SIZE, 0);
		if (n <= 0)
		{
			if (shouldStop.load())
				break;
			// Connection lost or timeout
			if (n == 0)
			{
				LOGTELNET("C64UTelnetClient::ReadThread: connection closed by remote");
				connected = false;
				break;
			}
			// n < 0: could be timeout, check errno
#ifdef _WIN32
			int err = WSAGetLastError();
			if (err == WSAETIMEDOUT)
				continue;
#else
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
#endif
			LOGTELNET("C64UTelnetClient::ReadThread: recv error");
			connected = false;
			break;
		}
		ProcessRecvBuffer(buffer, n);
	}
}

void C64UTelnetClient::ProcessRecvBuffer(const uint8_t *buffer, int len)
{
	// Process bytes through IAC state machine, batch clean data for callback
	std::vector<uint8_t> cleanBuf;
	cleanBuf.reserve(len);

	for (int i = 0; i < len; i++)
	{
		uint8_t b = buffer[i];

		switch (iacState)
		{
			case STATE_NORMAL:
				if (b == IAC)
				{
					// Flush accumulated clean data before processing IAC
					if (!cleanBuf.empty() && dataCallback)
					{
						dataCallback(cleanBuf.data(), cleanBuf.size());
						cleanBuf.clear();
					}
					iacState = STATE_IAC;
				}
				else
				{
					cleanBuf.push_back(b);
				}
				break;

			case STATE_IAC:
				if (b == WILL)
					iacState = STATE_OPTION_WILL;
				else if (b == WONT)
					iacState = STATE_OPTION_WONT;
				else if (b == DO_CMD)
					iacState = STATE_OPTION_DO;
				else if (b == DONT)
					iacState = STATE_OPTION_DONT;
				else if (b == SB)
				{
					subnegBuffer.clear();
					iacState = STATE_SUBNEG;
				}
				else if (b == IAC)
				{
					// Escaped IAC: literal 0xFF
					cleanBuf.push_back(0xFF);
					iacState = STATE_NORMAL;
				}
				else
				{
					// Unknown command, ignore
					iacState = STATE_NORMAL;
				}
				break;

			case STATE_OPTION_WILL:
				HandleIACCommand(WILL, b);
				iacState = STATE_NORMAL;
				break;

			case STATE_OPTION_WONT:
				HandleIACCommand(WONT, b);
				iacState = STATE_NORMAL;
				break;

			case STATE_OPTION_DO:
				HandleIACCommand(DO_CMD, b);
				iacState = STATE_NORMAL;
				break;

			case STATE_OPTION_DONT:
				HandleIACCommand(DONT, b);
				iacState = STATE_NORMAL;
				break;

			case STATE_SUBNEG:
				if (b == IAC)
					iacState = STATE_SUBNEG_IAC;
				else
					subnegBuffer.push_back(b);
				break;

			case STATE_SUBNEG_IAC:
				if (b == SE)
				{
					HandleSubnegotiation(subnegBuffer);
					iacState = STATE_NORMAL;
				}
				else
				{
					// Escaped IAC within subneg
					subnegBuffer.push_back(IAC);
					subnegBuffer.push_back(b);
					iacState = STATE_SUBNEG;
				}
				break;
		}
	}

	// Flush remaining clean data
	if (!cleanBuf.empty() && dataCallback)
	{
		dataCallback(cleanBuf.data(), cleanBuf.size());
	}
}

void C64UTelnetClient::HandleIACCommand(uint8_t cmd, uint8_t option)
{
	if (cmd == DO_CMD)
	{
		// Server asks us to enable something
		if (option == OPT_TTYPE || option == OPT_NAWS)
			SendIAC(WILL, option);  // We support terminal type and window size
		else
			SendIAC(WONT, option);  // Reject everything else
	}
	else if (cmd == WILL)
	{
		// Server offers a capability
		if (option == OPT_ECHO || option == OPT_SGA)
			SendIAC(DO_CMD, option);  // Accept echo and suppress-go-ahead
		else
			SendIAC(DONT, option);
	}
	// WONT/DONT from server: just acknowledge, no response needed
}

void C64UTelnetClient::HandleSubnegotiation(const std::vector<uint8_t> &data)
{
	if (data.empty())
		return;

	if (data[0] == OPT_TTYPE && data.size() > 1 && data[1] == 1)  // SEND
	{
		// Respond with terminal type "VT100"
		uint8_t response[] = { IAC, SB, OPT_TTYPE, 0, 'V', 'T', '1', '0', '0', IAC, SE };
		std::lock_guard<std::mutex> lock(sendMutex);
		send(sock, (char *)response, sizeof(response), 0);
	}
	else if (data[0] == OPT_NAWS)
	{
		// Send window size 80x25
		uint8_t response[] = { IAC, SB, OPT_NAWS, 0, 80, 0, 25, IAC, SE };
		std::lock_guard<std::mutex> lock(sendMutex);
		send(sock, (char *)response, sizeof(response), 0);
	}
}

void C64UTelnetClient::SendIAC(uint8_t cmd, uint8_t option)
{
	uint8_t buf[] = { IAC, cmd, option };
	std::lock_guard<std::mutex> lock(sendMutex);
	send(sock, (char *)buf, 3, 0);
}

void C64UTelnetClient::CloseSocket()
{
	if (sock >= 0)
	{
		CLOSE_SOCKET(sock);
		sock = -1;
	}
}

// Static method for testing: strip IAC sequences from input
void C64UTelnetClient::StripIAC(const uint8_t *input, size_t inputLen,
								std::vector<uint8_t> &cleanOutput,
								std::vector<std::vector<uint8_t>> &iacCommands)
{
	cleanOutput.clear();
	iacCommands.clear();

	enum { ST_NORMAL, ST_IAC, ST_WILL, ST_WONT, ST_DO, ST_DONT, ST_SUBNEG, ST_SUBNEG_IAC } state = ST_NORMAL;
	std::vector<uint8_t> subneg;

	for (size_t i = 0; i < inputLen; i++)
	{
		uint8_t b = input[i];

		switch (state)
		{
			case ST_NORMAL:
				if (b == 0xFF)
					state = ST_IAC;
				else
					cleanOutput.push_back(b);
				break;

			case ST_IAC:
				if (b == 0xFB)  // WILL
					state = ST_WILL;
				else if (b == 0xFC)  // WONT
					state = ST_WONT;
				else if (b == 0xFD)  // DO
					state = ST_DO;
				else if (b == 0xFE)  // DONT
					state = ST_DONT;
				else if (b == 0xFA)  // SB
				{
					subneg.clear();
					state = ST_SUBNEG;
				}
				else if (b == 0xFF)
				{
					// Escaped IAC
					cleanOutput.push_back(0xFF);
					state = ST_NORMAL;
				}
				else
				{
					state = ST_NORMAL;
				}
				break;

			case ST_WILL:
			{
				std::vector<uint8_t> cmd = { 0xFB, b };
				iacCommands.push_back(cmd);
				state = ST_NORMAL;
				break;
			}

			case ST_WONT:
			{
				std::vector<uint8_t> cmd = { 0xFC, b };
				iacCommands.push_back(cmd);
				state = ST_NORMAL;
				break;
			}

			case ST_DO:
			{
				std::vector<uint8_t> cmd = { 0xFD, b };
				iacCommands.push_back(cmd);
				state = ST_NORMAL;
				break;
			}

			case ST_DONT:
			{
				std::vector<uint8_t> cmd = { 0xFE, b };
				iacCommands.push_back(cmd);
				state = ST_NORMAL;
				break;
			}

			case ST_SUBNEG:
				if (b == 0xFF)
					state = ST_SUBNEG_IAC;
				else
					subneg.push_back(b);
				break;

			case ST_SUBNEG_IAC:
				if (b == 0xF0)  // SE
				{
					std::vector<uint8_t> cmd = { 0xFA };
					cmd.insert(cmd.end(), subneg.begin(), subneg.end());
					iacCommands.push_back(cmd);
					state = ST_NORMAL;
				}
				else
				{
					subneg.push_back(0xFF);
					subneg.push_back(b);
					state = ST_SUBNEG;
				}
				break;
		}
	}
}
