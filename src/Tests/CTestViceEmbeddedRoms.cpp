#include "CTestViceEmbeddedRoms.h"
#include "EmulatorsConfig.h"
#include "CViewC64.h"
#include "CDebugInterfaceC64.h"
#include "CDebugInterfaceVice.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include "DebuggerDefs.h"
#include <cstdio>
#include <cstring>

extern "C"
{
#include "ViceWrapper.h"
};

static char failureMsg[512];

void CTestViceEmbeddedRoms::Run(ITestCallback *cb)
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

	di->PauseEmulationBlockedWait();

	bool allPassed = true;

	// --- Step 1: KERNAL ROM signature ---
	// With default banking ($01=$37), $E000-$FFFF shows KERNAL ROM
	// Reset vector at $FFFC/$FFFD should point to KERNAL cold start
	{
		di->SetByteToRamC64(0x0001, 0x37);

		u8 resetLo = di->GetByteC64(0xFFFC);
		u8 resetHi = di->GetByteC64(0xFFFD);
		u16 resetVec = resetLo | (resetHi << 8);

		// C64 KERNAL reset vector is $FCE2
		if (resetVec != 0xFCE2)
		{
			sprintf(failureMsg, "KERNAL reset vector: $%04X (expected $FCE2)", resetVec);
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "KERNAL ROM: reset vector=$%04X (correct)", resetVec);
			StepCompleted(1, true, msg);
		}
		else
		{
			StepCompleted(1, false, failureMsg);
		}
	}

	// --- Step 2: BASIC ROM signature ---
	// $A000-$BFFF with $01=$37 shows BASIC ROM
	// BASIC cold start entry at $A000 is a jump (4C xx xx)
	if (allPassed)
	{
		u8 basicFirst = di->GetByteC64(0xA000);

		// First byte of BASIC ROM is $94 (the word pointer to the first BASIC routine)
		// Actually the BASIC ROM starts with $94 $E3 $7B $A4 (the cold/warm start vectors)
		// Just verify it reads differently than our RAM write
		di->SetByteToRamC64(0xA000, 0x11);
		u8 romVal = di->GetByteC64(0xA000);

		if (romVal == 0x11)
		{
			sprintf(failureMsg, "BASIC ROM: reads RAM value $11 at $A000, ROM not mapped");
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "BASIC ROM: $A000=$%02X (ROM mapped correctly)", romVal);
			StepCompleted(2, true, msg);
		}
		else
		{
			StepCompleted(2, false, failureMsg);
		}
	}

	// --- Step 3: Character ROM via API ---
	if (allPassed)
	{
		u8 *charRom = di->GetCharRom();
		if (!charRom)
		{
			sprintf(failureMsg, "GetCharRom() returned NULL");
			allPassed = false;
		}
		else
		{
			// First character in chargen is '@' (PETSCII $00)
			// Known pattern: byte 0=$3C, 1=$66, 2=$6E (top rows of '@')
			if (charRom[0] != 0x3C || charRom[1] != 0x66 || charRom[2] != 0x6E)
			{
				sprintf(failureMsg, "GetCharRom(): [0]=$%02X,[1]=$%02X,[2]=$%02X (expected $3C,$66,$6E)",
						charRom[0], charRom[1], charRom[2]);
				allPassed = false;
			}
		}

		if (allPassed)
			StepCompleted(3, true, "Character ROM verified via GetCharRom() API");
		else
			StepCompleted(3, false, failureMsg);
	}

	// --- Step 4: Palette data ---
	if (allPassed)
	{
		// Palette arrays should be populated with non-zero values
		bool hasNonZero = false;
		for (int i = 1; i < 16; i++)  // skip color 0 (black, which is 0)
		{
			if (c64d_palette_red[i] != 0 || c64d_palette_green[i] != 0 || c64d_palette_blue[i] != 0)
			{
				hasNonZero = true;
				break;
			}
		}

		if (!hasNonZero)
		{
			sprintf(failureMsg, "Palette: all non-black colors are zero");
			allPassed = false;
		}

		// Color 1 (white) should have high values
		if (allPassed)
		{
			if (c64d_palette_red[1] < 200 || c64d_palette_green[1] < 200 || c64d_palette_blue[1] < 200)
			{
				sprintf(failureMsg, "Palette: white (1) = R%d G%d B%d (expected >200)",
						c64d_palette_red[1], c64d_palette_green[1], c64d_palette_blue[1]);
				allPassed = false;
			}
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "Palette: white=(%d,%d,%d), valid colors present",
					c64d_palette_red[1], c64d_palette_green[1], c64d_palette_blue[1]);
			StepCompleted(4, true, msg);
		}
		else
		{
			StepCompleted(4, false, failureMsg);
		}
	}

	// --- Step 5: Drive ROM signature ---
	if (allPassed)
	{
		// 1541 drive ROM reset vector at $FFFC/$FFFD in drive address space
		u8 drvResetLo = di->GetByte1541(0xFFFC);
		u8 drvResetHi = di->GetByte1541(0xFFFD);
		u16 drvResetVec = drvResetLo | (drvResetHi << 8);

		// 1541 reset vector is $EAA0
		if (drvResetVec != 0xEAA0)
		{
			sprintf(failureMsg, "Drive ROM: reset vector=$%04X (expected $EAA0)", drvResetVec);
			allPassed = false;
		}

		if (allPassed)
		{
			char msg[128];
			sprintf(msg, "Drive ROM: reset vector=$%04X (correct)", drvResetVec);
			StepCompleted(5, true, msg);
		}
		else
		{
			StepCompleted(5, false, failureMsg);
		}
	}

	// Restore emulator state
	if (!wasRunning)
		viewC64->StopEmulationThread(di);

	if (allPassed)
		TestCompleted(true, "Embedded ROMs verified: KERNAL, BASIC, chargen, char API, palette, drive ROM");
	else
		TestCompleted(false, failureMsg);
#endif
}

void CTestViceEmbeddedRoms::Cancel()
{
	isRunning = false;
}
