#include "CDebuggerServerProtocol.h"

using namespace std;
using namespace nlohmann;

namespace DebuggerProtocol
{

RequestContext ParseRequest(const json &requestJson)
{
	RequestContext ctx;

	// Detect protocol version
	if (requestJson.contains("protocolVersion"))
	{
		ctx.protocolVersion = requestJson["protocolVersion"].get<int>();
	}
	else
	{
		ctx.protocolVersion = DEBUGGER_PROTOCOL_V1;
	}

	// v1 correlation
	if (requestJson.contains("token"))
	{
		ctx.token = requestJson["token"].get<string>();
	}

	// v2 correlation
	if (requestJson.contains("requestId"))
	{
		ctx.requestId = requestJson["requestId"].get<string>();
	}

	return ctx;
}

json MakeError(const string &code, const string &message, const json &details)
{
	json err;
	err["code"] = code;
	err["message"] = message;
	if (!details.empty())
	{
		err["details"] = details;
	}
	return err;
}

json MakeResponse(const RequestContext &ctx, int status, const json &result, const json &error)
{
	json resp;

	if (ctx.protocolVersion >= DEBUGGER_PROTOCOL_V2)
	{
		resp["protocolVersion"] = DEBUGGER_PROTOCOL_CURRENT;
		if (!ctx.requestId.empty())
			resp["requestId"] = ctx.requestId;
	}

	resp["status"] = status;

	// v1 compat: echo token
	if (!ctx.token.empty())
		resp["token"] = ctx.token;

	if (!error.is_null())
		resp["error"] = error;
	else if (!result.empty())
		resp["result"] = result;

	return resp;
}

} // namespace DebuggerProtocol
