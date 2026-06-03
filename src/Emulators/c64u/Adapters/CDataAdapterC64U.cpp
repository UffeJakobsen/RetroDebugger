#include "CDataAdapterC64U.h"
#include "../State/C64UMemoryCache.h"

CDataAdapterC64U::CDataAdapterC64U(CDebugSymbols *debugSymbols, C64UMemoryCache *memoryCache)
	: CDebugDataAdapter("C64U", debugSymbols), memoryCache(memoryCache)
{
}

int CDataAdapterC64U::AdapterGetDataLength()
{
	return 0x10000;
}

void CDataAdapterC64U::AdapterReadByte(int pointer, uint8 *value)
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

void CDataAdapterC64U::AdapterWriteByte(int pointer, uint8 value)
{
	if (pointer >= 0 && pointer < 0x10000)
	{
		memoryCache->WriteByte(pointer, value);
	}
}

void CDataAdapterC64U::AdapterReadByte(int pointer, uint8 *value, bool *isAvailable)
{
	if (pointer >= 0 && pointer < 0x10000)
	{
		memoryCache->MarkPageAccessed(pointer / C64UMemoryCache::PAGE_SIZE);
		*value = memoryCache->ReadByte(pointer);
		*isAvailable = memoryCache->IsPageFresh(pointer / C64UMemoryCache::PAGE_SIZE);
	}
	else
	{
		*value = 0;
		*isAvailable = false;
	}
}

void CDataAdapterC64U::AdapterWriteByte(int pointer, uint8 value, bool *isAvailable)
{
	if (pointer >= 0 && pointer < 0x10000)
	{
		memoryCache->WriteByte(pointer, value);
		*isAvailable = true;
	}
	else
	{
		*isAvailable = false;
	}
}

void CDataAdapterC64U::AdapterReadBlockDirect(uint8 *buffer, int pointerStart, int pointerEnd)
{
	if (pointerStart < 0)
		pointerStart = 0;
	if (pointerEnd > 0x10000)
		pointerEnd = 0x10000;

	if (pointerStart < pointerEnd)
	{
		// Base class contract: data for address N goes into buffer[N].
		// ReadBlock copies into buffer[0..], so offset the destination pointer.
		memoryCache->ReadBlock(buffer + pointerStart, pointerStart, pointerEnd);
	}
}
