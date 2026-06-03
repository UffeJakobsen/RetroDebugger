#ifndef _CVIEW_RENOISE_IMPORT_H_
#define _CVIEW_RENOISE_IMPORT_H_

#include "SYS_Defs.h"
#include "CSystemFileDialogCallback.h"
#include "CRenoiseImporter.h"
#include <string>

class CSlrString;

class CViewRenoiseImport : public CSystemFileDialogCallback
{
public:
    CViewRenoiseImport();
    ~CViewRenoiseImport();

    void Open();
    void Close();
    void RenderImGui();

    virtual void SystemDialogFileOpenSelected(CSlrString *path);
    virtual void SystemDialogFileOpenCancelled();

    bool isVisible;

private:
    void LoadXRNSFile(const char *path);
    void DoImport();
    void OpenFileDialog();

    std::string filePath;
    std::string fileName;
    bool fileLoaded;

    RenoiseSong parsedSong;
    bool songParsed;

    int trackMapping[3];
    int trackTranspose[3];
    bool keepInstruments;

    std::string statusMessage;
    bool statusIsError;

    CRenoiseImporter importer;
};

#endif
