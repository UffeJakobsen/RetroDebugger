#ifndef _CDATAADAPTERC64UVICRAM_H_
#define _CDATAADAPTERC64UVICRAM_H_

#include "CDebugDataAdapter.h"

class C64UMemoryCache;
class CDebugInterfaceC64U;

// Data adapter that presents the C64U memory cache from the VIC's perspective:
// - CPU-visible RAM for most addresses
// - Chargen ROM transparently overlaid at $1000-$1FFF and $9000-$9FFF
//   (VIC banks 0 and 2 ROM-transparent windows)
class CDataAdapterC64UVicRam : public CDebugDataAdapter
{
public:
	CDataAdapterC64UVicRam(CDebugInterfaceC64U *debugInterface, CDebugSymbols *debugSymbols, C64UMemoryCache *memoryCache);

	CDebugInterfaceC64U *debugInterface;
	C64UMemoryCache *memoryCache;

	virtual int AdapterGetDataLength();
	virtual void AdapterReadByte(int pointer, uint8 *value);
	virtual void AdapterWriteByte(int pointer, uint8 value);
	virtual void AdapterReadByte(int pointer, uint8 *value, bool *isAvailable);
	virtual void AdapterWriteByte(int pointer, uint8 value, bool *isAvailable);
	virtual void AdapterReadBlockDirect(uint8 *buffer, int pointerStart, int pointerEnd);
};

#endif
