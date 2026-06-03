#ifndef _CRENOISE_IMPORTER_H_
#define _CRENOISE_IMPORTER_H_

#include <string>
#include <vector>
#include <map>

constexpr int RENOISE_NOTE_EMPTY = -1;
constexpr int RENOISE_NOTE_OFF   = -2;

struct RenoiseNote {
    int noteValue;      // MIDI note (0-119), or RENOISE_NOTE_EMPTY / RENOISE_NOTE_OFF
    int instrument;     // -1 = none specified, 0+ = Renoise instrument index (0-based)
    RenoiseNote() : noteValue(RENOISE_NOTE_EMPTY), instrument(-1) {}
};

struct RenoisePatternLine {
    std::vector<RenoiseNote> noteColumns;   // All note columns for this line
    RenoisePatternLine() {}
};

struct RenoisePattern {
    int numLines;
    std::vector<std::vector<RenoisePatternLine>> tracks;  // tracks[trackIdx][lineIdx]
    RenoisePattern() : numLines(0) {}
};

struct RenoiseSequenceEntry {
    int patternIndex;
};

struct RenoiseSong {
    std::string name;
    std::string artist;
    int bpm;
    int linesPerBeat;
    int numTracks;
    std::vector<std::string> trackNames;
    std::vector<RenoisePattern> patterns;
    std::vector<RenoiseSequenceEntry> sequence;
    RenoiseSong() : bpm(120), linesPerBeat(4), numTracks(0) {}
};

struct RenoiseImportResult {
    bool success;
    std::string errorMessage;
    std::vector<std::string> warnings;
    RenoiseImportResult() : success(false) {}
};

class CRenoiseImporter
{
public:
    CRenoiseImporter();
    ~CRenoiseImporter();

    bool ParseXRNS(const char *filePath, RenoiseSong *outSong, RenoiseImportResult *result);
    bool WriteToGT2(const RenoiseSong *song, int trackMapping[3], int trackTranspose[3],
                    bool keepInstruments, RenoiseImportResult *result);

private:
    bool ExtractSongXml(const char *xrnsPath, std::vector<unsigned char> *outXml,
                        RenoiseImportResult *result);
    bool ParseSongXml(const unsigned char *xmlData, size_t xmlSize,
                      RenoiseSong *outSong, RenoiseImportResult *result);
    int ParseRenoiseNote(const char *noteStr);
    unsigned char MidiNoteToGT2(int midiNote);
};

#endif
