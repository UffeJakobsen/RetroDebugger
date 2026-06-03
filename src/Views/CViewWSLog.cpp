#include "CViewWSLog.h"
#include "CByteBuffer.h"
#include "CGuiMain.h"
#include "imgui.h"
#include <ctime>
#include <cstdio>
#include <cstring>
#include <sstream>

// --- Static members ---

std::mutex            CViewWSLog::sMutex;
std::deque<CViewWSLog::LogEntry> CViewWSLog::sEntries;

// ---------------------------------------------------------------------------

CViewWSLog::CViewWSLog(float posX, float posY, float posZ, float sizeX, float sizeY)
: CGuiView("WS Log", posX, posY, posZ, sizeX, sizeY)
{
}

// ---------------------------------------------------------------------------
// Static helpers

std::string CViewWSLog::MakeTimestamp()
{
	using namespace std::chrono;
	auto now   = system_clock::now();
	auto ms    = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
	std::time_t t  = system_clock::to_time_t(now);
	struct tm *lt  = std::localtime(&t);
	char buf[20];
	snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
			 lt->tm_hour, lt->tm_min, lt->tm_sec, (int)ms.count());
	return buf;
}

void CViewWSLog::PushEntry(EntryType type, const std::string &fn, const std::string &text, const std::string &body)
{
	LogEntry e;
	std::string ts = MakeTimestamp();
	strncpy(e.timestamp, ts.c_str(), sizeof(e.timestamp) - 1);
	e.timestamp[sizeof(e.timestamp) - 1] = '\0';
	e.type = type;
	e.fn   = fn;
	e.text = text;
	e.body = body;

	std::lock_guard<std::mutex> lock(sMutex);
	sEntries.push_back(std::move(e));
	if ((int)sEntries.size() > kMaxEntries)
		sEntries.pop_front();
}

void CViewWSLog::LogConnection(const char *msg)
{
	PushEntry(TypeConnection, {}, msg);
}

void CViewWSLog::LogRequest(const char *fn, const char *paramsPreview)
{
	std::string text = fn;
	if (paramsPreview && paramsPreview[0] != '\0')
	{
		text += "  ";
		size_t len = strlen(paramsPreview);
		if (len > 120)
		{
			text.append(paramsPreview, 120);
			text += "...";
		}
		else
		{
			text += paramsPreview;
		}
	}
	PushEntry(TypeRequest, fn, text);
}

void CViewWSLog::LogResponse(int status, const char *fn, int byteCount, const char *body)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "%d  %s  (%d bytes)", status, fn, byteCount);
	std::string bodyStr;
	if (body && body[0] != '\0')
	{
		// Truncate very large bodies
		size_t len = strlen(body);
		if (len > 512)
		{
			bodyStr.assign(body, 512);
			bodyStr += "...";
		}
		else
		{
			bodyStr = body;
		}
	}
	PushEntry(TypeResponse, fn, buf, bodyStr);
}

void CViewWSLog::Clear()
{
	std::lock_guard<std::mutex> lock(sMutex);
	sEntries.clear();
}

// ---------------------------------------------------------------------------
// Render

void CViewWSLog::RenderImGui()
{
	PreRenderImGui();

	// Toolbar
	if (ImGui::Button("Clear"))
		Clear();
	ImGui::SameLine();
	ImGui::Checkbox("Connections", &logConnections);
	ImGui::SameLine();
	ImGui::Checkbox("Requests", &logRequests);
	ImGui::SameLine();
	ImGui::Checkbox("Responses", &logResponses);
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &autoScroll);
	ImGui::SameLine();
	ImGui::Checkbox("Hide /platforms", &hidePlatformsPoll);
	ImGui::SameLine();
	if (!logResponses) ImGui::BeginDisabled();
	ImGui::Checkbox("Body", &logResponseBody);
	if (!logResponses) ImGui::EndDisabled();

	ImGui::Separator();

	// Snapshot entries under lock, then render without lock
	std::vector<LogEntry> snapshot;
	{
		std::lock_guard<std::mutex> lock(sMutex);
		snapshot.assign(sEntries.begin(), sEntries.end());
	}

	ImGui::BeginChild("##wslogscroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	// Pre-filter to avoid iterating hidden types twice
	std::vector<const LogEntry *> visible;
	visible.reserve(snapshot.size());
	for (const auto &e : snapshot)
	{
		if (e.type == TypeConnection && !logConnections) continue;
		if (e.type == TypeRequest    && !logRequests)    continue;
		if (e.type == TypeResponse   && !logResponses)   continue;
		if (hidePlatformsPoll && e.fn.find("server/platforms") != std::string::npos) continue;
		visible.push_back(&e);
	}

	// Use clipper only when body lines are hidden (clipper assumes 1 line per item)
	auto renderEntry = [&](const LogEntry *e)
	{
		switch (e->type)
		{
			case TypeConnection:
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));  // green
				break;
			case TypeRequest:
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));  // white
				break;
			case TypeResponse:
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));  // light blue
				break;
		}
		ImGui::Text("%s  %s", e->timestamp, e->text.c_str());
		if (logResponseBody && e->type == TypeResponse && !e->body.empty())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.65f, 0.8f, 1.0f));  // dimmer blue for body
			ImGui::TextUnformatted(e->body.c_str());
			ImGui::PopStyleColor();
		}
		ImGui::PopStyleColor();
	};

	if (logResponseBody)
	{
		// Body lines mean variable height per item — clipper can't be used
		for (const LogEntry *e : visible)
			renderEntry(e);
	}
	else
	{
		ImGuiListClipper clipper;
		clipper.Begin((int)visible.size());
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
				renderEntry(visible[i]);
		}
		clipper.End();
	}

	if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
		ImGui::SetScrollHereY(1.0f);

	ImGui::EndChild();

	PostRenderImGui();
}

// ---------------------------------------------------------------------------
// Serialization (saves display settings with the layout)

void CViewWSLog::Serialize(CByteBuffer *byteBuffer)
{
	CGuiView::Serialize(byteBuffer);
	byteBuffer->PutBool(logConnections);
	byteBuffer->PutBool(logRequests);
	byteBuffer->PutBool(logResponses);
	byteBuffer->PutBool(logResponseBody);
	byteBuffer->PutBool(autoScroll);
	byteBuffer->PutBool(hidePlatformsPoll);
}

void CViewWSLog::Deserialize(CByteBuffer *byteBuffer)
{
	CGuiView::Deserialize(byteBuffer);
	logConnections     = byteBuffer->GetBool();
	logRequests        = byteBuffer->GetBool();
	logResponses       = byteBuffer->GetBool();
	logResponseBody    = byteBuffer->GetBool();
	autoScroll         = byteBuffer->GetBool();
	hidePlatformsPoll  = byteBuffer->GetBool();
}
