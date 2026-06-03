#pragma once

#include "CTest.h"
#include "CViewC64.h"
#include "../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../Emulators/c64u/State/C64UMemoryCache.h"
#include "../Emulators/c64u/C64UTestFixture.h"

class CTestC64UMemoryCache : public CTest
{
public:
	virtual const char *GetName() override { return "C64UMemoryCache"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

#ifndef RUN_COMMODORE64
		TestCompleted(true, "Skipped (C64 not enabled)");
		return;
#else
		// Test 1: Basic cache operations
		C64UMemoryCache cache;
		cache.SetFixtureMode(true);

		if (cache.GetDataLength() != 0x10000)
		{
			TestCompleted(false, "Cache data length must be 64KB");
			return;
		}

		// Initially all zero
		uint8_t val = cache.ReadByte(0x0400);
		if (val != 0)
		{
			TestCompleted(false, "Cache must start zeroed");
			return;
		}
		StepCompleted(1, true, "Cache initializes to zero with 64KB size");

		// Test 2: Write and read back
		cache.WriteByte(0x0400, 0x42);
		val = cache.ReadByte(0x0400);
		if (val != 0x42)
		{
			TestCompleted(false, "Write-through must be readable");
			return;
		}
		StepCompleted(2, true, "Write-through and read-back works");

		// Test 3: Page freshness tracking
		cache.InvalidateAll();
		if (cache.IsPageFresh(0x04))
		{
			TestCompleted(false, "Page must be stale after invalidation");
			return;
		}
		cache.RefreshPage(0x04);
		if (!cache.IsPageFresh(0x04))
		{
			TestCompleted(false, "Page must be fresh after refresh");
			return;
		}
		StepCompleted(3, true, "Page freshness tracking works");

		// Test 4: Test pattern fill
		cache.FillWithTestPattern();
		val = cache.ReadByte(0x0042);
		if (val != 0x42)
		{
			TestCompleted(false, "Test pattern fill must set address low byte as value");
			return;
		}
		val = cache.ReadByte(0xFF00);
		if (val != 0x00)
		{
			TestCompleted(false, "Test pattern at 0xFF00 must be 0x00");
			return;
		}
		StepCompleted(4, true, "Test pattern fill works correctly");

		// Test 5: Block read
		uint8_t block[16];
		cache.ReadBlock(block, 0x0040, 0x0050);
		if (block[0] != 0x40 || block[15] != 0x4F)
		{
			TestCompleted(false, "Block read must match individual reads");
			return;
		}
		StepCompleted(5, true, "Block read works correctly");

		// Test 6: Bounds checking
		cache.WriteByte(-1, 0xFF);  // Should not crash
		cache.WriteByte(0x10000, 0xFF);  // Should not crash
		val = cache.ReadByte(-1);
		if (val != 0)
		{
			TestCompleted(false, "Out-of-bounds read must return 0");
			return;
		}
		StepCompleted(6, true, "Bounds checking works");

		// Test 7: Verify C64U backend has memory cache and data adapters
		if (viewC64->debugInterfaceC64U == NULL)
		{
			TestCompleted(false, "C64U backend must be registered");
			return;
		}
		CDebugInterfaceC64U *c64u = (CDebugInterfaceC64U *)viewC64->debugInterfaceC64U;
		if (c64u->GetMemoryCache() == NULL)
		{
			TestCompleted(false, "C64U backend must have a memory cache");
			return;
		}
		if (c64u->dataAdapterC64 == NULL)
		{
			TestCompleted(false, "C64U backend must have a mapped memory data adapter");
			return;
		}
		if (c64u->dataAdapterC64DirectRam == NULL)
		{
			TestCompleted(false, "C64U backend must have a direct RAM best-effort data adapter");
			return;
		}
		StepCompleted(7, true, "C64U backend has memory cache and data adapters");

		TestCompleted(true, "C64U memory cache works correctly");
#endif
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
