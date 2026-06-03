#include "CTestGoatTrackerExport.h"
#include "CViewC64.h"
#include "C64DebuggerPluginGoatTracker.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include <cstdio>
#include <cstring>

extern "C" {
#include "goattrk2.h"
#include "greloc.h"
#include "bme_io.h"
}

void CTestGoatTrackerExport::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;

	int step = 0;
	bool allPassed = true;

	// Step 1: Init GoatTracker plugin (may already be initialized from startup restore)
	step++;
	if (pluginGoatTracker == NULL)
	{
		PLUGIN_GoatTrackerInit();
	}
	// In headless mode the GT2 thread blocks on renderMutex for a long time.
	// Ensure the linked datafile is open on our thread so io_open("player.s") works.
	{
		extern unsigned char datafile[];
		io_openlinkeddatafile(datafile);
	}
	SYS_Sleep(500);

	if (pluginGoatTracker != NULL)
	{
		StepCompleted(step, true, "GoatTracker plugin initialized");
	}
	else
	{
		StepCompleted(step, false, "GoatTracker plugin is NULL");
		TestCompleted(false, "Plugin init failed");
		return;
	}

	// Step 1b: Verify datafile is ready (player.s accessible)
	step++;
	{
		int testHandle = io_open((char*)"player.s");
		if (testHandle >= 0)
		{
			io_close(testHandle);
			StepCompleted(step, true, "player.s accessible in linked datafile");
		}
		else
		{
			StepCompleted(step, false, "player.s NOT found - GT2 datafile not initialized");
			TestCompleted(false, "GT2 datafile not ready");
			return;
		}
	}

	// Step 2: Load test song
	step++;
	const char *songPath = "tests/data/gt2/coman02.sng";
	FILE *f = fopen(songPath, "rb");
	if (!f)
	{
		StepCompleted(step, false, "Test song file not found");
		TestCompleted(false, "Missing test data");
		return;
	}
	fclose(f);

	pluginGoatTracker->LoadSongFromFile(songPath);
	SYS_Sleep(500);

	if (strlen(loadedsongfilename) > 0)
	{
		char msg[256];
		snprintf(msg, sizeof(msg), "Song loaded: %s", loadedsongfilename);
		StepCompleted(step, true, msg);
	}
	else
	{
		StepCompleted(step, false, "Song not loaded (loadedsongfilename empty)");
		allPassed = false;
	}

	// Step 3: Set export options and try export to SID
	step++;
	playeradr = 0x1000;
	zeropageadr = 0xFB;
	fileformat = FORMAT_SID;
	playerversion = PLAYER_BUFFERED;

	const char *outputPath = "tests/data/gt2/coman02_test_export.sid";
	char errorMsg[256] = {0};

	LOGD("CTestGoatTrackerExport: calling relocator_export to '%s'", outputPath);
	unsigned long startTime = SYS_GetCurrentTimeInMillis();
	LOGD("CTestGoatTrackerExport: playeradr=%04X zeropageadr=%02X fileformat=%d playerversion=%u",
		 playeradr, zeropageadr, fileformat, playerversion);
	int result = relocator_export(outputPath, errorMsg, sizeof(errorMsg));
	unsigned long elapsed = SYS_GetCurrentTimeInMillis() - startTime;

	char resultMsg[512];
	if (result == 0)
	{
		snprintf(resultMsg, sizeof(resultMsg), "Export OK in %lu ms", elapsed);
		StepCompleted(step, true, resultMsg);

		// Verify output file exists and has content
		step++;
		FILE *out = fopen(outputPath, "rb");
		if (out)
		{
			fseek(out, 0, SEEK_END);
			long size = ftell(out);
			fclose(out);
			// Clean up test output
			remove(outputPath);

			snprintf(resultMsg, sizeof(resultMsg), "Output file size: %ld bytes", size);
			StepCompleted(step, size > 0, resultMsg);
			if (size <= 0) allPassed = false;
		}
		else
		{
			StepCompleted(step, false, "Output file not created");
			allPassed = false;
		}
	}
	else
	{
		snprintf(resultMsg, sizeof(resultMsg), "Export failed (result=%d) after %lu ms: %s", result, elapsed, errorMsg);
		StepCompleted(step, false, resultMsg);
		allPassed = false;
		// Clean up partial output
		remove(outputPath);
	}

	// Step 4: Also test PRG export
	step++;
	fileformat = FORMAT_PRG;
	const char *prgPath = "tests/data/gt2/coman02_test_export.prg";
	errorMsg[0] = 0;

	LOGD("CTestGoatTrackerExport: calling relocator_export for PRG");
	startTime = SYS_GetCurrentTimeInMillis();
	result = relocator_export(prgPath, errorMsg, sizeof(errorMsg));
	elapsed = SYS_GetCurrentTimeInMillis() - startTime;

	if (result == 0)
	{
		FILE *out = fopen(prgPath, "rb");
		if (out)
		{
			fseek(out, 0, SEEK_END);
			long size = ftell(out);
			fclose(out);
			remove(prgPath);
			snprintf(resultMsg, sizeof(resultMsg), "PRG export OK: %ld bytes in %lu ms", size, elapsed);
			StepCompleted(step, size > 0, resultMsg);
			if (size <= 0) allPassed = false;
		}
		else
		{
			StepCompleted(step, false, "PRG output file not created");
			allPassed = false;
		}
	}
	else
	{
		snprintf(resultMsg, sizeof(resultMsg), "PRG export failed (result=%d) after %lu ms: %s", result, elapsed, errorMsg);
		StepCompleted(step, false, resultMsg);
		allPassed = false;
		remove(prgPath);
	}

	TestCompleted(allPassed, allPassed ? "All GoatTracker export tests passed" : "Some export tests failed");
}

void CTestGoatTrackerExport::Cancel()
{
	isRunning = false;
}
