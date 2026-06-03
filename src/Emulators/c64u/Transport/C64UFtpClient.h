#ifndef _C64UFTPCLIENT_H_
#define _C64UFTPCLIENT_H_

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

struct C64UFtpEntry
{
	std::string name;
	bool isDirectory;
	uint64_t size;
};

class C64UFtpClient
{
public:
	C64UFtpClient();
	~C64UFtpClient();

	bool Connect(const std::string &host, int port = 21);
	bool Login(const std::string &user, const std::string &password);
	void Disconnect();
	bool IsConnected() const;

	bool ChangeDirectory(const std::string &path);
	bool ChangeToParent();
	std::string GetCurrentDirectory();
	std::vector<C64UFtpEntry> ListDirectory();

	bool DownloadFile(const std::string &remotePath, std::vector<uint8_t> &outData);
	bool UploadFile(const std::string &remotePath, const uint8_t *data, size_t length);
	bool DeleteFile(const std::string &remotePath);

	// Exposed for testing
	static int ParseResponseCode(const std::string &response);
	static bool ParsePasvResponse(const std::string &response, std::string &outHost, int &outPort);
	static std::vector<C64UFtpEntry> ParseListOutput(const std::string &listData);

private:
	bool SendCommand(const std::string &cmd);
	std::string ReadResponse();
	bool EnterPassiveMode(std::string &dataHost, int &dataPort);
	int ConnectDataSocket(const std::string &host, int port);
	void CloseSocket(int sock);
	void SetSocketTimeout(int sock, int timeoutSeconds);

	int controlSocket;
	bool connected;
	std::mutex ftpMutex;

	static const int BUFFER_SIZE = 4096;
	static const int TIMEOUT_SECONDS = 15;
};

#endif
