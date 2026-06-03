#ifndef _C64ULOGICALSTATECACHE_H_
#define _C64ULOGICALSTATECACHE_H_

#include "C64ULogicalState.h"
#include <mutex>

class C64UMemoryCache;

class C64ULogicalStateCache
{
public:
	C64ULogicalStateCache();

	void SetMemoryCache(C64UMemoryCache *cache);
	void RefreshFromMemory();
	void FillWithTestPattern();

	C64ULogicalState GetState() const;
	C64UVicState GetVicState() const;
	C64UCiaState GetCia1State() const;
	C64UCiaState GetCia2State() const;
	C64USidState GetSidState() const;
	C64UBankState GetBankState() const;

private:
	void DeriveBank();

	mutable std::mutex stateMutex;
	C64ULogicalState state;
	C64UMemoryCache *memoryCache;
};

#endif
