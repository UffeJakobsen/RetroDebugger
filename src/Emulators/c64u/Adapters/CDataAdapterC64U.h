#ifndef _CDATAADAPTERC64U_H_
#define _CDATAADAPTERC64U_H_

#include "CDebugDataAdapter.h"

class CDebugInterfaceC64U;
class C64UMemoryCache;

class CDataAdapterC64U : public CDebugDataAdapter
{
public:
	CDataAdapterC64U(CDebugSymbols *debugSymbols, C64UMemoryCache *memoryCache);

	C64UMemoryCache *memoryCache;

	virtual int AdapterGetDataLength();
	virtual void AdapterReadByte(int pointer, uint8 *value);
	virtual void AdapterWriteByte(int pointer, uint8 value);
	virtual void AdapterReadByte(int pointer, uint8 *value, bool *isAvailable);
	virtual void AdapterWriteByte(int pointer, uint8 value, bool *isAvailable);
	virtual void AdapterReadBlockDirect(uint8 *buffer, int pointerStart, int pointerEnd);
};

#endif
