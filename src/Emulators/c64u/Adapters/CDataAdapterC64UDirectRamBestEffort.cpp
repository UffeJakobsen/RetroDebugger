#include "CDataAdapterC64UDirectRamBestEffort.h"
#include "../CDebugInterfaceC64U.h"
#include "../State/C64UMemoryCache.h"
#include "../State/C64URomBypass.h"

CDataAdapterC64UDirectRamBestEffort::CDataAdapterC64UDirectRamBestEffort(CDebugInterfaceC64U *debugInterface, CDebugSymbols *debugSymbols, C64UMemoryCache *memoryCache)
	: CDebugDataAdapter("C64UDirectRam", debugSymbols), debugInterface(debugInterface), memoryCache(memoryCache)
{
}

int CDataAdapterC64UDirectRamBestEffort::AdapterGetDataLength()
{
	return 0x10000;
}

void CDataAdapterC64UDirectRamBestEffort::AdapterReadByte(int pointer, uint8 *value)
{
	if (pointer >= 0 && pointer < 0x10000)
	{
		memoryCache->MarkPageAccessed(pointer / C64UMemoryCache::PAGE_SIZE);
		*value = memoryCache->ReadByte(pointer);
	}
	else
	{
		*value = 0;
	}
}

void CDataAdapterC64UDirectRamBestEffort::AdapterWriteByte(int pointer, uint8 value)
{
	// Direct RAM write not supported on C64U - best effort: write to cache
	if (pointer >= 0 && pointer < 0x10000)
	{
		memoryCache->WriteByte(pointer, value);
	}
}

void CDataAdapterC64UDirectRamBestEffort::AdapterReadByte(int pointer, uint8 *value, bool *isAvailable)
{
	if (pointer >= 0 && pointer < 0x10000)
	{
		memoryCache->MarkPageAccessed(pointer / C64UMemoryCache::PAGE_SIZE);
		*value = memoryCache->ReadByte(pointer);
		// Direct RAM is not truly available on C64U, but we give best-effort cached data
		*isAvailable = false;
	}
	else
	{
		*value = 0;
		*isAvailable = false;
	}
}

void CDataAdapterC64UDirectRamBestEffort::AdapterWriteByte(int pointer, uint8 value, bool *isAvailable)
{
	*isAvailable = false;
}

void CDataAdapterC64UDirectRamBestEffort::AdapterReadBlockDirect(uint8 *buffer, int pointerStart, int pointerEnd)
{
	if (pointerStart < 0)
		pointerStart = 0;
	if (pointerEnd > 0x10000)
		pointerEnd = 0x10000;

	if (pointerStart >= pointerEnd)
		return;

	int length = pointerEnd - pointerStart;

	// When helper-assisted mode is enabled and the range overlaps ROM,
	// use the ROM bypass to read the underlying RAM data
	if (debugInterface->IsHelperAssisted() &&
		C64URomBypass::OverlapsRom((uint16_t)pointerStart, (uint16_t)length))
	{
		C64URomBypass *bypass = debugInterface->GetRomBypass();
		if (bypass && bypass->SmartRead((uint16_t)pointerStart, (uint16_t)length, buffer))
		{
			return;
		}
		// Fall through to cache on failure
	}

	// Base class contract: data for address N goes into buffer[N].
	// ReadBlock copies into buffer[0..], so offset the destination pointer.
	memoryCache->ReadBlock(buffer + pointerStart, pointerStart, pointerEnd);
}
