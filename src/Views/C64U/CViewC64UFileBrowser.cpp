#include "CViewC64UFileBrowser.h"
#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/Transport/C64UFtpClient.h"
#include "../../Emulators/c64u/Transport/C64URestClient.h"
#include "imgui.h"
#include "DBG_Log.h"
#include <algorithm>
#include <thread>

CViewC64UFileBrowser::CViewC64UFileBrowser(const char *name, float posX, float posY, float posZ,
										   float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
	this->selectedIndex = -1;
	this->operationPending = false;
	this->hasNewEntries = false;
	this->hasPendingStatus = false;
	memset(filterBuf, 0, sizeof(filterBuf));
	currentPath = "/";
}

CViewC64UFileBrowser::~CViewC64UFileBrowser()
{
}

void CViewC64UFileBrowser::Render()
{
}

void CViewC64UFileBrowser::RenderImGui()
{
	PreRenderImGui();

	// Pick up async results
	{
		std::lock_guard<std::mutex> lock(resultMutex);
		if (hasNewEntries)
		{
			std::lock_guard<std::mutex> eLock(entriesMutex);
			entries = std::move(pendingEntries);
			hasNewEntries = false;
			selectedIndex = -1;
		}
		if (hasPendingStatus)
		{
			statusMessage = pendingStatus;
			hasPendingStatus = false;
		}
	}

	// Apply filter
	filteredEntries.clear();
	std::string filter(filterBuf);
	for (auto &e : entries)
	{
		if (filter.empty() || e.name.find(filter) != std::string::npos)
			filteredEntries.push_back(e);
	}

	// Path bar
	ImGui::Text("Path: %s", currentPath.c_str());
	ImGui::SameLine();
	bool isLoading = operationPending.load();
	if (isLoading) ImGui::BeginDisabled();
	if (ImGui::Button("Refresh")) Refresh();
	if (isLoading) ImGui::EndDisabled();

	// Filter
	ImGui::SetNextItemWidth(200);
	ImGui::InputText("Filter", filterBuf, sizeof(filterBuf));

	ImGui::Separator();

	// File list
	float listHeight = ImGui::GetContentRegionAvail().y - 60; // leave room for buttons + status
	if (ImGui::BeginChild("FileList", ImVec2(0, listHeight), true))
	{
		// Parent directory entry
		if (currentPath != "/")
		{
			if (ImGui::Selectable("[..] (parent directory)", selectedIndex == -2,
				ImGuiSelectableFlags_AllowDoubleClick))
			{
				selectedIndex = -2;
				if (ImGui::IsMouseDoubleClicked(0))
					NavigateTo("..");
			}
		}

		for (int i = 0; i < (int)filteredEntries.size(); i++)
		{
			auto &entry = filteredEntries[i];
			char label[512];
			if (entry.isDirectory)
				snprintf(label, sizeof(label), "[D] %s/", entry.name.c_str());
			else
				snprintf(label, sizeof(label), "[F] %s  %s", entry.name.c_str(), FormatFileSize(entry.size).c_str());

			if (ImGui::Selectable(label, selectedIndex == i,
				ImGuiSelectableFlags_AllowDoubleClick))
			{
				selectedIndex = i;
				if (ImGui::IsMouseDoubleClicked(0) && entry.isDirectory)
					NavigateTo(entry.name);
			}
		}
	}
	ImGui::EndChild();

	// Action buttons
	bool hasSelection = selectedIndex >= 0 && selectedIndex < (int)filteredEntries.size();
	std::string selExt;
	std::string selName;
	if (hasSelection)
	{
		selName = filteredEntries[selectedIndex].name;
		selExt = GetFileExtension(selName);
	}

	if (isLoading) ImGui::BeginDisabled();

	if (hasSelection && !filteredEntries[selectedIndex].isDirectory)
	{
		if ((selExt == "prg" || selExt == "PRG") && ImGui::Button("Run PRG"))
		{
			std::string remotePath = currentPath;
			if (remotePath.back() != '/') remotePath += '/';
			remotePath += selName;
			// Use REST API to run PRG from remote path
			debugInterface->LoadExecutable((char *)remotePath.c_str());
			statusMessage = "Running: " + selName;
		}
		ImGui::SameLine();

		if ((selExt == "crt" || selExt == "CRT") && ImGui::Button("Run CRT"))
		{
			std::string remotePath = currentPath;
			if (remotePath.back() != '/') remotePath += '/';
			remotePath += selName;
			debugInterface->ScheduleRunCrt(remotePath);
			statusMessage = "Running CRT: " + selName;
		}

		bool isDisk = (selExt == "d64" || selExt == "D64" || selExt == "d71" || selExt == "D71" ||
					   selExt == "d81" || selExt == "D81" || selExt == "g64" || selExt == "G64");
		if (isDisk)
		{
			ImGui::SameLine();
			if (ImGui::Button("Mount A"))
			{
				std::string remotePath = currentPath;
				if (remotePath.back() != '/') remotePath += '/';
				remotePath += selName;
				debugInterface->MountDisk((char *)remotePath.c_str(), 0, false);
				statusMessage = "Mounted: " + selName;
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Download"))
		{
			std::string remotePath = currentPath;
			if (remotePath.back() != '/') remotePath += '/';
			remotePath += selName;
			// Download to ~/Desktop
			std::string localPath = std::string(getenv("HOME") ? getenv("HOME") : "/tmp") + "/Desktop/" + selName;
			StartAsyncDownload(remotePath, localPath);
		}
	}

	if (ImGui::Button("Upload File"))
	{
		// TODO: open native file dialog, then StartAsyncUpload
		statusMessage = "Upload: use File > Load PRG for now";
	}

	if (isLoading) ImGui::EndDisabled();

	// Status bar
	ImGui::Separator();
	if (isLoading)
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "Loading...");
	else if (!statusMessage.empty())
		ImGui::Text("%s", statusMessage.c_str());

	PostRenderImGui();
}

void CViewC64UFileBrowser::NavigateTo(const std::string &path)
{
	if (operationPending.load()) return;

	if (path == "..")
	{
		// Go up one level
		size_t pos = currentPath.rfind('/', currentPath.size() - 2);
		if (pos != std::string::npos)
			currentPath = currentPath.substr(0, pos + 1);
		else
			currentPath = "/";
	}
	else
	{
		if (currentPath.back() != '/') currentPath += '/';
		currentPath += path + "/";
	}

	StartAsyncListDir();
}

void CViewC64UFileBrowser::Refresh()
{
	if (operationPending.load()) return;
	StartAsyncListDir();
}

void CViewC64UFileBrowser::StartAsyncListDir()
{
	operationPending = true;
	std::string path = currentPath;
	CDebugInterfaceC64U *di = debugInterface;

	std::thread([this, path, di]() {
		C64UFtpClient *ftp = di->GetFtpClient();
		if (!ftp || !ftp->IsConnected())
		{
			std::lock_guard<std::mutex> lock(resultMutex);
			pendingStatus = "FTP not connected";
			hasPendingStatus = true;
			operationPending = false;
			return;
		}

		ftp->ChangeDirectory(path);
		auto ftpEntries = ftp->ListDirectory();

		std::vector<FileEntry> newEntries;
		// Sort: directories first, then files, alphabetically
		std::vector<FileEntry> dirs, files;
		for (auto &e : ftpEntries)
		{
			FileEntry fe;
			fe.name = e.name;
			fe.isDirectory = e.isDirectory;
			fe.size = e.size;
			if (e.isDirectory)
				dirs.push_back(fe);
			else
				files.push_back(fe);
		}
		std::sort(dirs.begin(), dirs.end(), [](const FileEntry &a, const FileEntry &b) { return a.name < b.name; });
		std::sort(files.begin(), files.end(), [](const FileEntry &a, const FileEntry &b) { return a.name < b.name; });
		newEntries.insert(newEntries.end(), dirs.begin(), dirs.end());
		newEntries.insert(newEntries.end(), files.begin(), files.end());

		std::lock_guard<std::mutex> lock(resultMutex);
		pendingEntries = std::move(newEntries);
		hasNewEntries = true;
		pendingStatus = std::to_string(ftpEntries.size()) + " items";
		hasPendingStatus = true;
		operationPending = false;
	}).detach();
}

void CViewC64UFileBrowser::StartAsyncDownload(const std::string &remotePath, const std::string &localPath)
{
	operationPending = true;
	CDebugInterfaceC64U *di = debugInterface;

	std::thread([this, remotePath, localPath, di]() {
		C64UFtpClient *ftp = di->GetFtpClient();
		if (!ftp || !ftp->IsConnected())
		{
			std::lock_guard<std::mutex> lock(resultMutex);
			pendingStatus = "FTP not connected";
			hasPendingStatus = true;
			operationPending = false;
			return;
		}

		std::vector<uint8_t> data;
		bool ok = ftp->DownloadFile(remotePath, data);

		std::string status;
		if (ok)
		{
			FILE *f = fopen(localPath.c_str(), "wb");
			if (f)
			{
				fwrite(data.data(), 1, data.size(), f);
				fclose(f);
				status = "Downloaded: " + localPath;
			}
			else
			{
				status = "Failed to save: " + localPath;
			}
		}
		else
		{
			status = "Download failed: " + remotePath;
		}

		std::lock_guard<std::mutex> lock(resultMutex);
		pendingStatus = status;
		hasPendingStatus = true;
		operationPending = false;
	}).detach();
}

void CViewC64UFileBrowser::StartAsyncUpload(const std::string &localPath, const std::string &remoteName)
{
	operationPending = true;
	CDebugInterfaceC64U *di = debugInterface;
	std::string destPath = currentPath;
	if (destPath.back() != '/') destPath += '/';
	destPath += remoteName;

	std::thread([this, localPath, destPath, di]() {
		// Read local file
		FILE *f = fopen(localPath.c_str(), "rb");
		if (!f)
		{
			std::lock_guard<std::mutex> lock(resultMutex);
			pendingStatus = "Failed to open: " + localPath;
			hasPendingStatus = true;
			operationPending = false;
			return;
		}

		fseek(f, 0, SEEK_END);
		long fileSize = ftell(f);
		fseek(f, 0, SEEK_SET);
		std::vector<uint8_t> data(fileSize);
		fread(data.data(), 1, fileSize, f);
		fclose(f);

		C64UFtpClient *ftp = di->GetFtpClient();
		if (!ftp || !ftp->IsConnected())
		{
			std::lock_guard<std::mutex> lock(resultMutex);
			pendingStatus = "FTP not connected";
			hasPendingStatus = true;
			operationPending = false;
			return;
		}

		bool ok = ftp->UploadFile(destPath, data.data(), data.size());

		std::lock_guard<std::mutex> lock(resultMutex);
		if (ok)
			pendingStatus = "Uploaded: " + destPath;
		else
			pendingStatus = "Upload failed: " + destPath;
		hasPendingStatus = true;
		operationPending = false;
	}).detach();
}

bool CViewC64UFileBrowser::DoDropFile(char *filePath)
{
	if (operationPending.load()) return false;

	std::string path(filePath);
	// Extract filename from full path
	std::string fileName;
	size_t lastSlash = path.rfind('/');
	if (lastSlash != std::string::npos)
		fileName = path.substr(lastSlash + 1);
	else
		fileName = path;

	LOGD("CViewC64UFileBrowser::DoDropFile: uploading '%s' as '%s'", filePath, fileName.c_str());
	StartAsyncUpload(path, fileName);
	return true;
}

std::string CViewC64UFileBrowser::GetFileExtension(const std::string &name)
{
	size_t pos = name.rfind('.');
	if (pos == std::string::npos) return "";
	return name.substr(pos + 1);
}

std::string CViewC64UFileBrowser::FormatFileSize(uint64_t size)
{
	if (size < 1024) return std::to_string(size) + " B";
	if (size < 1024*1024) return std::to_string(size/1024) + " KB";
	return std::to_string(size/(1024*1024)) + " MB";
}
