#ifndef _CVIEWWSLOG_H_
#define _CVIEWWSLOG_H_

#include "CGuiView.h"
#include <mutex>
#include <deque>
#include <string>
#include <chrono>

class CByteBuffer;

class CViewWSLog : public CGuiView
{
public:
	CViewWSLog(float posX, float posY, float posZ, float sizeX, float sizeY);

	virtual void RenderImGui();

	virtual void Serialize(CByteBuffer *byteBuffer);
	virtual void Deserialize(CByteBuffer *byteBuffer);

	// Display filters (serialized with layout)
	bool logConnections    = true;
	bool logRequests       = true;
	bool logResponses      = false;
	bool logResponseBody   = false;  // show response body (requires logResponses)
	bool autoScroll        = true;
	bool hidePlatformsPoll = true;   // hide high-frequency server/platforms polling

	// Called from any thread (WS server thread, main thread, etc.)
	static void LogConnection(const char *msg);
	static void LogRequest(const char *fn, const char *paramsPreview);   // paramsPreview truncated
	static void LogResponse(int status, const char *fn, int byteCount, const char *body = nullptr);

	static void Clear();

private:
	enum EntryType { TypeConnection = 0, TypeRequest = 1, TypeResponse = 2 };

	struct LogEntry
	{
		char        timestamp[20]; // "HH:MM:SS.mmm"
		EntryType   type;
		std::string fn;            // endpoint name — used for /platforms filter
		std::string text;
		std::string body;          // response body (optional, only for TypeResponse)
	};

	static std::mutex            sMutex;
	static std::deque<LogEntry>  sEntries;
	static const int             kMaxEntries = 2000;

	static void               PushEntry(EntryType type, const std::string &fn, const std::string &text, const std::string &body = {});
	static std::string        MakeTimestamp();
};

#endif // _CVIEWWSLOG_H_
