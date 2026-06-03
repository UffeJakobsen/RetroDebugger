#include "CTestViceInputReplay.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "CDebugInterfaceVice.h"
#include "CSnapshotsManager.h"
#include "DebuggerDefs.h"
#include "SYS_Main.h"
#include "CByteBuffer.h"
#include <cstring>
#include <cstdio>

// 6510 test program at $1000: SEI, read $DC00 in tight loop, store 256 samples to $1100
//
// Address map:
//   $1000: SEI
//   $1001: LDA #$00
//   $1003: STA $FB       ; offset = 0
//   $1005: LDA $DC00     ; <- loop
//   $1008: EOR #$FF
//   $100A: AND #$1F
//   $100C: LDX $FB
//   $100E: STA $1100,X
//   $1011: INX
//   $1012: STX $FB
//   $1014: CPX #$00
//   $1016: BNE $1005     ; offset = $1005 - ($1016+2) = -19 = $ED
//   $1018: LDA #$FF
//   $101A: STA $11FF     ; done marker
//   $101D: JMP $101D     ; spin

static const u8 testCode[] = {
	0x78,             // $1000: SEI
	0xA9, 0x00,       // $1001: LDA #$00
	0x85, 0xFB,       // $1003: STA $FB
	0xAD, 0x00, 0xDC, // $1005: LDA $DC00     ; loop
	0x49, 0xFF,       // $1008: EOR #$FF
	0x29, 0x1F,       // $100A: AND #$1F
	0xA6, 0xFB,       // $100C: LDX $FB
	0x9D, 0x00, 0x11, // $100E: STA $1100,X
	0xE8,             // $1011: INX
	0x86, 0xFB,       // $1012: STX $FB
	0xE0, 0x00,       // $1014: CPX #$00
	0xD0, 0xED,       // $1016: BNE loop       ; -19 = $ED -> $1005
	0xA9, 0xFF,       // $1018: LDA #$FF
	0x8D, 0xFF, 0x11, // $101A: STA $11FF
	0x4C, 0x1D, 0x10  // $101D: JMP $101D
};

static const u16 CODE_ADDR = 0x1000;
static const u16 RESULTS_ADDR = 0x1100;
static const u16 DONE_MARKER_ADDR = 0x11FF;
static const int NUM_SAMPLES = 256;

static void InjectAndRun(CDebugInterfaceVice *di)
{
	di->PauseEmulationBlockedWait();
	for (int i = 0; i < (int)sizeof(testCode); i++)
		di->SetByteToRamC64(CODE_ADDR + i, testCode[i]);
	for (int i = 0; i < NUM_SAMPLES; i++)
		di->SetByteToRamC64(RESULTS_ADDR + i, 0x00);
	di->MakeJmpC64(CODE_ADDR);
	di->SetDebugMode(DEBUGGER_MODE_RUNNING);
}

static bool WaitForDone(CDebugInterfaceVice *di)
{
	for (int tries = 0; tries < 200; tries++)
	{
		SYS_Sleep(10);
		if (di->GetByteFromRamC64(DONE_MARKER_ADDR) == 0xFF)
			return true;
	}
	return false;
}

static void ReadResults(CDebugInterfaceVice *di, u8 *buffer)
{
	di->PauseEmulationBlockedWait();
	for (int i = 0; i < NUM_SAMPLES; i++)
		buffer[i] = di->GetByteFromRamC64(RESULTS_ADDR + i);
	di->SetDebugMode(DEBUGGER_MODE_RUNNING);
	SYS_Sleep(50);
}

void CTestViceInputReplay::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;
	char msg[512];

#ifndef RUN_COMMODORE64
	TestCompleted(true, "Skipped (C64 not enabled)");
	return;
#else
	CDebugInterfaceVice *di = (CDebugInterfaceVice *)viewC64->debugInterfaceC64;
	if (!di)
	{
		TestCompleted(false, "C64 debug interface is NULL");
		return;
	}

	bool wasRunning = di->isRunning;
	if (!wasRunning)
	{
		viewC64->StartEmulationThread(di);
		SYS_Sleep(2000);
	}

	if (!di->isRunning)
	{
		TestCompleted(false, "C64 emulator failed to start");
		return;
	}

	if (di->GetDebugMode() != DEBUGGER_MODE_RUNNING)
		di->SetDebugMode(DEBUGGER_MODE_RUNNING);
	SYS_Sleep(500);

	CSnapshotsManager *sm = di->snapshotsManager;

	// --- Step 1: Run test program (no input — baseline) ---
	InjectAndRun(di);

	if (!WaitForDone(di))
	{
		di->PauseEmulationBlockedWait();
		int pc = di->GetCpuPC();
		di->SetDebugMode(DEBUGGER_MODE_RUNNING);
		snprintf(msg, sizeof(msg), "Test program did not complete: PC=$%04X", pc);
		StepCompleted(1, false, msg);
		if (!wasRunning) viewC64->StopEmulationThread(di);
		TestCompleted(false, msg);
		return;
	}

	u8 baseline[NUM_SAMPLES];
	ReadResults(di, baseline);

	// Baseline should be all zeros (no joystick pressed, CIA returns $FF, inverted+masked = $00 if no buttons)
	// Actually CIA $DC00 returns active-low: unpressed = 1, pressed = 0. EOR #$FF inverts, AND #$1F masks.
	// With no joystick: CIA returns $FF, EOR = $00, AND = $00. So baseline = all zeros. Good.
	int baseNonZero = 0;
	for (int i = 0; i < NUM_SAMPLES; i++)
		if (baseline[i] != 0) baseNonZero++;

	snprintf(msg, sizeof(msg), "Baseline: %d/%d non-zero (expect 0 with no joy input)", baseNonZero, NUM_SAMPLES);
	StepCompleted(1, true, msg);

	// --- Step 2: Test replay path (pre-populate events → enable replay → verify 6510 detects input) ---
	// Press fire via task queue, wait for it to apply, then run 6510 code.
	// The task goes through ExecuteDebugInterruptTasks and calls c64d_joystick_key_down.
	di->JoystickDown(1, JOYPAD_FIRE);
	SYS_Sleep(200);

	uint32 joyState = di->GetJoystickState(1);
	if ((joyState & JOYPAD_FIRE) == 0)
	{
		snprintf(msg, sizeof(msg), "JoystickDown did not set fire: GetJoystickState(1)=$%02X", joyState);
		StepCompleted(2, false, msg);
		if (!wasRunning) viewC64->StopEmulationThread(di);
		TestCompleted(false, msg);
		return;
	}

	InjectAndRun(di);

	if (!WaitForDone(di))
	{
		di->JoystickUp(1, JOYPAD_FIRE);
		StepCompleted(2, false, "Test program did not complete with joystick pressed");
		if (!wasRunning) viewC64->StopEmulationThread(di);
		TestCompleted(false, "VICE test program timeout");
		return;
	}

	u8 replayResults[NUM_SAMPLES];
	ReadResults(di, replayResults);

	// Release joystick (cleanup for subsequent tests)
	di->JoystickUp(1, JOYPAD_FIRE);
	SYS_Sleep(100);

	int replayNonZero = 0;
	int newNonZero = 0;
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		if (replayResults[i] != 0) replayNonZero++;
		if (replayResults[i] != 0 && baseline[i] == 0) newNonZero++;
	}

	if (newNonZero > 0)
	{
		snprintf(msg, sizeof(msg), "Task injection: %d new non-zero — 6510 detected FIRE via $DC00", newNonZero);
		StepCompleted(2, true, msg);
	}
	else
	{
		snprintf(msg, sizeof(msg), "Task injection: %d non-zero (same as baseline %d)", replayNonZero, baseNonZero);
		StepCompleted(2, false, msg);
		if (!wasRunning) viewC64->StopEmulationThread(di);
		TestCompleted(false, "VICE task injection: 6510 did not detect joystick");
		return;
	}

	// --- Step 3: Test event map replay path ---
	// Place event 500 cycles in the future from CURRENT cycle (while running),
	// enable replay, wait for it to fire, check GetJoystickState.
	// This tests CheckInputEventsAtCurrentCycle → ReplayInputEventsFromSnapshotsManager.

	u64 baseCycle = di->GetMainCpuCycleCounter();
	u64 eventCycle = baseCycle + 500;

	CStoredInputEvent *ev = sm->GetNewInputEventSnapshot(0, eventCycle);
	sm->inputEventsByCycle[eventCycle] = ev;
	ev->byteBuffer->PutU8(DEBUGGER_EVENT_TYPE_JOYSTICK);
	ev->byteBuffer->PutI32(1);          // port 1
	ev->byteBuffer->PutU32(JOYPAD_N);   // UP (different from Step 2's FIRE)
	ev->byteBuffer->PutU8(DEBUGGER_EVENT_BUTTON_DOWN);

	sm->isReplayInputEventsEnabled = true;

	// Wait for event to fire (500 cycles ~ 0.5ms, sleep much longer)
	SYS_Sleep(300);

	uint32 afterMapReplay = di->GetJoystickState(1);

	sm->isReplayInputEventsEnabled = false;
	for (auto &pair : sm->inputEventsByCycle)
		sm->inputEventsToReuse.push_back(pair.second);
	sm->inputEventsByCycle.clear();

	// Cleanup joystick
	di->JoystickUp(1, JOYPAD_N);
	SYS_Sleep(100);

	bool mapReplayWorked = (afterMapReplay & JOYPAD_N) != 0;

	if (mapReplayWorked)
	{
		snprintf(msg, sizeof(msg), "Map replay: GetJoystickState(1)=$%02X (N bit set) — ReplayInputEventsFromSnapshotsManager works", afterMapReplay);
		StepCompleted(3, true, msg);
		TestCompleted(true, "VICE input replay: task injection + map replay both verified");
	}
	else
	{
		snprintf(msg, sizeof(msg), "Map replay: GetJoystickState(1)=$%02X (N bit NOT set) — events not replayed", afterMapReplay);
		StepCompleted(3, false, msg);
		TestCompleted(false, "VICE map replay: events not applied");
	}

	if (!wasRunning) viewC64->StopEmulationThread(di);
#endif
}
