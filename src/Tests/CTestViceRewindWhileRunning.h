#pragma once

#include "CTest.h"

// Reproducer for the VICE 3.10 regression where rewinding the timeline
// (CSnapshotsManager::RestoreSnapshotByNumFramesOffset, cmd+left) WHILE the C64
// is RUNNING (not paused) intermittently corrupts 6502 state: the CPU jams,
// lands in BRK, or ends up paused (red CPU status).
//
// Loads tests/data/rewind-jam-test.prg (a frozen copy of the xparty raster-IRQ
// sprite demo), enables per-frame timeline recording, then repeatedly rewinds a
// few frames while leaving the emulator in RUNNING mode and checks for:
//   - CPU jam            (di->IsCpuJam())
//   - unexpected PAUSED  (GetDebugMode() flipped to DEBUGGER_MODE_PAUSED)
//   - cycle-counter stall (the 6502 stopped advancing == stuck / "deadlock")
//
// On failure it captures PC/opcode/SP/P/cycle/frame; the enriched JAM() log in
// c64cpusc.c additionally records the interrupt/alarm/CLK state at the jam.
class CTestViceRewindWhileRunning : public CTest
{
public:
	virtual const char *GetName() override { return "ViceRewindWhileRunning"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
