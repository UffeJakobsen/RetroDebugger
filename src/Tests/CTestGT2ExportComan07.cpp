#include "CTestGT2ExportComan07.h"
#include "C64DebuggerPluginGoatTracker.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include "DBG_Log.h"
#include <cstdio>
#include <cstring>

extern "C" {
#include "goattrk2.h"
#include "greloc.h"
#include "bme_io.h"
}

void CTestGT2ExportComan07::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;

	int step = 0;

	// 1. Init plugin + linked datafile so player.s is reachable.
	step++;
	if (pluginGoatTracker == NULL)
		PLUGIN_GoatTrackerInit();
	{
		extern unsigned char datafile[];
		io_openlinkeddatafile(datafile);
	}
	SYS_Sleep(500);

	if (pluginGoatTracker == NULL)
	{
		StepCompleted(step, false, "GT2 plugin failed to initialize");
		TestCompleted(false, "Plugin init failed");
		return;
	}
	{
		int h = io_open((char*)"player.s");
		if (h < 0)
		{
			StepCompleted(step, false, "player.s NOT in datafile");
			TestCompleted(false, "GT2 datafile not ready");
			return;
		}
		io_close(h);
	}
	StepCompleted(step, true, "GT2 plugin + datafile ready");

	// 2. Load coman07.sng.
	step++;
	const char *songPath = "tests/data/gt2/coman07.sng";
	FILE *f = fopen(songPath, "rb");
	if (!f)
	{
		// Fallback path in case CWD is the build dir.
		songPath = "../tests/data/gt2/coman07.sng";
		f = fopen(songPath, "rb");
	}
	if (!f)
	{
		StepCompleted(step, false, "coman07.sng not found in tests/data/gt2/");
		TestCompleted(false, "Missing test asset");
		return;
	}
	fclose(f);

	pluginGoatTracker->LoadSongFromFile(songPath);
	SYS_Sleep(500);

	if (strlen(loadedsongfilename) == 0)
	{
		StepCompleted(step, false, "loadsong did not set loadedsongfilename");
		TestCompleted(false, "Load reported failure");
		return;
	}
	StepCompleted(step, true, "coman07.sng loaded");

	// 3. Export to a throwaway path. Same defaults the editor uses.
	step++;
	playeradr     = 0x1000;
	zeropageadr   = 0xFB;
	fileformat    = FORMAT_PRG;
	playerversion = PLAYER_BUFFERED;

	const char *outputPath = "/tmp/coman07_test_export.prg";
	char errorMsg[256] = {0};

	LOGD("CTestGT2ExportComan07: relocator_export to %s "
	     "(playeradr=%04X zeropageadr=%02X fmt=%d ver=%u)",
	     outputPath, playeradr, zeropageadr, fileformat, playerversion);

	unsigned long t0 = SYS_GetCurrentTimeInMillis();
	int result = relocator_export(outputPath, errorMsg, sizeof errorMsg);
	unsigned long elapsed = SYS_GetCurrentTimeInMillis() - t0;

	if (result == 0)
	{
		char msg[256];
		snprintf(msg, sizeof msg, "Export OK (%lu ms)", elapsed);
		StepCompleted(step, true, msg);
		TestCompleted(true, "coman07.sng exported cleanly");
		return;
	}

	// Failure path: surface result code AND the verbatim error message.
	// errorMsg holds the assembler diagnostic, pool-overflow text, or
	// whatever relocator wrote — that's exactly what the user sees as
	// "Assembly failed." in the dialog.
	const char *errBody = (errorMsg[0] != 0) ? errorMsg : "(no error message returned)";
	char fail[512];
	snprintf(fail, sizeof fail,
	         "export returned %d after %lu ms: %s",
	         result, elapsed, errBody);
	StepCompleted(step, false, fail);
	TestCompleted(false, fail);
}
