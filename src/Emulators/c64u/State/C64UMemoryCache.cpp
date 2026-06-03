#include "C64UMemoryCache.h"
#include "C64SettingsStorage.h"
#include "../Transport/C64URestClient.h"

#include <chrono>
#include <cstring>

static uint64_t GetCurrentTimeMillis()
{
	return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

C64UMemoryCache::C64UMemoryCache()
	: currentFrame(0), lastRefreshCount(0), pendingWriteTimestamp(0),
	  pageStaleAfterFrames(120), fixtureMode(false), restClient(nullptr),
	  isShuttingDown(false)
{
	memset(memory, 0, TOTAL_SIZE);
	memset(pageFresh, 0, sizeof(pageFresh));
	memset(pageInFlight, 0, sizeof(pageInFlight));
	memset(pageInFlightSinceFrame, 0, sizeof(pageInFlightSinceFrame));
	memset(pageLastAccessedFrame, 0, sizeof(pageLastAccessedFrame));
	memset(pageLastFetchedFrame, 0, sizeof(pageLastFetchedFrame));
}

C64UMemoryCache::~C64UMemoryCache()
{
}

void C64UMemoryCache::BeginShutdown()
{
	// Lock the mutex so any in-progress access completes, then set the flag.
	// After this returns, all lock-acquiring methods become early-exit no-ops.
	// Caller must sleep briefly (~5ms) before deleting to close the tiny window
	// between a thread's flag check and its mutex lock attempt.
	std::lock_guard<std::mutex> lock(cacheMutex);
	isShuttingDown = true;
}

void C64UMemoryCache::SetRestClient(C64URestClient *client)
{
	restClient = client;
}

uint8_t C64UMemoryCache::ReadByte(int address) const
{
	if (address < 0 || address >= TOTAL_SIZE)
		return 0;
	if (isShuttingDown)
		return 0;

	std::lock_guard<std::mutex> lock(cacheMutex);
	return memory[address];
}

void C64UMemoryCache::WriteByte(int address, uint8_t value)
{
	if (address < 0 || address >= TOTAL_SIZE)
		return;
	if (isShuttingDown)
		return;

	std::lock_guard<std::mutex> lock(cacheMutex);
	memory[address] = value;
	pageFresh[address / PAGE_SIZE] = true;

	// Queue write for coalesced REST flush
	if (!fixtureMode)
	{
		if (pendingWrites.empty())
		{
			pendingWriteTimestamp = GetCurrentTimeMillis();
		}
		pendingWrites.push_back(std::make_pair(address, value));
	}
}

bool C64UMemoryCache::IsPageFresh(int pageIndex) const
{
	if (pageIndex < 0 || pageIndex >= NUM_PAGES)
		return false;
	if (isShuttingDown)
		return false;

	std::lock_guard<std::mutex> lock(cacheMutex);
	return pageFresh[pageIndex];
}

void C64UMemoryCache::ReadBlock(uint8_t *buffer, int startAddress, int endAddress) const
{
	if (buffer == nullptr || startAddress < 0 || endAddress > TOTAL_SIZE || startAddress >= endAddress)
		return;
	if (isShuttingDown)
		return;

	std::lock_guard<std::mutex> lock(cacheMutex);
	memcpy(buffer, memory + startAddress, endAddress - startAddress);
}

void C64UMemoryCache::InvalidateAll()
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	memset(pageFresh, 0, sizeof(pageFresh));
	memset(pageInFlight, 0, sizeof(pageInFlight));
	memset(pageInFlightSinceFrame, 0, sizeof(pageInFlightSinceFrame));
}

void C64UMemoryCache::RefreshPage(int pageIndex)
{
	if (pageIndex < 0 || pageIndex >= NUM_PAGES)
		return;

	if (fixtureMode)
	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		pageFresh[pageIndex] = true;
		return;
	}

	// TODO: when real REST client is available, fetch the page from device
	// restClient->ReadMemoryPage(pageIndex * PAGE_SIZE, PAGE_SIZE, memory + pageIndex * PAGE_SIZE);
	std::lock_guard<std::mutex> lock(cacheMutex);
	pageFresh[pageIndex] = true;
}

void C64UMemoryCache::SchedulePageRefresh(int pageIndex)
{
	if (pageIndex < 0 || pageIndex >= NUM_PAGES)
		return;

	// Rate-limit: just mark as stale, the refresh will happen on next poll cycle
	std::lock_guard<std::mutex> lock(cacheMutex);
	pageFresh[pageIndex] = false;
}

void C64UMemoryCache::SetFixtureMode(bool enabled)
{
	fixtureMode = enabled;
}

void C64UMemoryCache::FillWithTestPattern()
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	for (int i = 0; i < TOTAL_SIZE; i++)
	{
		memory[i] = (uint8_t)(i & 0xff);
	}
	memset(pageFresh, 1, sizeof(pageFresh));
}

void C64UMemoryCache::MarkPageAccessed(int pageIndex)
{
	if (pageIndex < 0 || pageIndex >= NUM_PAGES)
		return;
	if (isShuttingDown)
		return;

	std::lock_guard<std::mutex> lock(cacheMutex);
	pageLastAccessedFrame[pageIndex] = currentFrame;
}

void C64UMemoryCache::SetCurrentFrame(uint32_t frame)
{
	currentFrame = frame;
}

void C64UMemoryCache::ScheduleVisiblePageRefreshes()
{
	if (isShuttingDown)
		return;
	std::lock_guard<std::mutex> lock(cacheMutex);

	int maxPagesPerCycle = c64SettingsC64UMemoryRefreshRate;
	if (maxPagesPerCycle <= 0)
		maxPagesPerCycle = 4;

	// Age out fresh pages that haven't been re-fetched for a while,
	// so they get re-requested on the next cycle (live refresh).
	if (pageStaleAfterFrames > 0)
	{
		for (int i = 0; i < NUM_PAGES; i++)
		{
			if (pageFresh[i] && !pageInFlight[i]
				&& pageLastAccessedFrame[i] + 60 >= currentFrame   // still visible
				&& pageLastFetchedFrame[i] + pageStaleAfterFrames < currentFrame)
			{
				pageFresh[i] = false;
			}
		}
	}

	// Expire in-flight pages that never got a response (after ~90 frames = 1.5s)
	// This handles cases where the REST read timed out or the device was busy
	const uint32_t inFlightTimeoutFrames = 90;
	for (int i = 0; i < NUM_PAGES; i++)
	{
		if (pageInFlight[i] && pageInFlightSinceFrame[i] + inFlightTimeoutFrames < currentFrame)
		{
			pageInFlight[i] = false;
		}
	}

	int refreshed = 0;

	for (int i = 0; i < NUM_PAGES && refreshed < maxPagesPerCycle; i++)
	{
		// Only refresh pages accessed within the last ~60 frames (~1 second at 60fps)
		if (pageLastAccessedFrame[i] + 60 < currentFrame)
			continue;

		// Only refresh stale pages
		if (pageFresh[i])
			continue;

		// Skip pages already in flight to prevent REST queue flooding
		if (pageInFlight[i])
			continue;

		// Skip I/O range ($D000-$DFFF) unless explicitly allowed — CPU PEEK has side effects there
		if (!c64SettingsC64URefreshIORange && i >= 0xD0 && i <= 0xDF)
			continue;

		if (fixtureMode)
		{
			pageFresh[i] = true;
			pageLastFetchedFrame[i] = currentFrame;
		}
		else if (restClient != nullptr)
		{
			pageInFlight[i] = true;
			pageInFlightSinceFrame[i] = currentFrame;
			restClient->ScheduleReadMemory(i * PAGE_SIZE, PAGE_SIZE);
		}

		refreshed++;
	}

	lastRefreshCount = refreshed;
}

void C64UMemoryCache::SnapshotIORegisters()
{
	// Force a one-time REST read of VIC/SID/CIA/CIA2 pages to bootstrap
	// the cache when entering trace mode. The trace only updates these on
	// CPU writes, which may have happened before the trace buffer window.
	// Note: REST reads of I/O pages go through a CPU PEEK so may have minor
	// side effects — this is intentional and a known limitation.
	if (isShuttingDown || fixtureMode || restClient == nullptr)
		return;

	static const int ioPages[] = { 0xD0, 0xD4, 0xDC, 0xDD };

	std::lock_guard<std::mutex> lock(cacheMutex);
	for (int page : ioPages)
	{
		if (!pageInFlight[page])
		{
			pageFresh[page] = false;
			pageInFlight[page] = true;
			pageInFlightSinceFrame[page] = currentFrame;
			restClient->ScheduleReadMemory(page * PAGE_SIZE, PAGE_SIZE);
		}
	}
}

void C64UMemoryCache::FlushPendingWrites()
{
	if (isShuttingDown)
		return;
	std::lock_guard<std::mutex> lock(cacheMutex);

	if (pendingWrites.empty())
		return;

	if (fixtureMode)
	{
		pendingWrites.clear();
		pendingWriteTimestamp = 0;
		return;
	}

	// Only flush if at least 50ms have elapsed since first pending write
	uint64_t now = GetCurrentTimeMillis();
	if (now - pendingWriteTimestamp < 50)
		return;

	if (restClient == nullptr)
	{
		pendingWrites.clear();
		pendingWriteTimestamp = 0;
		return;
	}

	// Group writes by page and send one writemem per page
	// Use a simple approach: collect unique pages and their byte data
	bool pageHasWrites[NUM_PAGES];
	memset(pageHasWrites, 0, sizeof(pageHasWrites));

	// First pass: identify which pages have writes
	for (const auto &write : pendingWrites)
	{
		int page = write.first / PAGE_SIZE;
		pageHasWrites[page] = true;
	}

	// Second pass: for each page with writes, collect contiguous data from cache
	for (int p = 0; p < NUM_PAGES; p++)
	{
		if (!pageHasWrites[p])
			continue;

		// Send the full page worth of cached data for this page
		restClient->ScheduleWriteMemory(p * PAGE_SIZE, memory + p * PAGE_SIZE, PAGE_SIZE);
	}

	pendingWrites.clear();
	pendingWriteTimestamp = 0;
}

void C64UMemoryCache::UpdatePageFromNetwork(int address, const uint8_t *data, int length)
{
	if (data == nullptr || length <= 0 || address < 0)
		return;
	if (isShuttingDown)
		return;

	std::lock_guard<std::mutex> lock(cacheMutex);

	int copyLen = length;
	if (address + copyLen > TOTAL_SIZE)
		copyLen = TOTAL_SIZE - address;
	if (copyLen <= 0)
		return;

	memcpy(memory + address, data, copyLen);

	// Mark affected pages as fresh and clear in-flight
	int startPage = address / PAGE_SIZE;
	int endPage = (address + copyLen - 1) / PAGE_SIZE;
	for (int p = startPage; p <= endPage && p < NUM_PAGES; p++)
	{
		pageFresh[p] = true;
		pageInFlight[p] = false;
		pageLastFetchedFrame[p] = currentFrame;
	}
}

void C64UMemoryCache::UpdateByteFromTrace(int address, uint8_t data)
{
	if (address < 0 || address >= TOTAL_SIZE)
		return;
	if (isShuttingDown)
		return;

	std::lock_guard<std::mutex> lock(cacheMutex);
	memory[address] = data;
	int page = address / PAGE_SIZE;
	pageFresh[page] = true;
	pageInFlight[page] = false;
	pageLastFetchedFrame[page] = currentFrame;
}

int C64UMemoryCache::GetPendingRefreshCount() const
{
	return lastRefreshCount;
}

int C64UMemoryCache::GetPendingWriteCount() const
{
	if (isShuttingDown)
		return 0;
	std::lock_guard<std::mutex> lock(cacheMutex);
	return (int)pendingWrites.size();
}

int C64UMemoryCache::GetDataLength() const
{
	return TOTAL_SIZE;
}
