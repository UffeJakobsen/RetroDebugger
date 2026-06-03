#include "CDebuggerServerApiNes.h"
#include "CDebugInterfaceNes.h"
#include "CDebuggerServer.h"

using namespace std;
using namespace nlohmann;

CDebuggerServerApiNes::CDebuggerServerApiNes(CDebugInterface *debugInterface)
: CDebuggerServerApi(debugInterface)
{
	this->debugInterfaceNes = (CDebugInterfaceNes *)debugInterface;
}

void CDebuggerServerApiNes::RegisterEndpoints(CDebuggerServer *server)
{
	// Register generic cross-platform endpoints first
	CDebuggerServerApi::RegisterEndpoints(server);

	char *buf = SYS_GetCharBuf();

	// NES machine state
	sprintf(buf, "%s/machine/state", debugInterface->GetPlatformNameEndpointString());
	server->AddEndpointFunction(buf, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
	{
		json result;
		{
			CDebugInterfaceMutexGuard lock(debugInterfaceNes);
			result["platform"] = "nes";
			result["isRunning"] = debugInterfaceNes->isRunning;
		}
		return server->PrepareResult(HTTP_OK, token, result, NULL, 0);
	});

	const char *plat = debugInterface->GetPlatformNameEndpointString();

	// PPU register read (bulk, like Atari ANTIC pattern)
	{
		sprintf(buf, "%s/ppu/read", plat);
		EndpointDescriptor desc;
		desc.fn = buf; desc.platform = plat; desc.category = "chips";
		desc.description = "Read NES PPU registers ($2000-$2007). Params: registers (array of ints)";
		server->AddEndpointFunction(desc, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
		{
			json result;
			json regs;
			{
				CDebugInterfaceMutexGuard lock(debugInterfaceNes);
				for (const auto &reg : params["registers"])
				{
					int addr = reg.get<int>();
					regs[to_string(addr)] = debugInterfaceNes->GetPpuRegister(addr);
				}
			}
			result["registers"] = regs;
			return server->PrepareResult(HTTP_OK, token, result, NULL, 0);
		});
	}

	// PPU clocks (scanline position)
	{
		sprintf(buf, "%s/ppu/clocks", plat);
		EndpointDescriptor desc;
		desc.fn = buf; desc.platform = plat; desc.category = "chips";
		desc.description = "Read PPU clock counters (hClock, vClock, cycle)";
		server->AddEndpointFunction(desc, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
		{
			u32 hClock, vClock, cycle;
			{
				CDebugInterfaceMutexGuard lock(debugInterfaceNes);
				debugInterfaceNes->GetPpuClocks(&hClock, &vClock, &cycle);
			}
			json result;
			result["hClock"] = hClock;
			result["vClock"] = vClock;
			result["cycle"] = cycle;
			return server->PrepareResult(HTTP_OK, token, result, NULL, 0);
		});
	}

	// PPU nametable memory read
	{
		sprintf(buf, "%s/ppu/nametable/readBlock", plat);
		EndpointDescriptor desc;
		desc.fn = buf; desc.platform = plat; desc.category = "chips";
		desc.description = "Read binary block from PPU nametable memory. Params: address, size";
		desc.supportsBinaryOutput = true;
		server->AddEndpointFunction(desc, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
		{
			int address = params.at("address").get<int>();
			int size = params.at("size").get<int>();
			unsigned char *buf = new unsigned char[size];
			{
				CDebugInterfaceMutexGuard lock(debugInterfaceNes);
				debugInterfaceNes->dataAdapterPpuNmt->AdapterReadBlockDirect(buf, address, address + size - 1);
			}
			auto *result = server->PrepareResult(HTTP_OK, token, json(), buf, size);
			delete[] buf;
			return result;
		});
	}

	// PPU nametable memory write
	{
		sprintf(buf, "%s/ppu/nametable/writeBlock", plat);
		EndpointDescriptor desc;
		desc.fn = buf; desc.platform = plat; desc.category = "chips";
		desc.description = "Write binary block to PPU nametable memory. Params: address + binary data";
		desc.supportsBinaryInput = true;
		server->AddEndpointFunction(desc, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
		{
			int address = params.at("address").get<int>();
			{
				CDebugInterfaceMutexGuard lock(debugInterfaceNes);
				for (int i = 0; i < binaryDataSize; i++)
				{
					debugInterfaceNes->dataAdapterPpuNmt->AdapterWriteByte(address + i, binaryData[i]);
				}
			}
			return server->PrepareResult(HTTP_OK, token, json(), NULL, 0);
		});
	}

	// APU register read (bulk)
	{
		sprintf(buf, "%s/apu/read", plat);
		EndpointDescriptor desc;
		desc.fn = buf; desc.platform = plat; desc.category = "chips";
		desc.description = "Read NES APU registers ($4000-$4017). Params: registers (array of ints)";
		server->AddEndpointFunction(desc, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
		{
			json result;
			json regs;
			{
				CDebugInterfaceMutexGuard lock(debugInterfaceNes);
				for (const auto &reg : params["registers"])
				{
					int addr = reg.get<int>();
					regs[to_string(addr)] = debugInterfaceNes->GetApuRegister(addr);
				}
			}
			result["registers"] = regs;
			return server->PrepareResult(HTTP_OK, token, result, NULL, 0);
		});
	}

	// APU channel mute
	{
		sprintf(buf, "%s/apu/mute", plat);
		EndpointDescriptor desc;
		desc.fn = buf; desc.platform = plat; desc.category = "chips";
		desc.description = "Mute/unmute individual APU channels. Params: square1, square2, triangle, noise, dmc, ext (booleans)";
		server->AddEndpointFunction(desc, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
		{
			bool sq1 = params.value("square1", false);
			bool sq2 = params.value("square2", false);
			bool tri = params.value("triangle", false);
			bool noise = params.value("noise", false);
			bool dmc = params.value("dmc", false);
			bool ext = params.value("ext", false);
			debugInterfaceNes->SetApuMuteChannels(0, sq1, sq2, tri, noise, dmc, ext);
			return server->PrepareResult(HTTP_OK, token, json(), NULL, 0);
		});
	}

	// Cart insert
	{
		sprintf(buf, "%s/media/cart/insert", plat);
		EndpointDescriptor desc;
		desc.fn = buf; desc.platform = plat; desc.category = "media";
		desc.description = "Insert NES cartridge ROM file. Params: path";
		server->AddEndpointFunction(desc, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
		{
			string path = params.at("path").get<string>();
			bool success = debugInterfaceNes->InsertCartridge((char *)path.c_str());
			if (success)
				return server->PrepareResult(HTTP_OK, token, json(), NULL, 0);
			return server->PrepareResult(HTTP_INTERNAL_SERVER_ERROR, token, json(), NULL, 0);
		});
	}

	// FDS insert disk
	{
		sprintf(buf, "%s/fds/insert", plat);
		EndpointDescriptor desc;
		desc.fn = buf; desc.platform = plat; desc.category = "media";
		desc.description = "Insert FDS disk. Params: disk (int), side (int)";
		server->AddEndpointFunction(desc, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
		{
			int disk = params.value("disk", 0);
			int side = params.value("side", 0);
			bool success = debugInterfaceNes->FdsInsertDisk(disk, side);
			if (success)
				return server->PrepareResult(HTTP_OK, token, json(), NULL, 0);
			return server->PrepareResult(HTTP_INTERNAL_SERVER_ERROR, token, json(), NULL, 0);
		});
	}

	// FDS eject disk
	{
		sprintf(buf, "%s/fds/eject", plat);
		EndpointDescriptor desc;
		desc.fn = buf; desc.platform = plat; desc.category = "media";
		desc.description = "Eject current FDS disk";
		server->AddEndpointFunction(desc, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
		{
			bool success = debugInterfaceNes->FdsEjectDisk();
			if (success)
				return server->PrepareResult(HTTP_OK, token, json(), NULL, 0);
			return server->PrepareResult(HTTP_INTERNAL_SERVER_ERROR, token, json(), NULL, 0);
		});
	}

	// FDS change side
	{
		sprintf(buf, "%s/fds/changeSide", plat);
		EndpointDescriptor desc;
		desc.fn = buf; desc.platform = plat; desc.category = "media";
		desc.description = "Flip FDS disk to the other side";
		server->AddEndpointFunction(desc, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
		{
			bool success = debugInterfaceNes->FdsChangeSide();
			if (success)
				return server->PrepareResult(HTTP_OK, token, json(), NULL, 0);
			return server->PrepareResult(HTTP_INTERNAL_SERVER_ERROR, token, json(), NULL, 0);
		});
	}

	// FDS status
	{
		sprintf(buf, "%s/fds/status", plat);
		EndpointDescriptor desc;
		desc.fn = buf; desc.platform = plat; desc.category = "media";
		desc.description = "Get FDS status: isFDS, hasBIOS, numDisks, currentDisk, currentSide, diskInserted";
		server->AddEndpointFunction(desc, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
		{
			json result;
			{
				CDebugInterfaceMutexGuard lock(debugInterfaceNes);
				result["isFDS"] = debugInterfaceNes->IsFDS();
				result["hasBIOS"] = debugInterfaceNes->FdsHasBIOS();
				result["numDisks"] = debugInterfaceNes->FdsGetNumDisks();
				result["currentDisk"] = debugInterfaceNes->FdsGetCurrentDisk();
				result["currentSide"] = debugInterfaceNes->FdsGetCurrentDiskSide();
				result["diskInserted"] = debugInterfaceNes->FdsIsAnyDiskInserted();
			}
			return server->PrepareResult(HTTP_OK, token, result, NULL, 0);
		});
	}

	// FDS set BIOS
	{
		sprintf(buf, "%s/fds/setBIOS", plat);
		EndpointDescriptor desc;
		desc.fn = buf; desc.platform = plat; desc.category = "media";
		desc.description = "Load FDS BIOS ROM file. Params: path";
		server->AddEndpointFunction(desc, [this, server](const string token, json params, unsigned char *binaryData, int binaryDataSize) -> vector<char>*
		{
			string path = params.at("path").get<string>();
			bool success = debugInterfaceNes->FdsSetBIOS(path.c_str());
			if (success)
				return server->PrepareResult(HTTP_OK, token, json(), NULL, 0);
			return server->PrepareResult(HTTP_INTERNAL_SERVER_ERROR, token, json(), NULL, 0);
		});
	}

	SYS_ReleaseCharBuf(buf);
}
