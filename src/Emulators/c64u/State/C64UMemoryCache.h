#ifndef _C64UMEMORYCACHE_H_
#define _C64UMEMORYCACHE_H_

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>
#include <utility>

class C64URestClient;

class C64UMemoryCache
{
public:
	C64UMemoryCache();
	~C64UMemoryCache();

	void SetRestClient(C64URestClient *client);

	uint8_t ReadByte(int address) const;
	void WriteByte(int address, uint8_t value);
	bool IsPageFresh(int pageIndex) const;

	void ReadBlock(uint8_t *buffer, int startAddress, int endAddress) const;
	void InvalidateAll();
	void RefreshPage(int pageIndex);
	void SchedulePageRefresh(int pageIndex);

	// Call before deleting the cache to prevent use-after-free from background threads.
	// Locks the cache mutex (drains any in-progress access), sets a shutdown flag,
	// then returns. After this, all public methods that acquire the mutex become no-ops.
	void BeginShutdown();

	void SetFixtureMode(bool enabled);
	void FillWithTestPattern();

	int GetDataLength() const;

	// Page access tracking for demand-driven refresh
	void MarkPageAccessed(int pageIndex);
	void SetCurrentFrame(uint32_t frame);
	void ScheduleVisiblePageRefreshes();
	void SnapshotIORegisters();
	void FlushPendingWrites();
	void UpdatePageFromNetwork(int address, const uint8_t *data, int length);
	void UpdateByteFromTrace(int address, uint8_t data);
	int GetPendingRefreshCount() const;
	int GetPendingWriteCount() const;

	static const int PAGE_SIZE = 256;
	static const int NUM_PAGES = 256;
	static const int TOTAL_SIZE = 0x10000;

	// How many frames before a fetched page goes stale and is re-fetched
	// (default ~2s at 60fps = 120 frames)
	uint32_t pageStaleAfterFrames;

private:
	std::atomic<bool> isShuttingDown;
	uint8_t memory[TOTAL_SIZE];
	bool pageFresh[NUM_PAGES];
	bool pageInFlight[NUM_PAGES];
	uint32_t pageInFlightSinceFrame[NUM_PAGES];  // frame when in-flight was set (for timeout)
	uint32_t pageLastAccessedFrame[NUM_PAGES];
	uint32_t pageLastFetchedFrame[NUM_PAGES];
	uint32_t currentFrame;
	int lastRefreshCount;
	std::vector<std::pair<int, uint8_t>> pendingWrites;
	uint64_t pendingWriteTimestamp;
	bool fixtureMode;
	mutable std::mutex cacheMutex;
	C64URestClient *restClient;
};

#endif
