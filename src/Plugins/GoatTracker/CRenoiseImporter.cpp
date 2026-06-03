#include "CRenoiseImporter.h"
#include "libs/tinyxml2.h"
#include "libs/miniz.h"
#include <cstring>
#include <algorithm>

extern "C" {
#include "gcommon.h"
#include "gsong.h"
#include "gplay.h"
}

CRenoiseImporter::CRenoiseImporter() {}
CRenoiseImporter::~CRenoiseImporter() {}

// ---------------------------------------------------------------------------
// ExtractSongXml — ZIP extraction using miniz
// ---------------------------------------------------------------------------

bool CRenoiseImporter::ExtractSongXml(const char *xrnsPath,
                                       std::vector<unsigned char> *outXml,
                                       RenoiseImportResult *result)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, xrnsPath, 0))
    {
        result->errorMessage = "Failed to open .xrns file as ZIP archive";
        return false;
    }

    int fileIndex = mz_zip_reader_locate_file(&zip, "Song.xml", NULL, 0);
    if (fileIndex < 0)
    {
        result->errorMessage = "Song.xml not found in .xrns archive";
        mz_zip_reader_end(&zip);
        return false;
    }

    mz_zip_archive_file_stat fileStat;
    if (!mz_zip_reader_file_stat(&zip, fileIndex, &fileStat))
    {
        result->errorMessage = "Failed to read Song.xml metadata from archive";
        mz_zip_reader_end(&zip);
        return false;
    }

    outXml->resize((size_t)fileStat.m_uncomp_size);
    if (!mz_zip_reader_extract_to_mem(&zip, fileIndex, outXml->data(), outXml->size(), 0))
    {
        result->errorMessage = "Failed to extract Song.xml from archive";
        mz_zip_reader_end(&zip);
        return false;
    }

    mz_zip_reader_end(&zip);
    return true;
}

// ---------------------------------------------------------------------------
// ParseSongXml — XML parsing using TinyXML2
// ---------------------------------------------------------------------------

bool CRenoiseImporter::ParseSongXml(const unsigned char *xmlData, size_t xmlSize,
                                     RenoiseSong *outSong, RenoiseImportResult *result)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse((const char *)xmlData, xmlSize) != tinyxml2::XML_SUCCESS)
    {
        result->errorMessage = "Failed to parse Song.xml: ";
        result->errorMessage += doc.ErrorStr();
        return false;
    }

    auto *root = doc.FirstChildElement("RenoiseSong");
    if (!root)
    {
        result->errorMessage = "Invalid Renoise song: missing RenoiseSong root element";
        return false;
    }

    // --- GlobalSongData ---
    auto *globalData = root->FirstChildElement("GlobalSongData");
    if (globalData)
    {
        auto *el = globalData->FirstChildElement("BeatsPerMin");
        if (el && el->GetText()) outSong->bpm = atoi(el->GetText());

        el = globalData->FirstChildElement("LinesPerBeat");
        if (el && el->GetText()) outSong->linesPerBeat = atoi(el->GetText());

        el = globalData->FirstChildElement("SongName");
        if (el && el->GetText()) outSong->name = el->GetText();

        el = globalData->FirstChildElement("Artist");
        if (el && el->GetText()) outSong->artist = el->GetText();
    }

    // --- Tracks (direct child of RenoiseSong root, NOT inside GlobalSongData) ---
    auto *tracksNode = root->FirstChildElement("Tracks");
    if (tracksNode)
    {
        for (auto *track = tracksNode->FirstChildElement("SequencerTrack");
             track; track = track->NextSiblingElement("SequencerTrack"))
        {
            std::string trackName;
            auto *nameEl = track->FirstChildElement("Name");
            if (nameEl && nameEl->GetText())
                trackName = nameEl->GetText();
            else
                trackName = "Track " + std::to_string(outSong->numTracks + 1);

            outSong->trackNames.push_back(trackName);
            outSong->numTracks++;
        }
    }

    if (outSong->numTracks == 0)
    {
        result->errorMessage = "No sequencer tracks found in Renoise song";
        return false;
    }

    // --- PatternPool ---
    auto *patternPool = root->FirstChildElement("PatternPool");
    if (!patternPool)
    {
        result->errorMessage = "Missing PatternPool element";
        return false;
    }

    auto *patterns = patternPool->FirstChildElement("Patterns");
    if (!patterns)
    {
        result->errorMessage = "Missing Patterns element";
        return false;
    }

    for (auto *patEl = patterns->FirstChildElement("Pattern");
         patEl; patEl = patEl->NextSiblingElement("Pattern"))
    {
        RenoisePattern pat;

        auto *numLinesEl = patEl->FirstChildElement("NumberOfLines");
        pat.numLines = (numLinesEl && numLinesEl->GetText()) ? atoi(numLinesEl->GetText()) : 64;

        pat.tracks.resize(outSong->numTracks);
        for (int t = 0; t < outSong->numTracks; t++)
            pat.tracks[t].resize(pat.numLines);

        auto *tracksEl = patEl->FirstChildElement("Tracks");
        if (tracksEl)
        {
            int trackIdx = 0;
            for (auto *trackEl = tracksEl->FirstChildElement("PatternTrack");
                 trackEl; trackEl = trackEl->NextSiblingElement("PatternTrack"))
            {
                if (trackIdx >= outSong->numTracks) break;

                // Check for alias (AliasPatternIndex >= 0 means this track
                // copies data from another pattern; -1 means no alias)
                auto *aliasEl = trackEl->FirstChildElement("AliasPatternIndex");
                if (aliasEl && aliasEl->GetText())
                {
                    int aliasIdx = atoi(aliasEl->GetText());
                    if (aliasIdx >= 0)
                    {
                        // This is an actual alias — resolve it
                        if (aliasIdx < (int)outSong->patterns.size()
                            && trackIdx < (int)outSong->patterns[aliasIdx].tracks.size())
                        {
                            auto &srcTrack = outSong->patterns[aliasIdx].tracks[trackIdx];
                            int copyLines = std::min(pat.numLines, (int)srcTrack.size());
                            for (int i = 0; i < copyLines; i++)
                                pat.tracks[trackIdx][i] = srcTrack[i];
                        }
                        else
                        {
                            result->warnings.push_back(
                                "Pattern alias to index " + std::to_string(aliasIdx) +
                                " could not be resolved (forward reference or out of range)");
                        }
                        trackIdx++;
                        continue;
                    }
                    // aliasIdx == -1 means no alias — fall through to parse Lines normally
                }

                auto *linesEl = trackEl->FirstChildElement("Lines");
                if (linesEl)
                {
                    for (auto *lineEl = linesEl->FirstChildElement("Line");
                         lineEl; lineEl = lineEl->NextSiblingElement("Line"))
                    {
                        int lineIdx = -1;
                        lineEl->QueryIntAttribute("index", &lineIdx);
                        if (lineIdx < 0 || lineIdx >= pat.numLines) continue;

                        auto *noteColumns = lineEl->FirstChildElement("NoteColumns");
                        if (!noteColumns) continue;

                        RenoisePatternLine &pline = pat.tracks[trackIdx][lineIdx];

                        for (auto *noteCol = noteColumns->FirstChildElement("NoteColumn");
                             noteCol; noteCol = noteCol->NextSiblingElement("NoteColumn"))
                        {
                            RenoiseNote note;

                            auto *noteEl = noteCol->FirstChildElement("Note");
                            if (noteEl && noteEl->GetText())
                                note.noteValue = ParseRenoiseNote(noteEl->GetText());

                            auto *instrEl = noteCol->FirstChildElement("Instrument");
                            if (instrEl && instrEl->GetText())
                                note.instrument = (int)strtol(instrEl->GetText(), NULL, 16);

                            pline.noteColumns.push_back(note);
                        }
                    }
                }
                trackIdx++;
            }
        }

        outSong->patterns.push_back(pat);
    }

    // --- PatternSequence ---
    auto *patSeq = root->FirstChildElement("PatternSequence");
    if (!patSeq)
    {
        result->errorMessage = "Missing PatternSequence element";
        return false;
    }

    auto *seqEntries = patSeq->FirstChildElement("SequenceEntries");
    if (seqEntries)
    {
        for (auto *entry = seqEntries->FirstChildElement("SequenceEntry");
             entry; entry = entry->NextSiblingElement("SequenceEntry"))
        {
            auto *patIdxEl = entry->FirstChildElement("Pattern");
            if (patIdxEl && patIdxEl->GetText())
            {
                RenoiseSequenceEntry se;
                se.patternIndex = atoi(patIdxEl->GetText());
                outSong->sequence.push_back(se);
            }
        }
    }

    if (outSong->sequence.empty())
    {
        result->errorMessage = "Empty pattern sequence";
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Note conversion helpers
// ---------------------------------------------------------------------------

int CRenoiseImporter::ParseRenoiseNote(const char *noteStr)
{
    if (!noteStr || !noteStr[0])
        return RENOISE_NOTE_EMPTY;

    if (strcmp(noteStr, "OFF") == 0)
        return RENOISE_NOTE_OFF;

    int semitone = -1;
    int pos = 0;

    switch (noteStr[0])
    {
        case 'C': semitone = 0; break;
        case 'D': semitone = 2; break;
        case 'E': semitone = 4; break;
        case 'F': semitone = 5; break;
        case 'G': semitone = 7; break;
        case 'A': semitone = 9; break;
        case 'B': semitone = 11; break;
        default: return RENOISE_NOTE_EMPTY;
    }
    pos = 1;

    if (noteStr[pos] == '#')
    {
        semitone++;
        pos++;
    }

    if (noteStr[pos] == '-')
        pos++;

    int octave = noteStr[pos] - '0';
    if (octave < 0 || octave > 9)
        return RENOISE_NOTE_EMPTY;

    return octave * 12 + semitone;
}

unsigned char CRenoiseImporter::MidiNoteToGT2(int midiNote)
{
    int gt2Note = FIRSTNOTE + midiNote;
    if (gt2Note > LASTNOTE)
        gt2Note = LASTNOTE;
    return (unsigned char)gt2Note;
}

// ---------------------------------------------------------------------------
// ParseXRNS — public entry point
// ---------------------------------------------------------------------------

bool CRenoiseImporter::ParseXRNS(const char *filePath, RenoiseSong *outSong,
                                  RenoiseImportResult *result)
{
    std::vector<unsigned char> xmlData;
    if (!ExtractSongXml(filePath, &xmlData, result))
        return false;

    if (!ParseSongXml(xmlData.data(), xmlData.size(), outSong, result))
        return false;

    result->success = true;
    return true;
}

// ---------------------------------------------------------------------------
// WriteToGT2 — convert and write to GT2 globals
// ---------------------------------------------------------------------------

bool CRenoiseImporter::WriteToGT2(const RenoiseSong *song, int trackMapping[3], int trackTranspose[3],
                                   bool keepInstruments, RenoiseImportResult *result)
{
    // Build compact map of unique patterns used in sequence
    std::map<int, int> compactMap;
    std::vector<int> uniquePatterns;
    for (const auto &entry : song->sequence)
    {
        if (compactMap.find(entry.patternIndex) == compactMap.end())
        {
            compactMap[entry.patternIndex] = (int)uniquePatterns.size();
            uniquePatterns.push_back(entry.patternIndex);
        }
    }

    int totalGT2Patterns = (int)uniquePatterns.size() * MAX_CHN;
    if (totalGT2Patterns > MAX_PATT - 1)
    {
        result->errorMessage = "Too many unique patterns (" +
            std::to_string(uniquePatterns.size()) + "). Max supported: " +
            std::to_string((MAX_PATT - 1) / MAX_CHN) + ".";
        return false;
    }

    if ((int)song->sequence.size() > MAX_SONGLEN)
    {
        result->errorMessage = "Sequence too long (" +
            std::to_string(song->sequence.size()) + " entries). Max: " +
            std::to_string(MAX_SONGLEN) + ".";
        return false;
    }

    // Clear (clearsong calls stopsong internally)
    if (keepInstruments)
        clearsong(1, 1, 0, 0, 1);
    else
        clearsong(1, 1, 1, 1, 1);

    // Convert patterns
    for (int ui = 0; ui < (int)uniquePatterns.size(); ui++)
    {
        int renoisePatIdx = uniquePatterns[ui];
        if (renoisePatIdx < 0 || renoisePatIdx >= (int)song->patterns.size())
        {
            result->warnings.push_back("Sequence references non-existent pattern " +
                std::to_string(renoisePatIdx) + ", skipping");
            continue;
        }

        const RenoisePattern &renPat = song->patterns[renoisePatIdx];
        int pattRows = renPat.numLines;
        if (pattRows > MAX_PATTROWS)
        {
            result->warnings.push_back("Pattern " + std::to_string(renoisePatIdx) +
                " has " + std::to_string(pattRows) + " rows, truncating to " +
                std::to_string(MAX_PATTROWS));
            pattRows = MAX_PATTROWS;
        }

        for (int ch = 0; ch < MAX_CHN; ch++)
        {
            int gt2PatIdx = ui * MAX_CHN + ch;
            int renTrack = trackMapping[ch];
            int transpose = trackTranspose[ch];

            // Initialize pattern with REST (clearsong already zeroed, but be explicit)
            for (int r = 0; r <= MAX_PATTROWS; r++)
                pattern[gt2PatIdx][r * 4] = REST;

            if (renTrack < 0 || renTrack >= (int)renPat.tracks.size())
            {
                pattern[gt2PatIdx][pattRows * 4] = ENDPATT;
                continue;
            }

            const auto &trackData = renPat.tracks[renTrack];

            // Determine max note columns in this track for arp detection
            int maxNoteCols = 0;
            for (int r = 0; r < pattRows; r++)
            {
                int nc = (int)trackData[r].noteColumns.size();
                if (nc > maxNoteCols) maxNoteCols = nc;
            }

            // If track has >1 note column, enable arp and track max columns needed
            if (maxNoteCols > 1)
            {
                int arpCols = maxNoteCols - 1; // first column is base note
                if (arpCols > MAX_ARP_COLS) arpCols = MAX_ARP_COLS;
                if (arpCols > numarpcolumns) numarpcolumns = arpCols;
            }

            for (int r = 0; r < pattRows; r++)
            {
                int offset = r * 4;
                const RenoisePatternLine &pline = trackData[r];

                // Column 0: base note → pattern[]
                if (!pline.noteColumns.empty())
                {
                    const RenoiseNote &note = pline.noteColumns[0];

                    if (note.noteValue == RENOISE_NOTE_OFF)
                        pattern[gt2PatIdx][offset + 0] = KEYOFF;
                    else if (note.noteValue >= 0)
                    {
                        int tNote = note.noteValue + transpose;
                        if (tNote < 0) tNote = 0;
                        if (tNote > 119) tNote = 119;
                        pattern[gt2PatIdx][offset + 0] = MidiNoteToGT2(tNote);
                    }
                    else
                        pattern[gt2PatIdx][offset + 0] = REST;

                    if (note.instrument >= 0)
                    {
                        int gt2Instr = note.instrument + 1;
                        if (gt2Instr >= MAX_INSTR)
                            gt2Instr = MAX_INSTR - 1;
                        pattern[gt2PatIdx][offset + 1] = (unsigned char)gt2Instr;
                    }
                    else
                    {
                        pattern[gt2PatIdx][offset + 1] = 0x00;
                    }
                }

                // Bytes 2-3: Command + Parameter (always 0 for now)
                pattern[gt2PatIdx][offset + 2] = 0x00;
                pattern[gt2PatIdx][offset + 3] = 0x00;

                // Columns 1+: arp data → arpdata[]
                for (int a = 0; a < MAX_ARP_COLS; a++)
                {
                    int colIdx = a + 1; // column 0 is base, column 1+ are arp
                    if (colIdx < (int)pline.noteColumns.size())
                    {
                        const RenoiseNote &arpNote = pline.noteColumns[colIdx];
                        if (arpNote.noteValue == RENOISE_NOTE_OFF)
                            arpdata[gt2PatIdx][ch][r][a] = KEYOFF;
                        else if (arpNote.noteValue >= 0)
                        {
                            int tNote = arpNote.noteValue + transpose;
                            if (tNote < 0) tNote = 0;
                            if (tNote > 119) tNote = 119;
                            arpdata[gt2PatIdx][ch][r][a] = MidiNoteToGT2(tNote);
                        }
                        else
                            arpdata[gt2PatIdx][ch][r][a] = 0x00;
                    }
                    else
                    {
                        arpdata[gt2PatIdx][ch][r][a] = 0x00;
                    }
                }
            }

            // ENDPATT terminator
            pattern[gt2PatIdx][pattRows * 4] = ENDPATT;
        }
    }

    // Build song order
    for (int ch = 0; ch < MAX_CHN; ch++)
    {
        int pos = 0;
        for (const auto &entry : song->sequence)
        {
            auto it = compactMap.find(entry.patternIndex);
            if (it != compactMap.end())
            {
                int gt2PatIdx = it->second * MAX_CHN + ch;
                songorder[0][ch][pos] = (unsigned char)gt2PatIdx;
                pos++;
            }
        }
        songorder[0][ch][pos] = LOOPSONG;
        songorder[0][ch][pos + 1] = 0x00;  // Loop destination: start
    }

    // Metadata (truncate to MAX_STR-1 = 31 chars)
    memset(songname, 0, MAX_STR);
    memset(authorname, 0, MAX_STR);
    strncpy(songname, song->name.c_str(), MAX_STR - 1);
    strncpy(authorname, song->artist.c_str(), MAX_STR - 1);

    // Recalculate all derived state
    countpatternlengths();

    result->success = true;
    return true;
}
