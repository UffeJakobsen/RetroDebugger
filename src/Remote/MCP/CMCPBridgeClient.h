#ifndef _CMCPBridgeClient_h_
#define _CMCPBridgeClient_h_

#include "CDebuggerServer.h"
#include "SYS_Threading.h"
#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

class CMCPServer;

// Bridge connection states
enum MCPBridgeState
{
	MCP_BRIDGE_DISCONNECTED = 0,
	MCP_BRIDGE_CONNECTING,
	MCP_BRIDGE_CONNECTED,
	MCP_BRIDGE_STALE         // was connected, lost connection
};

// Callback for state transitions (old state, new state)
typedef std::function<void(MCPBridgeState oldState, MCPBridgeState newState)> MCPBridgeStateCallback;

class CMCPBridgeClient : public CDebuggerServer
{
public:
	CMCPBridgeClient(const std::string &host, int port, const std::string &path);
	virtual ~CMCPBridgeClient();

	virtual void Start();
	virtual void Stop();
	virtual void ThreadRun(void *passData);

	// CDebuggerServer interface — forwards calls over WebSocket
	virtual void AddEndpointFunction(const std::string &endpointName, std::function<std::vector<char> *(const std::string, const nlohmann::json, u8 *, int)> func);
	virtual void AddEndpointFunction(const EndpointDescriptor &desc, EndpointHandlerV1 handler);
	virtual std::vector<char> *RunEndpointFunction(const std::string &endpointName, const std::string token, nlohmann::json params, u8 *binaryData, int binaryDataSize);
	virtual std::vector<char> *PrepareResult(int status, const std::string token, nlohmann::json resultJson, u8 *binaryData, int binaryDataSize);
	virtual std::vector<char> *RunEndpointFunction(const std::string &endpointName, const RequestContext &ctx, nlohmann::json params, u8 *binaryData, int binaryDataSize);
	virtual std::vector<char> *PrepareResult(int status, const RequestContext &ctx, nlohmann::json resultJson, u8 *binaryData, int binaryDataSize);
	virtual void BroadcastEvent(const char *eventName, nlohmann::json j);
	virtual bool AreClientsConnected();
	virtual std::vector<EndpointDescriptor> GetEndpointDescriptors();

	// Bridge-specific
	MCPBridgeState GetState();
	std::string GetStateString();
	bool TryConnect();
	void Disconnect();
	bool RunDiscovery();
	nlohmann::json GetDiagnostics();

	// Poll remote platforms — returns true if changed
	bool PollPlatformState();

	// State change callback
	void SetStateCallback(MCPBridgeStateCallback cb);

	// MCP server back-reference — set by CMCPServer::SetBridgeMode so the bridge can
	// forward desktop broadcast events as MCP notifications to the LLM client.
	void SetMCPServer(CMCPServer *server) { mcpServer = server; }

	// Discovery results — platformsMutex guards remotePlatforms (written by bridge thread, read by MCP thread)
	std::mutex platformsMutex;
	int remoteProtocolVersion;
	int remoteDiscoveryVersion;
	std::string remoteServerName;
	std::string remoteServerVersion;
	nlohmann::json remotePlatforms;
	std::vector<EndpointDescriptor> remoteEndpoints;
	nlohmann::json remoteFeatures;

	// Thread-safe copy of remotePlatforms
	nlohmann::json GetRemotePlatformsCopy()
	{
		std::lock_guard<std::mutex> lock(platformsMutex);
		return remotePlatforms;
	}

private:
	std::string host;
	int port;
	std::string path;

	std::atomic<MCPBridgeState> state;
	int socketFd;
	std::mutex socketMutex;       // protects ALL WebSocket I/O
	std::mutex connectMutex;      // guards connect attempts (single at a time)

	std::string lastError;
	MCPBridgeStateCallback stateCallback;
	CMCPServer *mcpServer = nullptr;

	// Reconnect — constexpr so these are inline-defined; otherwise
	// odr-uses (e.g. passing to std::min / std::max) fail to link without
	// a separate out-of-class definition.
	int reconnectAttempts;
	static constexpr int kMaxReconnectBackoffMs = 10000;
	static constexpr int kInitialReconnectMs = 500;
	static constexpr int kRequestReconnectTimeoutMs = 3000;

	// State transition helper — fires callback
	void SetState(MCPBridgeState newState);

	// Internal connect without connectMutex (caller must hold it)
	bool TryConnectInternal();

	// WebSocket low-level
	bool WebSocketConnect();
	void WebSocketClose();
	std::vector<char> *WebSocketSendRequest(const std::string &fn, const nlohmann::json &params, u8 *binaryData, int binaryDataSize);
	bool WebSocketSendFrame(const std::vector<char> &data);
	std::vector<char> *WebSocketReadFrame();

	// Helpers
	std::string GenerateWebSocketKey();
	bool ValidateWebSocketAccept(const std::string &key, const std::string &acceptHeader);
	bool EnsureConnected();
};

#endif
