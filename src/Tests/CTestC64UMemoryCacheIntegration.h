#pragma once

#include "CTest.h"
#include "../Emulators/c64u/State/C64UMemoryCache.h"
#include "C64SettingsStorage.h"

class CTestC64UMemoryCacheIntegration : public CTest
{
public:
	virtual const char *GetName() override { return "C64UMemoryCacheIntegration"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

#ifndef RUN_COMMODORE64
		TestCompleted(true, "Skipped (C64 not enabled)");
		return;
#else
		// Save and override the refresh rate setting for deterministic testing
		int savedRefreshRate = c64SettingsC64UMemoryRefreshRate;

		// Test 1: Page access tracking and visible page refresh
		{
			C64UMemoryCache cache;
			cache.SetFixtureMode(true);
			cache.FillWithTestPattern();

			// Set current frame to 100 so we have headroom
			cache.SetCurrentFrame(100);

			// Mark page 4 accessed, then invalidate it so it's stale
			cache.MarkPageAccessed(4);
			cache.InvalidateAll();

			// Schedule refresh — page 4 was accessed and is stale, should be refreshed
			c64SettingsC64UMemoryRefreshRate = 4;
			cache.ScheduleVisiblePageRefreshes();

			if (cache.GetPendingRefreshCount() != 1)
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				TestCompleted(false, "Expected 1 page refresh for single accessed stale page");
				return;
			}
			// In fixture mode, the page should now be fresh
			if (!cache.IsPageFresh(4))
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				TestCompleted(false, "Page 4 must be fresh after fixture-mode refresh");
				return;
			}
		}
		StepCompleted(1, true, "Page access tracking triggers refresh for accessed stale page");

		// Test 2: No refresh for unaccessed pages
		{
			C64UMemoryCache cache;
			cache.SetFixtureMode(true);
			cache.FillWithTestPattern();

			cache.SetCurrentFrame(100);
			// Do NOT mark any pages accessed
			cache.InvalidateAll();

			c64SettingsC64UMemoryRefreshRate = 4;
			cache.ScheduleVisiblePageRefreshes();

			if (cache.GetPendingRefreshCount() != 0)
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				TestCompleted(false, "Expected 0 refreshes when no pages were accessed");
				return;
			}
		}
		StepCompleted(2, true, "No refresh for pages that were never accessed");

		// Test 3: Rate limiting — mark 20 pages, only 4 should refresh per cycle
		{
			C64UMemoryCache cache;
			cache.SetFixtureMode(true);
			cache.FillWithTestPattern();

			cache.SetCurrentFrame(100);

			// Mark 20 pages as accessed
			for (int i = 0; i < 20; i++)
			{
				cache.MarkPageAccessed(i);
			}
			// Invalidate all to make them stale
			cache.InvalidateAll();

			c64SettingsC64UMemoryRefreshRate = 4;
			cache.ScheduleVisiblePageRefreshes();

			if (cache.GetPendingRefreshCount() != 4)
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				char msg[128];
				snprintf(msg, sizeof(msg), "Expected 4 refreshes (rate limit), got %d", cache.GetPendingRefreshCount());
				TestCompleted(false, msg);
				return;
			}

			// Verify first 4 pages are now fresh, page 4 still stale
			for (int i = 0; i < 4; i++)
			{
				if (!cache.IsPageFresh(i))
				{
					c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
					TestCompleted(false, "First 4 pages should be fresh after rate-limited refresh");
					return;
				}
			}
			if (cache.IsPageFresh(4))
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				TestCompleted(false, "Page 4 should still be stale (rate limit reached)");
				return;
			}
		}
		StepCompleted(3, true, "Rate limiting caps refreshes per cycle");

		// Test 4: WriteByte queues pending writes (non-fixture mode)
		{
			C64UMemoryCache cache;
			// NOT fixture mode, so writes get queued
			cache.SetFixtureMode(false);

			cache.WriteByte(0x0400, 0xAA);
			cache.WriteByte(0x0401, 0xBB);
			cache.WriteByte(0x0402, 0xCC);

			if (cache.GetPendingWriteCount() != 3)
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				char msg[128];
				snprintf(msg, sizeof(msg), "Expected 3 pending writes, got %d", cache.GetPendingWriteCount());
				TestCompleted(false, msg);
				return;
			}

			// Values should still be readable from cache
			if (cache.ReadByte(0x0400) != 0xAA || cache.ReadByte(0x0401) != 0xBB || cache.ReadByte(0x0402) != 0xCC)
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				TestCompleted(false, "Written values must be readable from cache");
				return;
			}
		}
		StepCompleted(4, true, "WriteByte queues pending writes in non-fixture mode");

		// Test 5: WriteByte does NOT queue in fixture mode
		{
			C64UMemoryCache cache;
			cache.SetFixtureMode(true);

			cache.WriteByte(0x0400, 0xAA);
			cache.WriteByte(0x0401, 0xBB);

			if (cache.GetPendingWriteCount() != 0)
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				TestCompleted(false, "Fixture mode should not queue pending writes");
				return;
			}
		}
		StepCompleted(5, true, "Fixture mode does not queue writes");

		// Test 6: Pages that haven't been accessed recently stop being refreshed
		{
			C64UMemoryCache cache;
			cache.SetFixtureMode(true);
			cache.FillWithTestPattern();

			// Access page 10 at frame 100
			cache.SetCurrentFrame(100);
			cache.MarkPageAccessed(10);
			cache.InvalidateAll();

			// Advance to frame 161 (100 + 61 = past the 60 frame window)
			cache.SetCurrentFrame(161);

			c64SettingsC64UMemoryRefreshRate = 256;
			cache.ScheduleVisiblePageRefreshes();

			if (cache.GetPendingRefreshCount() != 0)
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				char msg[128];
				snprintf(msg, sizeof(msg), "Expected 0 refreshes for expired page, got %d", cache.GetPendingRefreshCount());
				TestCompleted(false, msg);
				return;
			}

			if (cache.IsPageFresh(10))
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				TestCompleted(false, "Page 10 should remain stale (access expired)");
				return;
			}
		}
		StepCompleted(6, true, "Unaccessed pages stop being refreshed after frame window expires");

		// Test 7: InvalidateAll + ScheduleVisiblePageRefreshes refreshes accessed pages
		{
			C64UMemoryCache cache;
			cache.SetFixtureMode(true);
			cache.FillWithTestPattern();

			cache.SetCurrentFrame(200);

			// Mark pages 5, 10, 15 as accessed
			cache.MarkPageAccessed(5);
			cache.MarkPageAccessed(10);
			cache.MarkPageAccessed(15);

			// Invalidate all
			cache.InvalidateAll();

			c64SettingsC64UMemoryRefreshRate = 256;  // high limit to not cap
			cache.ScheduleVisiblePageRefreshes();

			if (cache.GetPendingRefreshCount() != 3)
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				char msg[128];
				snprintf(msg, sizeof(msg), "Expected 3 refreshes for 3 accessed pages, got %d", cache.GetPendingRefreshCount());
				TestCompleted(false, msg);
				return;
			}

			if (!cache.IsPageFresh(5) || !cache.IsPageFresh(10) || !cache.IsPageFresh(15))
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				TestCompleted(false, "All 3 accessed pages should be fresh after refresh");
				return;
			}

			// Non-accessed page should remain stale
			if (cache.IsPageFresh(20))
			{
				c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
				TestCompleted(false, "Non-accessed page 20 should remain stale");
				return;
			}
		}
		StepCompleted(7, true, "InvalidateAll + ScheduleVisiblePageRefreshes refreshes accessed pages");

		c64SettingsC64UMemoryRefreshRate = savedRefreshRate;
		TestCompleted(true, "C64U memory cache integration works correctly");
#endif
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
