#include "CTestViceMemoryAccess.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include "DebuggerDefs.h"
#include <cstdio>
#include <cstring>

static char failureMsg[512];

void CTestViceMemoryAccess::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;
	failureMsg[0] = '\0';

#ifndef RUN_COMMODORE64
	TestCompleted(true, "Skipped (C64 not enabled)");
	return;
#else
	CDebugInterfaceC64 *di = (CDebugInterfaceC64 *)viewC64->debugInterfaceC64;
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

	di->PauseEmulationBlockedWait();

	bool allPassed = true;

	// --- Step 1: RAM read/write round-trip ---
	{
		u8 testValues[] = { 0x00, 0xFF, 0x42, 0xA5 };
		for (int i = 0; i < 4; i++)
		{
			u16 addr = 0x0800 + i;
			di->SetByteToRamC64(addr, testValues[i]);
			u8 readBack = di->GetByteFromRamC64(addr);
			if (readBack != testValues[i])
			{
				sprintf(failureMsg, "RAM write/read at $%04X: wrote $%02X, read $%02X", addr, testValues[i], readBack);
				allPassed = false;
				break;
			}
		}

		if (allPassed)
			StepCompleted(1, true, "RAM read/write round-trip verified");
		else
			StepCompleted(1, false, failureMsg);
	}

	// --- Step 2: Multiple write/read round-trips ---
	if (allPassed)
	{
		// Write a pattern across a range and verify each byte individually
		for (int i = 0; i < 16; i++)
		{
			di->SetByteToRamC64(0x0900 + i, (u8)(0xB0 + i));
		}

		for (int i = 0; i < 16; i++)
		{
			u8 expected = (u8)(0xB0 + i);
			u8 readBack = di->GetByteFromRamC64(0x0900 + i);
			if (readBack != expected)
			{
				sprintf(failureMsg, "Multi-byte R/W: $%04X expected $%02X, got $%02X", 0x0900 + i, expected, readBack);
				allPassed = false;
				break;
			}
		}

		if (allPassed)
			StepCompleted(2, true, "Multi-byte write/read verified (16 bytes with unique pattern)");
		else
			StepCompleted(2, false, failureMsg);
	}

	// --- Step 3: Banking - ROM vs RAM access paths ---
	if (allPassed)
	{
		// Write known pattern to RAM underneath ROMs
		di->SetByteToRamC64(0xA000, 0x11);  // Under BASIC ROM
		di->SetByteToRamC64(0xE000, 0x22);  // Under KERNAL ROM

		// Direct RAM read should return what we wrote
		u8 ramAtA000 = di->GetByteFromRamC64(0xA000);
		u8 ramAtE000 = di->GetByteFromRamC64(0xE000);

		if (ramAtA000 != 0x11)
		{
			sprintf(failureMsg, "Direct RAM: $A000=$%02X (expected $11)", ramAtA000);
			allPassed = false;
		}
		else if (ramAtE000 != 0x22)
		{
			sprintf(failureMsg, "Direct RAM: $E000=$%02X (expected $22)", ramAtE000);
			allPassed = false;
		}

		// Banked read (GetByteC64) with default banking ($01=$37) should see ROM
		if (allPassed)
		{
			u8 bankedA000 = di->GetByteC64(0xA000);
			u8 bankedE000 = di->GetByteC64(0xE000);

			// ROM values should differ from our RAM values
			if (bankedA000 == 0x11)
			{
				sprintf(failureMsg, "Banked read: $A000 returns RAM value $11 (expected BASIC ROM)");
				allPassed = false;
			}
			else if (bankedE000 == 0x22)
			{
				sprintf(failureMsg, "Banked read: $E000 returns RAM value $22 (expected KERNAL ROM)");
				allPassed = false;
			}
		}

		if (allPassed)
			StepCompleted(3, true, "Banking verified: direct RAM vs banked read differ at ROM addresses");
		else
			StepCompleted(3, false, failureMsg);
	}

	// --- Step 4: Data adapter access ---
	if (allPassed)
	{
		di->SetByteToRamC64(0x0A00, 0x77);

		u8 val = 0;
		di->dataAdapterC64DirectRam->AdapterReadByte(0x0A00, &val);

		if (val != 0x77)
		{
			sprintf(failureMsg, "DataAdapter: read $%02X at $0A00, expected $77", val);
			allPassed = false;
		}

		// Write via adapter, read via direct API
		if (allPassed)
		{
			di->dataAdapterC64DirectRam->AdapterWriteByte(0x0A01, 0x88);
			u8 val2 = di->GetByteFromRamC64(0x0A01);
			if (val2 != 0x88)
			{
				sprintf(failureMsg, "DataAdapter write: $0A01=$%02X, expected $88", val2);
				allPassed = false;
			}
		}

		if (allPassed)
			StepCompleted(4, true, "Data adapter read/write verified");
		else
			StepCompleted(4, false, failureMsg);
	}

	// --- Step 5: Color RAM access ---
	if (allPassed)
	{
		// Color RAM is at $D800-$DBFF, only low nibble significant
		di->SetByteC64(0xD800, 0x05);
		u8 colorVal = di->GetByteC64(0xD800);

		// Color RAM only stores low nibble
		if ((colorVal & 0x0F) != 0x05)
		{
			sprintf(failureMsg, "Color RAM: $D800=$%02X, expected low nibble $05", colorVal);
			allPassed = false;
		}

		if (allPassed)
			StepCompleted(5, true, "Color RAM access verified");
		else
			StepCompleted(5, false, failureMsg);
	}

	// Restore emulator state
	if (!wasRunning)
		viewC64->StopEmulationThread(di);

	if (allPassed)
		TestCompleted(true, "Memory access verified: RAM, bulk ops, banking, adapters, color RAM");
	else
		TestCompleted(false, failureMsg);
#endif
}

void CTestViceMemoryAccess::Cancel()
{
	isRunning = false;
}
