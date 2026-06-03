#include "C64UFtpClient.h"
#include "DBG_Log.h"

#include <cstring>
#include <cstdlib>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
// Undo Windows API macros that conflict with our method names
#undef GetCurrentDirectory
#undef DeleteFile
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#define SOCKET_ERROR_VAL SOCKET_ERROR
#define INVALID_SOCK INVALID_SOCKET
#define CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define SOCKET_ERROR_VAL -1
#define INVALID_SOCK -1
#define CLOSE_SOCKET ::close
#endif

#define LOGFTP(...) LOGD(__VA_ARGS__)

C64UFtpClient::C64UFtpClient()
	: controlSocket(-1), connected(false)
{
}

C64UFtpClient::~C64UFtpClient()
{
	Disconnect();
}

bool C64UFtpClient::Connect(const std::string &host, int port)
{
	std::lock_guard<std::mutex> lock(ftpMutex);

	if (connected)
		return false;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	struct hostent *he = gethostbyname(host.c_str());
	if (!he)
	{
		LOGFTP("C64UFtpClient::Connect: gethostbyname failed for %s", host.c_str());
		return false;
	}
	memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

	controlSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (controlSocket < 0)
	{
		LOGFTP("C64UFtpClient::Connect: socket() failed");
		return false;
	}

	SetSocketTimeout(controlSocket, TIMEOUT_SECONDS);

	if (::connect(controlSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		LOGFTP("C64UFtpClient::Connect: connect() failed for %s:%d", host.c_str(), port);
		CloseSocket(controlSocket);
		controlSocket = -1;
		return false;
	}

	// Read welcome banner
	std::string welcome = ReadResponse();
	if (ParseResponseCode(welcome) != 220)
	{
		LOGFTP("C64UFtpClient::Connect: unexpected welcome response: %s", welcome.c_str());
		CloseSocket(controlSocket);
		controlSocket = -1;
		return false;
	}

	connected = true;
	LOGFTP("C64UFtpClient::Connect: connected to %s:%d", host.c_str(), port);
	return true;
}

bool C64UFtpClient::Login(const std::string &user, const std::string &password)
{
	std::lock_guard<std::mutex> lock(ftpMutex);

	if (!connected)
		return false;

	// Send USER
	if (!SendCommand("USER " + user + "\r\n"))
		return false;

	std::string userResp = ReadResponse();
	int userCode = ParseResponseCode(userResp);
	if (userCode != 331 && userCode != 230)
	{
		LOGFTP("C64UFtpClient::Login: USER failed: %s", userResp.c_str());
		return false;
	}

	// If already logged in (230), no need for PASS
	if (userCode == 230)
		return true;

	// Send PASS
	if (!SendCommand("PASS " + password + "\r\n"))
		return false;

	std::string passResp = ReadResponse();
	if (ParseResponseCode(passResp) != 230)
	{
		LOGFTP("C64UFtpClient::Login: PASS failed: %s", passResp.c_str());
		return false;
	}

	LOGFTP("C64UFtpClient::Login: logged in as %s", user.c_str());
	return true;
}

void C64UFtpClient::Disconnect()
{
	std::lock_guard<std::mutex> lock(ftpMutex);

	if (!connected)
		return;

	SendCommand("QUIT\r\n");
	// Read response but don't care about result
	ReadResponse();

	CloseSocket(controlSocket);
	controlSocket = -1;
	connected = false;

	LOGFTP("C64UFtpClient::Disconnect: disconnected");
}

bool C64UFtpClient::IsConnected() const
{
	return connected;
}

bool C64UFtpClient::ChangeDirectory(const std::string &path)
{
	std::lock_guard<std::mutex> lock(ftpMutex);

	if (!connected)
		return false;

	if (!SendCommand("CWD " + path + "\r\n"))
		return false;

	std::string resp = ReadResponse();
	return ParseResponseCode(resp) == 250;
}

bool C64UFtpClient::ChangeToParent()
{
	std::lock_guard<std::mutex> lock(ftpMutex);

	if (!connected)
		return false;

	if (!SendCommand("CDUP\r\n"))
		return false;

	std::string resp = ReadResponse();
	int code = ParseResponseCode(resp);
	return (code == 200 || code == 250);
}

std::string C64UFtpClient::GetCurrentDirectory()
{
	std::lock_guard<std::mutex> lock(ftpMutex);

	if (!connected)
		return "";

	if (!SendCommand("PWD\r\n"))
		return "";

	std::string resp = ReadResponse();
	if (ParseResponseCode(resp) != 257)
		return "";

	// Parse path from 257 "path" response
	size_t firstQuote = resp.find('"');
	size_t secondQuote = resp.find('"', firstQuote + 1);
	if (firstQuote != std::string::npos && secondQuote != std::string::npos)
	{
		return resp.substr(firstQuote + 1, secondQuote - firstQuote - 1);
	}

	return "";
}

std::vector<C64UFtpEntry> C64UFtpClient::ListDirectory()
{
	std::lock_guard<std::mutex> lock(ftpMutex);

	std::vector<C64UFtpEntry> entries;
	if (!connected)
		return entries;

	// Enter passive mode
	std::string dataHost;
	int dataPort;
	if (!EnterPassiveMode(dataHost, dataPort))
		return entries;

	// Connect data socket
	int dataSock = ConnectDataSocket(dataHost, dataPort);
	if (dataSock < 0)
		return entries;

	// Send LIST command
	if (!SendCommand("LIST\r\n"))
	{
		CloseSocket(dataSock);
		return entries;
	}

	std::string listResp = ReadResponse();
	int listCode = ParseResponseCode(listResp);
	if (listCode != 150 && listCode != 125)
	{
		LOGFTP("C64UFtpClient::ListDirectory: LIST failed: %s", listResp.c_str());
		CloseSocket(dataSock);
		return entries;
	}

	// Read all data from data socket
	std::string listData;
	char buf[BUFFER_SIZE];
	while (true)
	{
		int n = (int)recv(dataSock, buf, sizeof(buf), 0);
		if (n <= 0)
			break;
		listData.append(buf, n);
	}

	CloseSocket(dataSock);

	// Read transfer complete response
	std::string completeResp = ReadResponse();
	if (ParseResponseCode(completeResp) != 226)
	{
		LOGFTP("C64UFtpClient::ListDirectory: transfer complete not received: %s", completeResp.c_str());
	}

	entries = ParseListOutput(listData);
	return entries;
}

bool C64UFtpClient::DownloadFile(const std::string &remotePath, std::vector<uint8_t> &outData)
{
	std::lock_guard<std::mutex> lock(ftpMutex);

	outData.clear();
	if (!connected)
		return false;

	// Set binary mode
	if (!SendCommand("TYPE I\r\n"))
		return false;

	std::string typeResp = ReadResponse();
	if (ParseResponseCode(typeResp) != 200)
	{
		LOGFTP("C64UFtpClient::DownloadFile: TYPE I failed: %s", typeResp.c_str());
		return false;
	}

	// Enter passive mode
	std::string dataHost;
	int dataPort;
	if (!EnterPassiveMode(dataHost, dataPort))
		return false;

	// Connect data socket
	int dataSock = ConnectDataSocket(dataHost, dataPort);
	if (dataSock < 0)
		return false;

	// Send RETR command
	if (!SendCommand("RETR " + remotePath + "\r\n"))
	{
		CloseSocket(dataSock);
		return false;
	}

	std::string retrResp = ReadResponse();
	int retrCode = ParseResponseCode(retrResp);
	if (retrCode != 150 && retrCode != 125)
	{
		LOGFTP("C64UFtpClient::DownloadFile: RETR failed: %s", retrResp.c_str());
		CloseSocket(dataSock);
		return false;
	}

	// Read all data from data socket
	char buf[BUFFER_SIZE];
	while (true)
	{
		int n = (int)recv(dataSock, buf, sizeof(buf), 0);
		if (n <= 0)
			break;
		outData.insert(outData.end(), buf, buf + n);
	}

	CloseSocket(dataSock);

	// Read transfer complete response
	std::string completeResp = ReadResponse();
	if (ParseResponseCode(completeResp) != 226)
	{
		LOGFTP("C64UFtpClient::DownloadFile: transfer complete not received: %s", completeResp.c_str());
		return false;
	}

	LOGFTP("C64UFtpClient::DownloadFile: downloaded %d bytes from %s", (int)outData.size(), remotePath.c_str());
	return true;
}

bool C64UFtpClient::UploadFile(const std::string &remotePath, const uint8_t *data, size_t length)
{
	std::lock_guard<std::mutex> lock(ftpMutex);

	if (!connected)
		return false;

	// Set binary mode
	if (!SendCommand("TYPE I\r\n"))
		return false;

	std::string typeResp = ReadResponse();
	if (ParseResponseCode(typeResp) != 200)
	{
		LOGFTP("C64UFtpClient::UploadFile: TYPE I failed: %s", typeResp.c_str());
		return false;
	}

	// Enter passive mode
	std::string dataHost;
	int dataPort;
	if (!EnterPassiveMode(dataHost, dataPort))
		return false;

	// Connect data socket
	int dataSock = ConnectDataSocket(dataHost, dataPort);
	if (dataSock < 0)
		return false;

	// Send STOR command
	if (!SendCommand("STOR " + remotePath + "\r\n"))
	{
		CloseSocket(dataSock);
		return false;
	}

	std::string storResp = ReadResponse();
	int storCode = ParseResponseCode(storResp);
	if (storCode != 150 && storCode != 125)
	{
		LOGFTP("C64UFtpClient::UploadFile: STOR failed: %s", storResp.c_str());
		CloseSocket(dataSock);
		return false;
	}

	// Send all data to data socket
	size_t sent = 0;
	while (sent < length)
	{
		int n = (int)send(dataSock, (const char *)(data + sent), (int)(length - sent), 0);
		if (n <= 0)
		{
			LOGFTP("C64UFtpClient::UploadFile: send() failed at offset %d", (int)sent);
			CloseSocket(dataSock);
			return false;
		}
		sent += n;
	}

	CloseSocket(dataSock);

	// Read transfer complete response
	std::string completeResp = ReadResponse();
	if (ParseResponseCode(completeResp) != 226)
	{
		LOGFTP("C64UFtpClient::UploadFile: transfer complete not received: %s", completeResp.c_str());
		return false;
	}

	LOGFTP("C64UFtpClient::UploadFile: uploaded %d bytes to %s", (int)length, remotePath.c_str());
	return true;
}

bool C64UFtpClient::DeleteFile(const std::string &remotePath)
{
	std::lock_guard<std::mutex> lock(ftpMutex);

	if (!connected)
		return false;

	if (!SendCommand("DELE " + remotePath + "\r\n"))
		return false;

	std::string resp = ReadResponse();
	return ParseResponseCode(resp) == 250;
}

// --- Private methods ---

bool C64UFtpClient::SendCommand(const std::string &cmd)
{
	if (controlSocket < 0)
		return false;

	int totalSent = 0;
	int len = (int)cmd.length();
	while (totalSent < len)
	{
		int n = (int)send(controlSocket, cmd.c_str() + totalSent, len - totalSent, 0);
		if (n <= 0)
		{
			LOGFTP("C64UFtpClient::SendCommand: send() failed");
			return false;
		}
		totalSent += n;
	}
	return true;
}

std::string C64UFtpClient::ReadResponse()
{
	// Read FTP response lines until we get a final line (3 digits + space, not dash)
	std::string accumulated;
	char buf[BUFFER_SIZE];

	while (true)
	{
		int n = (int)recv(controlSocket, buf, sizeof(buf) - 1, 0);
		if (n <= 0)
			break;

		buf[n] = '\0';
		accumulated.append(buf, n);

		// Check if we have a complete final response line
		// A final response line starts with 3 digits followed by a space (not a dash)
		// Multi-line responses use 3 digits + dash for continuation
		size_t pos = 0;
		bool foundFinal = false;
		while (pos < accumulated.size())
		{
			size_t lineEnd = accumulated.find('\n', pos);
			if (lineEnd == std::string::npos)
				break;

			std::string line = accumulated.substr(pos, lineEnd - pos);
			// Remove trailing \r
			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			// Check if this line is a final response: 3 digits + space
			if (line.size() >= 4 && isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2]) && line[3] == ' ')
			{
				foundFinal = true;
			}

			pos = lineEnd + 1;
		}

		if (foundFinal)
			break;
	}

	return accumulated;
}

bool C64UFtpClient::EnterPassiveMode(std::string &dataHost, int &dataPort)
{
	if (!SendCommand("PASV\r\n"))
		return false;

	std::string resp = ReadResponse();
	return ParsePasvResponse(resp, dataHost, dataPort);
}

int C64UFtpClient::ConnectDataSocket(const std::string &host, int port)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	struct hostent *he = gethostbyname(host.c_str());
	if (!he)
	{
		LOGFTP("C64UFtpClient::ConnectDataSocket: gethostbyname failed for %s", host.c_str());
		return -1;
	}
	memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		LOGFTP("C64UFtpClient::ConnectDataSocket: socket() failed");
		return -1;
	}

	SetSocketTimeout(sock, TIMEOUT_SECONDS);

	if (::connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		LOGFTP("C64UFtpClient::ConnectDataSocket: connect() failed for %s:%d", host.c_str(), port);
		CloseSocket(sock);
		return -1;
	}

	return sock;
}

void C64UFtpClient::CloseSocket(int sock)
{
	if (sock >= 0)
	{
		CLOSE_SOCKET(sock);
	}
}

void C64UFtpClient::SetSocketTimeout(int sock, int timeoutSeconds)
{
#ifdef _WIN32
	DWORD timeout = timeoutSeconds * 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
#else
	struct timeval tv;
	tv.tv_sec = timeoutSeconds;
	tv.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

// --- Static parsing methods ---

int C64UFtpClient::ParseResponseCode(const std::string &response)
{
	if (response.size() < 3)
		return 0;

	if (!isdigit(response[0]) || !isdigit(response[1]) || !isdigit(response[2]))
		return 0;

	return (response[0] - '0') * 100 + (response[1] - '0') * 10 + (response[2] - '0');
}

bool C64UFtpClient::ParsePasvResponse(const std::string &response, std::string &outHost, int &outPort)
{
	// Parse 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
	if (ParseResponseCode(response) != 227)
		return false;

	size_t openParen = response.find('(');
	size_t closeParen = response.find(')', openParen);
	if (openParen == std::string::npos || closeParen == std::string::npos)
		return false;

	std::string nums = response.substr(openParen + 1, closeParen - openParen - 1);

	int h1, h2, h3, h4, p1, p2;
	if (sscanf(nums.c_str(), "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6)
		return false;

	char hostBuf[64];
	snprintf(hostBuf, sizeof(hostBuf), "%d.%d.%d.%d", h1, h2, h3, h4);
	outHost = hostBuf;
	outPort = p1 * 256 + p2;

	return true;
}

std::vector<C64UFtpEntry> C64UFtpClient::ParseListOutput(const std::string &listData)
{
	std::vector<C64UFtpEntry> entries;

	if (listData.empty())
		return entries;

	std::istringstream stream(listData);
	std::string line;

	while (std::getline(stream, line))
	{
		// Remove trailing \r
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		// Skip empty lines
		if (line.empty())
			continue;

		// Try Unix ls -l format: first char is 'd' (directory) or '-' (file) or 'l' (link)
		if (line.size() > 10 && (line[0] == 'd' || line[0] == '-' || line[0] == 'l'))
		{
			bool isDir = (line[0] == 'd');

			// Split line by whitespace to extract fields
			// Format: perms links owner group size month day time/year name
			std::vector<std::string> fields;
			std::istringstream lineStream(line);
			std::string field;
			while (lineStream >> field)
			{
				fields.push_back(field);
			}

			// Need at least 9 fields for standard ls -l output
			if (fields.size() < 9)
				continue;

			// Size is field index 4
			uint64_t size = 0;
			size = strtoull(fields[4].c_str(), nullptr, 10);

			// Name is field index 8 (and possibly beyond, for names with spaces)
			// Reconstruct name from field 8 onwards
			std::string name;
			for (size_t i = 8; i < fields.size(); i++)
			{
				if (!name.empty())
					name += " ";
				name += fields[i];
			}

			// Skip . and .. entries
			if (name == "." || name == "..")
				continue;

			// For symlinks, strip " -> target" from name
			if (line[0] == 'l')
			{
				size_t arrowPos = name.find(" -> ");
				if (arrowPos != std::string::npos)
					name = name.substr(0, arrowPos);
			}

			C64UFtpEntry entry;
			entry.name = name;
			entry.isDirectory = isDir;
			entry.size = size;
			entries.push_back(entry);
		}
		else
		{
			// Fallback: simple one-name-per-line format
			// Trim whitespace
			size_t start = line.find_first_not_of(" \t");
			size_t end = line.find_last_not_of(" \t");
			if (start == std::string::npos)
				continue;

			std::string name = line.substr(start, end - start + 1);

			// Skip . and ..
			if (name == "." || name == "..")
				continue;

			C64UFtpEntry entry;
			entry.name = name;
			entry.isDirectory = false;  // can't tell from name alone
			entry.size = 0;
			entries.push_back(entry);
		}
	}

	return entries;
}
