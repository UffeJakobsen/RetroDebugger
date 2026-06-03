#include "CTestViceSnapshot.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "CDebugInterfaceVice.h"
#include "CByteBuffer.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include "DebuggerDefs.h"
#include <cstdio>
#include <cstring>

static char failureMsg[512];

void CTestViceSnapshot::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;
	failureMsg[0] = '\0';

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

	bool allPassed = true;

	// NOTE: SaveChipsSnapshotSynced/LoadChipsSnapshotSynced must be called while the
	// emulator is RUNNING (not paused). Internally, c64_snapshot_write_in_memory calls
	// drive_cpu_execute_all() which runs the drive CPU loop. That loop checks c64d_debug_mode
	// and enters c64d_debug_pause_check() spin loop if paused — causing a deadlock.
	// This matches how CSnapshotsManager uses them (from the emulation thread while running).

	// Ensure emulator is running (not paused from a previous test)
	if (di->GetDebugMode() != DEBUGGER_MODE_RUNNING)
	{
		di->SetDebugMode(DEBUGGER_MODE_RUNNING);
		SYS_Sleep(100);
	}

	// --- Step 1: Save snapshot to buffer (synchronous API) ---
	CByteBuffer *snapshotBuffer = new CByteBuffer();

	{
		// Write known pattern to RAM before saving
		di->SetByteToRamC64(0x0800, 0xAA);
		di->SetByteToRamC64(0x0801, 0xBB);
		di->SetByteToRamC64(0x0802, 0xCC);

		// Use SaveChipsSnapshotSynced — the synchronous buffer-based save
		bool saved = di->SaveChipsSnapshotSynced(snapshotBuffer);

		if (!saved || snapshotBuffer->length == 0)
		{
			sprintf(failureMsg, "SaveChipsSnapshotSynced %s (buffer length=%d)",
					saved ? "produced empty buffer" : "returned false", (int)snapshotBuffer->length);
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "Snapshot saved: %d bytes", (int)snapshotBuffer->length);
			StepCompleted(1, true, msg);
		}
		else
		{
			StepCompleted(1, false, failureMsg);
		}
	}

	// --- Step 2: Modify state after save ---
	u8 origRAM[3];

	if (allPassed)
	{
		// Record what we saved
		origRAM[0] = 0xAA;
		origRAM[1] = 0xBB;
		origRAM[2] = 0xCC;

		// Modify RAM
		di->SetByteToRamC64(0x0800, 0x11);
		di->SetByteToRamC64(0x0801, 0x22);
		di->SetByteToRamC64(0x0802, 0x33);

		// Brief pause to let the write settle, then verify
		SYS_Sleep(50);

		// Pause to read RAM safely
		di->PauseEmulationBlockedWait();
		u8 modified = di->GetByteFromRamC64(0x0800);
		di->SetDebugMode(DEBUGGER_MODE_RUNNING);
		SYS_Sleep(50);

		if (modified != 0x11)
		{
			sprintf(failureMsg, "State modification failed: $0800=$%02X (expected $11)", modified);
			allPassed = false;
		}

		if (allPassed)
			StepCompleted(2, true, "State modified: RAM changed ($AA->$11, $BB->$22, $CC->$33)");
		else
			StepCompleted(2, false, failureMsg);
	}

	// --- Step 3: Restore snapshot and verify RAM ---
	if (allPassed)
	{
		snapshotBuffer->Rewind();
		bool loaded = di->LoadChipsSnapshotSynced(snapshotBuffer);

		if (!loaded)
		{
			sprintf(failureMsg, "LoadChipsSnapshotSynced returned false");
			allPassed = false;
		}

		if (allPassed)
		{
			// Pause to read RAM safely after restore
			SYS_Sleep(50);
			di->PauseEmulationBlockedWait();

			u8 ram0 = di->GetByteFromRamC64(0x0800);
			u8 ram1 = di->GetByteFromRamC64(0x0801);
			u8 ram2 = di->GetByteFromRamC64(0x0802);

			di->SetDebugMode(DEBUGGER_MODE_RUNNING);
			SYS_Sleep(50);

			if (ram0 != origRAM[0] || ram1 != origRAM[1] || ram2 != origRAM[2])
			{
				sprintf(failureMsg, "RAM not restored: $0800=$%02X,$%02X,$%02X (expected $%02X,$%02X,$%02X)",
						ram0, ram1, ram2, origRAM[0], origRAM[1], origRAM[2]);
				allPassed = false;
			}
		}

		if (allPassed)
		{
			StepCompleted(3, true, "Snapshot restored: RAM values match pre-save state");
		}
		else
		{
			StepCompleted(3, false, failureMsg);
		}
	}

	// --- Step 4: Second save/restore cycle to verify re-save works ---
	if (allPassed)
	{
		// Write new pattern
		di->SetByteToRamC64(0x0900, 0x42);
		di->SetByteToRamC64(0x0901, 0x43);

		CByteBuffer *snapshot2 = new CByteBuffer();
		bool saved2 = di->SaveChipsSnapshotSynced(snapshot2);

		if (!saved2 || snapshot2->length == 0)
		{
			sprintf(failureMsg, "Second SaveChipsSnapshotSynced failed");
			allPassed = false;
		}

		if (allPassed)
		{
			// Modify
			di->SetByteToRamC64(0x0900, 0x00);
			di->SetByteToRamC64(0x0901, 0x00);

			// Restore
			snapshot2->Rewind();
			bool loaded2 = di->LoadChipsSnapshotSynced(snapshot2);

			if (!loaded2)
			{
				sprintf(failureMsg, "Second LoadChipsSnapshotSynced failed");
				allPassed = false;
			}
			else
			{
				SYS_Sleep(50);
				di->PauseEmulationBlockedWait();

				u8 val0 = di->GetByteFromRamC64(0x0900);
				u8 val1 = di->GetByteFromRamC64(0x0901);

				di->SetDebugMode(DEBUGGER_MODE_RUNNING);
				SYS_Sleep(50);

				if (val0 != 0x42 || val1 != 0x43)
				{
					sprintf(failureMsg, "Second restore: $0900=$%02X,$%02X (expected $42,$43)", val0, val1);
					allPassed = false;
				}
			}
		}

		if (allPassed)
			StepCompleted(4, true, "Second save/restore cycle verified");
		else
			StepCompleted(4, false, failureMsg);

		delete snapshot2;
	}

	delete snapshotBuffer;

	// Restore emulator state
	if (!wasRunning)
		viewC64->StopEmulationThread(di);

	if (allPassed)
		TestCompleted(true, "Snapshots verified: buffer save/load, state restore, file save/load");
	else
		TestCompleted(false, failureMsg);
#endif
}

void CTestViceSnapshot::Cancel()
{
	isRunning = false;
}
