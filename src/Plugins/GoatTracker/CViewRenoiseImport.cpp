#include "CViewRenoiseImport.h"
#include "C64DebuggerPluginGoatTracker.h"
#include "CViewC64.h"
#include "CViewGT2Patterns.h"
#include "CViewGT2Tables.h"
#include "CConfigStorageHjson.h"
#include "CSlrString.h"
#include "SYS_Main.h"
#include "DBG_Log.h"
#include "imgui.h"
#include <algorithm>
#include <list>

CViewRenoiseImport::CViewRenoiseImport()
    : isVisible(false)
    , fileLoaded(false)
    , songParsed(false)
    , keepInstruments(false)
    , statusIsError(false)
{
    trackMapping[0] = 0;
    trackMapping[1] = 1;
    trackMapping[2] = 2;
    trackTranspose[0] = -12;
    trackTranspose[1] = -12;
    trackTranspose[2] = -12;
}

CViewRenoiseImport::~CViewRenoiseImport()
{
}

void CViewRenoiseImport::Open()
{
    isVisible = true;
    statusMessage.clear();
    statusIsError = false;

    // Restore all dialog state from config: track mapping per SID channel
    // and the "keep existing instruments" toggle. Loaded before the file
    // so LoadXRNSFile can clamp the mapping to the song's track count
    // rather than overwriting it with defaults on every reopen.
    viewC64->config->GetInt("RenoiseImportTrackMap0", &trackMapping[0], 0);
    viewC64->config->GetInt("RenoiseImportTrackMap1", &trackMapping[1], 1);
    viewC64->config->GetInt("RenoiseImportTrackMap2", &trackMapping[2], 2);
    viewC64->config->GetInt("RenoiseImportTranspose0", &trackTranspose[0], -12);
    viewC64->config->GetInt("RenoiseImportTranspose1", &trackTranspose[1], -12);
    viewC64->config->GetInt("RenoiseImportTranspose2", &trackTranspose[2], -12);
    viewC64->config->GetBool("RenoiseImportKeepInstruments", &keepInstruments, false);

    const char *lastPath = NULL;
    viewC64->config->GetString("RenoiseImportPath", &lastPath, "");
    if (lastPath && lastPath[0])
    {
        LoadXRNSFile(lastPath);
    }
}

void CViewRenoiseImport::Close()
{
    isVisible = false;
}

void CViewRenoiseImport::OpenFileDialog()
{
    std::list<CSlrString *> extensions;
    extensions.push_back(new CSlrString("xrns"));

    // Get last folder from config
    const char *lastFolder = NULL;
    viewC64->config->GetString("RenoiseImportFolder", &lastFolder, "");
    CSlrString *defaultFolder = (lastFolder && lastFolder[0]) ? new CSlrString(lastFolder) : NULL;

    CSlrString *windowTitle = new CSlrString("Open Renoise Song");
    viewC64->ShowDialogOpenFile(this, &extensions, defaultFolder, windowTitle);

    delete windowTitle;
    if (defaultFolder) delete defaultFolder;
    for (auto *ext : extensions) delete ext;
}

void CViewRenoiseImport::SystemDialogFileOpenSelected(CSlrString *path)
{
    char *cPath = path->GetStdASCII();
    LoadXRNSFile(cPath);

    // Save path to config
    viewC64->config->SetString("RenoiseImportPath", (const char **)&cPath);

    // Save folder
    CSlrString *folder = path->GetFilePathWithoutFileNameComponentFromPath();
    char *cFolder = folder->GetStdASCII();
    viewC64->config->SetString("RenoiseImportFolder", (const char **)&cFolder);
    delete[] cFolder;
    delete folder;

    delete[] cPath;
}

void CViewRenoiseImport::SystemDialogFileOpenCancelled()
{
}

void CViewRenoiseImport::LoadXRNSFile(const char *path)
{
    filePath = path;
    songParsed = false;
    fileLoaded = false;
    statusMessage.clear();
    statusIsError = false;

    size_t lastSlash = filePath.find_last_of("/\\");
    fileName = (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;

    RenoiseImportResult result;
    parsedSong = RenoiseSong();

    if (importer.ParseXRNS(path, &parsedSong, &result))
    {
        songParsed = true;
        fileLoaded = true;

        // Clamp the restored mapping to this song's track count. Only
        // reset to the default (i→i) when the saved value is out of
        // range, so reopening the dialog after picking a custom mapping
        // preserves it.
        for (int i = 0; i < 3; i++)
        {
            if (trackMapping[i] >= parsedSong.numTracks)
                trackMapping[i] = (i < parsedSong.numTracks) ? i : -1;
        }
    }
    else
    {
        statusMessage = result.errorMessage;
        statusIsError = true;
    }
}

void CViewRenoiseImport::DoImport()
{
    if (!songParsed) return;

    RenoiseImportResult result;
    if (importer.WriteToGT2(&parsedSong, trackMapping, trackTranspose, keepInstruments, &result))
	{
		if (pluginGoatTracker && pluginGoatTracker->viewPatterns)
			pluginGoatTracker->viewPatterns->ClearPatternUndoHistory();
		if (pluginGoatTracker && pluginGoatTracker->viewTables)
			pluginGoatTracker->viewTables->ClearTableUndoHistory();
		statusMessage = "Import successful!";
        statusIsError = false;

        for (const auto &w : result.warnings)
            statusMessage += "\n  Warning: " + w;
    }
    else
    {
        statusMessage = "Import failed: " + result.errorMessage;
        statusIsError = true;
    }
}

void CViewRenoiseImport::RenderImGui()
{
    if (!isVisible) return;

    ImGui::OpenPopup("Import Renoise Song");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Import Renoise Song", &isVisible,
                                ImGuiWindowFlags_AlwaysAutoResize))
    {
        // File selection
        ImGui::Text("File:");
        ImGui::SameLine();
        if (fileLoaded)
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "%s", fileName.c_str());
        else
            ImGui::TextDisabled("(none)");
        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
            OpenFileDialog();

        // Song info
        if (songParsed)
        {
            ImGui::Separator();
            ImGui::Text("Tracks: %d  |  Patterns: %d  |  BPM: %d",
                         parsedSong.numTracks,
                         (int)parsedSong.patterns.size(),
                         parsedSong.bpm);

            // Track mapping
            ImGui::Separator();
            ImGui::Text("Track Mapping");

            const char *sidLabels[3] = { "SID Channel 1", "SID Channel 2", "SID Channel 3" };

            for (int ch = 0; ch < 3; ch++)
            {
                std::string previewStr;
                if (trackMapping[ch] < 0)
                    previewStr = "-- None --";
                else if (trackMapping[ch] < parsedSong.numTracks)
                    previewStr = std::to_string(trackMapping[ch] + 1) + ": " +
                                 parsedSong.trackNames[trackMapping[ch]];
                else
                    previewStr = "-- None --";

                ImGui::PushID(ch);
                ImGui::SetNextItemWidth(220.0f);
                if (ImGui::BeginCombo(sidLabels[ch], previewStr.c_str()))
                {
                    if (ImGui::Selectable("-- None --", trackMapping[ch] < 0))
                    {
                        trackMapping[ch] = -1;
                        const char *key = (ch == 0) ? "RenoiseImportTrackMap0"
                                        : (ch == 1) ? "RenoiseImportTrackMap1"
                                                    : "RenoiseImportTrackMap2";
                        viewC64->config->SetInt(key, &trackMapping[ch]);
                    }

                    for (int t = 0; t < parsedSong.numTracks; t++)
                    {
                        std::string label = std::to_string(t + 1) + ": " + parsedSong.trackNames[t];
                        if (ImGui::Selectable(label.c_str(), trackMapping[ch] == t))
                        {
                            trackMapping[ch] = t;
                            const char *key = (ch == 0) ? "RenoiseImportTrackMap0"
                                            : (ch == 1) ? "RenoiseImportTrackMap1"
                                                        : "RenoiseImportTrackMap2";
                            viewC64->config->SetInt(key, &trackMapping[ch]);
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine();
                ImGui::SetNextItemWidth(90.0f);
                if (ImGui::InputInt("Transpose", &trackTranspose[ch], 1, 12))
                {
                    if (trackTranspose[ch] < -96) trackTranspose[ch] = -96;
                    if (trackTranspose[ch] >  96) trackTranspose[ch] =  96;
                    const char *key = (ch == 0) ? "RenoiseImportTranspose0"
                                    : (ch == 1) ? "RenoiseImportTranspose1"
                                                : "RenoiseImportTranspose2";
                    viewC64->config->SetInt(key, &trackTranspose[ch]);
                }
                ImGui::PopID();
            }

            // Options
            ImGui::Separator();
            if (ImGui::Checkbox("Keep existing instruments", &keepInstruments))
                viewC64->config->SetBool("RenoiseImportKeepInstruments", &keepInstruments);
        }

        // Status
        if (!statusMessage.empty())
        {
            ImGui::Separator();
            if (statusIsError)
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", statusMessage.c_str());
            else
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", statusMessage.c_str());
        }

        // Buttons
        ImGui::Separator();
        bool canImport = songParsed;
        if (!canImport) ImGui::BeginDisabled();
        if (ImGui::Button("Import", ImVec2(120, 0)))
        {
            DoImport();
            if (!statusIsError)
            {
                Close();
                ImGui::CloseCurrentPopup();
            }
        }
        if (!canImport) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            Close();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    else
    {
        isVisible = false;
    }
}
