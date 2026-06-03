#ifndef _CDATAADAPTERC64UDIRECTRAMBESTEFFORT_H_
#define _CDATAADAPTERC64UDIRECTRAMBESTEFFORT_H_

#include "CDebugDataAdapter.h"

class C64UMemoryCache;
class CDebugInterfaceC64U;

class CDataAdapterC64UDirectRamBestEffort : public CDebugDataAdapter
{
public:
	CDataAdapterC64UDirectRamBestEffort(CDebugInterfaceC64U *debugInterface, CDebugSymbols *debugSymbols, C64UMemoryCache *memoryCache);

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
