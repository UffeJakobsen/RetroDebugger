#ifndef _CGT2VoiceWaveforms_H_
#define _CGT2VoiceWaveforms_H_

// Per-voice waveform buffers fed directly from GT2's SID audio loop
// (gsid.cpp). The C64 emulator's debugInterface->sidChannelWaveform[]
// only receives data from VICE — GT2's audio path bypasses VICE, so the
// oscilloscope view needs its own buffers that GT2 fills from the
// reSID-style instance it drives.

#ifdef __cplusplus
class CWaveformData;
extern "C" {
#endif

// One sample for each voice + the mixed output, called from the SID
// render loop after every clock() chunk. Implemented C++-side; safe to
// call even before initialization (no-ops while pointers are NULL).
void c64d_gt2_capture_voice_samples(int v0, int v1, int v2, short mix);

#ifdef __cplusplus
}   // extern "C"

// C++ access to the underlying buffers — the oscilloscope view binds
// CViewWaveform instances over them. Lifetime owned by the GT2 plugin
// (created in Init, destroyed in shutdown).
extern CWaveformData *gt2VoiceWaveform[3];
extern CWaveformData *gt2MixWaveform;

// Allocates / frees the buffers. Length is the rolling sample window
// each waveform stores (1024 ≈ ~23ms at 44.1kHz, a typical oscilloscope
// span for a SID note).
void GT2_VoiceWaveforms_Create(int sampleWindow);
void GT2_VoiceWaveforms_Destroy();

// Per-frame snapshot + trigger-position recompute, mirroring
// CDebugInterfaceC64::UpdateWaveforms(). Call once per UI frame before
// the oscilloscope view renders.
void GT2_VoiceWaveforms_UpdatePerFrame();

#endif

#endif
