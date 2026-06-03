#include "C64UMediaStatusCache.h"

C64UMediaStatusCache::C64UMediaStatusCache()
{
}

void C64UMediaStatusCache::SetMountedDiskImage(int driveId, const std::string &path)
{
	if (driveId < 0 || driveId >= 4)
		return;
	std::lock_guard<std::mutex> lock(statusMutex);
	mountedDisk[driveId] = path;
}

void C64UMediaStatusCache::ClearMountedDiskImage(int driveId)
{
	if (driveId < 0 || driveId >= 4)
		return;
	std::lock_guard<std::mutex> lock(statusMutex);
	mountedDisk[driveId].clear();
}

std::string C64UMediaStatusCache::GetMountedDiskImage(int driveId) const
{
	if (driveId < 0 || driveId >= 4)
		return "";
	std::lock_guard<std::mutex> lock(statusMutex);
	return mountedDisk[driveId];
}

void C64UMediaStatusCache::SetLoadedPrg(const std::string &path)
{
	std::lock_guard<std::mutex> lock(statusMutex);
	loadedPrg = path;
}

std::string C64UMediaStatusCache::GetLoadedPrg() const
{
	std::lock_guard<std::mutex> lock(statusMutex);
	return loadedPrg;
}

void C64UMediaStatusCache::ClearLoadedPrg()
{
	std::lock_guard<std::mutex> lock(statusMutex);
	loadedPrg.clear();
}

void C64UMediaStatusCache::SetLastCartridge(const std::string &path)
{
	std::lock_guard<std::mutex> lock(statusMutex);
	lastCartridge = path;
}

std::string C64UMediaStatusCache::GetLastCartridge() const
{
	std::lock_guard<std::mutex> lock(statusMutex);
	return lastCartridge;
}
