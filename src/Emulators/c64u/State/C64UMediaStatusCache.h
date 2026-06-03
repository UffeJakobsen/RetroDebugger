#ifndef _C64UMEDIASTATUSCACHE_H_
#define _C64UMEDIASTATUSCACHE_H_

#include <mutex>
#include <string>

class C64UMediaStatusCache
{
public:
	C64UMediaStatusCache();

	void SetMountedDiskImage(int driveId, const std::string &path);
	void ClearMountedDiskImage(int driveId);
	std::string GetMountedDiskImage(int driveId) const;

	void SetLoadedPrg(const std::string &path);
	std::string GetLoadedPrg() const;
	void ClearLoadedPrg();

	void SetLastCartridge(const std::string &path);
	std::string GetLastCartridge() const;

private:
	mutable std::mutex statusMutex;
	std::string mountedDisk[4];
	std::string loadedPrg;
	std::string lastCartridge;
};

#endif
