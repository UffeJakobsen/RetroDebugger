#include "CDataAdapterC64UVicRam.h"
#include "../CDebugInterfaceC64U.h"
#include "../State/C64UMemoryCache.h"

// C64 chargen ROM: 4KB, extern-declared in c64mem.h (defined in c64chargen.h via VICE)
extern "C" uint8_t mem_chargen_rom[0x1000];

// In VIC banks 0 and 2, the chargen ROM is transparently visible at VIC-relative
// offsets $1000-$1FFF.  In absolute C64 address space that is $1000-$1FFF (bank 0)
// and $9000-$9FFF (bank 2).  The CPU sees RAM at these addresses; the VIC sees ROM.
static inline bool IsChargenRomAddress(int addr)
{
	return (addr >= 0x1000 && addr < 0x2000) || (addr >= 0x9000 && addr < 0xA000);
}

static inline uint8_t ReadChargenRomByte(int addr)
{
	return mem_chargen_rom[addr & 0x0FFF];
}

CDataAdapterC64UVicRam::CDataAdapterC64UVicRam(CDebugInterfaceC64U *debugInterface, CDebugSymbols *debugSymbols, C64UMemoryCache *memoryCache)
	: CDebugDataAdapter("C64UVicRam", debugSymbols), debugInterface(debugInterface), memoryCache(memoryCache)
{
}

int CDataAdapterC64UVicRam::AdapterGetDataLength()
{
	return 0x10000;
}

void CDataAdapterC64UVicRam::AdapterReadByte(int pointer, uint8 *value)
{
	if (pointer >= 0 && pointer < 0x10000)
	{
		if (IsChargenRomAddress(pointer))
		{
			*value = ReadChargenRomByte(pointer);
		}
		else
		{
			memoryCache->MarkPageAccessed(pointer / C64UMemoryCache::PAGE_SIZE);
			*value = memoryCache->ReadByte(pointer);
		}
	}
	else
	{
		*value = 0;
	}
}

void CDataAdapterC64UVicRam::AdapterWriteByte(int pointer, uint8 value)
{
	// Write goes to RAM cache regardless of VIC ROM transparency
	if (pointer >= 0 && pointer < 0x10000)
	{
		memoryCache->WriteByte(pointer, value);
	}
}

void CDataAdapterC64UVicRam::AdapterReadByte(int pointer, uint8 *value, bool *isAvailable)
{
	if (pointer >= 0 && pointer < 0x10000)
	{
		if (IsChargenRomAddress(pointer))
		{
			*value = ReadChargenRomByte(pointer);
			*isAvailable = true;
		}
		else
		{
			memoryCache->MarkPageAccessed(pointer / C64UMemoryCache::PAGE_SIZE);
			*value = memoryCache->ReadByte(pointer);
			*isAvailable = false;
		}
	}
	else
	{
		*value = 0;
		*isAvailable = false;
	}
}

void CDataAdapterC64UVicRam::AdapterWriteByte(int pointer, uint8 value, bool *isAvailable)
{
	*isAvailable = false;
}

void CDataAdapterC64UVicRam::AdapterReadBlockDirect(uint8 *buffer, int pointerStart, int pointerEnd)
{
	if (pointerStart < 0)
		pointerStart = 0;
	if (pointerEnd > 0x10000)
		pointerEnd = 0x10000;

	if (pointerStart >= pointerEnd)
		return;

	// Read RAM from cache first
	memoryCache->ReadBlock(buffer + pointerStart, pointerStart, pointerEnd);

	// Overlay chargen ROM for VIC-transparent address windows
	int romStart1 = (pointerStart < 0x1000) ? 0x1000 : pointerStart;
	int romEnd1   = (pointerEnd   > 0x2000) ? 0x2000 : pointerEnd;
	for (int addr = romStart1; addr < romEnd1; addr++)
		buffer[addr] = ReadChargenRomByte(addr);

	int romStart2 = (pointerStart < 0x9000) ? 0x9000 : pointerStart;
	int romEnd2   = (pointerEnd   > 0xA000) ? 0xA000 : pointerEnd;
	for (int addr = romStart2; addr < romEnd2; addr++)
		buffer[addr] = ReadChargenRomByte(addr);
}
