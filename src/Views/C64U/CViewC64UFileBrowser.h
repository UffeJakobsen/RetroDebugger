#ifndef _CVIEWC64UFILEBROWSER_H_
#define _CVIEWC64UFILEBROWSER_H_

#include "CGuiView.h"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

class CDebugInterfaceC64U;
struct C64UFtpEntry;

class CViewC64UFileBrowser : public CGuiView
{
public:
	CViewC64UFileBrowser(const char *name, float posX, float posY, float posZ,
						 float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface);
	virtual ~CViewC64UFileBrowser();

	virtual void Render();
	virtual void RenderImGui();
	virtual bool DoDropFile(char *filePath);

	void NavigateTo(const std::string &path);
	void Refresh();

	CDebugInterfaceC64U *debugInterface;

private:
	void StartAsyncListDir();
	void StartAsyncDownload(const std::string &remotePath, const std::string &localPath);
	void StartAsyncUpload(const std::string &localPath, const std::string &remoteName);

	std::string GetFileExtension(const std::string &name);
	std::string FormatFileSize(uint64_t size);

	// Current state
	std::string currentPath;
	int selectedIndex;
	char filterBuf[256];
	std::string statusMessage;

	// File list (protected by mutex)
	struct FileEntry
	{
		std::string name;
		bool isDirectory;
		uint64_t size;
	};
	std::mutex entriesMutex;
	std::vector<FileEntry> entries;
	std::vector<FileEntry> filteredEntries;

	// Async operation state
	std::atomic<bool> operationPending;
	std::mutex resultMutex;
	bool hasNewEntries;
	std::vector<FileEntry> pendingEntries;
	std::string pendingStatus;
	bool hasPendingStatus;
};

#endif
