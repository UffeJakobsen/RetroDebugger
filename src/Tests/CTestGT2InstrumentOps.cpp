#include "CTestGT2InstrumentOps.h"
#include "CGT2Favorites.h"
#include "C64DebuggerPluginGoatTracker.h"
#include "CConfigStorageHjson.h"
#include "CViewC64.h"
#include "SYS_Main.h"
#include <cstdio>
#include <cstring>

extern CViewC64 *viewC64;

extern "C" {
#include "gcommon.h"
#include "gsong.h"
#include "ginstr.h"
#include "gtable.h"
#include "ginstrops.h"
}

void CTestGT2InstrumentOps::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;
	int step = 0;

	// The operations exercised here (insertinstrument, deleteinstrument,
	// deleteunusedinstruments, freeinstrtable_partial) scan the WHOLE song:
	// all MAX_PATT patterns and all MAX_INSTR instrument slots. The per-subtest
	// save/restore below only snapshots pattern[0] and the instruments it sets,
	// so residue left in other patterns/instruments by earlier GT2 tests (e.g.
	// CTestGT2Patterns' note-entry steps) leaks in and breaks results — most
	// visibly deleteunusedinstruments, which then sees stray references and
	// deletes nothing. Snapshot the full song state once, reset to an empty
	// clean-room baseline, and restore it on every exit so this test is
	// deterministic regardless of what ran before it.
	static unsigned char savedAllPattern[MAX_PATT][MAX_PATTROWS * 4 + 4];
	static int savedAllPattlen[MAX_PATT];
	static INSTR savedAllInstr[MAX_INSTR];
	static unsigned char savedAllLtable[MAX_TABLES][MAX_TABLELEN];
	static unsigned char savedAllRtable[MAX_TABLES][MAX_TABLELEN];
	memcpy(savedAllPattern, pattern, sizeof(savedAllPattern));
	memcpy(savedAllPattlen, pattlen, sizeof(savedAllPattlen));
	memcpy(savedAllInstr, ginstr, sizeof(savedAllInstr));
	memcpy(savedAllLtable, ltable, sizeof(savedAllLtable));
	memcpy(savedAllRtable, rtable, sizeof(savedAllRtable));

	auto restoreAll = [&]() {
		memcpy(pattern, savedAllPattern, sizeof(savedAllPattern));
		memcpy(pattlen, savedAllPattlen, sizeof(savedAllPattlen));
		memcpy(ginstr, savedAllInstr, sizeof(savedAllInstr));
		memcpy(ltable, savedAllLtable, sizeof(savedAllLtable));
		memcpy(rtable, savedAllRtable, sizeof(savedAllRtable));
		countpatternlengths();
	};

	for (int c = 0; c < MAX_PATT; c++) clearpattern(c);
	for (int c = 1; c < MAX_INSTR; c++) clearinstr(c);
	countpatternlengths();

	// --- Test 1: freeinstrtable_partial keeps rows shared with another instrument ---
	step++;
	{
		// Save full table + instrument state.
		unsigned char savedL[MAX_TABLES][MAX_TABLELEN];
		unsigned char savedR[MAX_TABLES][MAX_TABLELEN];
		memcpy(savedL, ltable, sizeof(savedL));
		memcpy(savedR, rtable, sizeof(savedR));
		INSTR savedA, savedB;
		memcpy(&savedA, &ginstr[1], sizeof(INSTR));
		memcpy(&savedB, &ginstr[2], sizeof(INSTR));

		// Build a wavetable region rows 0..5 (1-based pos 1..6), terminator at row 5.
		for (int i = 0; i < 6; i++) { ltable[WTBL][i] = 0x10 + i; rtable[WTBL][i] = 0x00; }
		ltable[WTBL][5] = 0xff; rtable[WTBL][5] = 0x01;     // jump-back terminator
		// Instrument 1 owns rows 0..5, instrument 2 owns rows 2..5.
		memset(&ginstr[1], 0, sizeof(INSTR)); ginstr[1].ptr[WTBL] = 1; // pos 0
		memset(&ginstr[2], 0, sizeof(INSTR)); ginstr[2].ptr[WTBL] = 3; // pos 2

		unsigned char row2before = ltable[WTBL][2];
		freeinstrtable_partial(1);

		// Rows 0,1 (uniquely instr1) gone; rows 2..5 kept; instr2 ptr now 1 (pos 0).
		bool ok = (ginstr[2].ptr[WTBL] == 1) && (ltable[WTBL][0] == row2before);
		char msg[160];
		snprintf(msg, sizeof(msg),
			"partial free: instr2 ptr=%d (expect 1), ltable[0]=0x%02X (expect 0x%02X)",
			ginstr[2].ptr[WTBL], ltable[WTBL][0], row2before);
		StepCompleted(step, ok, msg);

		memcpy(ltable, savedL, sizeof(savedL));
		memcpy(rtable, savedR, sizeof(savedR));
		memcpy(&ginstr[1], &savedA, sizeof(INSTR));
		memcpy(&ginstr[2], &savedB, sizeof(INSTR));
		if (!ok) { restoreAll(); TestCompleted(false, msg); return; }
	}

	// --- Test 2: insertinstrument shifts instruments and renumbers patterns ---
	step++;
	{
		INSTR savedInstr[MAX_INSTR];
		memcpy(savedInstr, ginstr, sizeof(savedInstr));
		unsigned char savedPat[16];
		memcpy(savedPat, pattern[0], sizeof(savedPat));

		// instr 5 marked, instr 6 marked; a pattern cell references instr 5.
		memset(&ginstr[5], 0, sizeof(INSTR)); ginstr[5].ad = 0x55;
		memset(&ginstr[6], 0, sizeof(INSTR)); ginstr[6].ad = 0x66;
		pattern[0][1] = 5;   // row 0 instrument column

		int rc = insertinstrument(5);

		// instr 5 now empty, old 5 is at 6 (ad 0x55), old 6 at 7 (ad 0x66),
		// pattern ref 5 -> 6.
		bool ok = rc && (ginstr[5].ad == 0x00) && (ginstr[6].ad == 0x55) &&
		          (ginstr[7].ad == 0x66) && (pattern[0][1] == 6);
		char msg[160];
		snprintf(msg, sizeof(msg),
			"insert@5: rc=%d g5=%02X g6=%02X g7=%02X patref=%d",
			rc, ginstr[5].ad, ginstr[6].ad, ginstr[7].ad, pattern[0][1]);
		StepCompleted(step, ok, msg);

		memcpy(ginstr, savedInstr, sizeof(savedInstr));
		memcpy(pattern[0], savedPat, sizeof(savedPat));
		countpatternlengths();
		if (!ok) { restoreAll(); TestCompleted(false, msg); return; }
	}

	// --- Test 3: insertinstrument refuses when last slot is occupied ---
	step++;
	{
		INSTR savedInstr[MAX_INSTR];
		memcpy(savedInstr, ginstr, sizeof(savedInstr));

		memset(&ginstr[GT2_LAST_INSTR], 0, sizeof(INSTR));
		ginstr[GT2_LAST_INSTR].ad = 0x99;       // last slot holds data

		int rc = insertinstrument(5);
		bool ok = (rc == 0) && (ginstr[GT2_LAST_INSTR].ad == 0x99);
		char msg[128];
		snprintf(msg, sizeof(msg), "insert refused: rc=%d (expect 0)", rc);
		StepCompleted(step, ok, msg);

		memcpy(ginstr, savedInstr, sizeof(savedInstr));
		if (!ok) { restoreAll(); TestCompleted(false, msg); return; }
	}

	// --- Test 4: deleteinstrument shifts instruments down and renumbers ---
	step++;
	{
		INSTR savedInstr[MAX_INSTR];
		memcpy(savedInstr, ginstr, sizeof(savedInstr));
		unsigned char savedPat[16];
		memcpy(savedPat, pattern[0], sizeof(savedPat));

		memset(&ginstr[5], 0, sizeof(INSTR)); ginstr[5].ad = 0x55;
		memset(&ginstr[6], 0, sizeof(INSTR)); ginstr[6].ad = 0x66;
		memset(&ginstr[7], 0, sizeof(INSTR)); ginstr[7].ad = 0x77;
		pattern[0][1] = 7;

		deleteinstrument(5);

		// old 6 -> 5, old 7 -> 6, pattern ref 7 -> 6.
		bool ok = (ginstr[5].ad == 0x66) && (ginstr[6].ad == 0x77) &&
		          (pattern[0][1] == 6);
		char msg[160];
		snprintf(msg, sizeof(msg),
			"delete@5: g5=%02X g6=%02X patref=%d",
			ginstr[5].ad, ginstr[6].ad, pattern[0][1]);
		StepCompleted(step, ok, msg);

		memcpy(ginstr, savedInstr, sizeof(savedInstr));
		memcpy(pattern[0], savedPat, sizeof(savedPat));
		countpatternlengths();
		if (!ok) { restoreAll(); TestCompleted(false, msg); return; }
	}

	// --- Test 5: deleteunusedinstruments removes only unreferenced ones ---
	step++;
	{
		INSTR savedInstr[MAX_INSTR];
		memcpy(savedInstr, ginstr, sizeof(savedInstr));
		unsigned char savedPat[16];
		memcpy(savedPat, pattern[0], sizeof(savedPat));

		for (int i = 1; i <= 4; i++) { memset(&ginstr[i], 0, sizeof(INSTR)); ginstr[i].ad = 0x10 + i; }
		// only instr 2 and instr 4 are referenced
		pattern[0][1] = 2;
		pattern[0][5] = 4;

		deleteunusedinstruments();

		// instr 1 and 3 (unused) removed; old 2 -> 1, old 4 -> 2.
		bool ok = (ginstr[1].ad == 0x12) && (ginstr[2].ad == 0x14) &&
		          (pattern[0][1] == 1) && (pattern[0][5] == 2);
		char msg[160];
		snprintf(msg, sizeof(msg),
			"delete-unused: g1=%02X g2=%02X ref0=%d ref1=%d",
			ginstr[1].ad, ginstr[2].ad, pattern[0][1], pattern[0][5]);
		StepCompleted(step, ok, msg);

		memcpy(ginstr, savedInstr, sizeof(savedInstr));
		memcpy(pattern[0], savedPat, sizeof(savedPat));
		countpatternlengths();
		if (!ok) { restoreAll(); TestCompleted(false, msg); return; }
	}

	// --- Test 6: capture then apply yields an independent instrument ---
	step++;
	{
		unsigned char savedL[MAX_TABLES][MAX_TABLELEN];
		unsigned char savedR[MAX_TABLES][MAX_TABLELEN];
		memcpy(savedL, ltable, sizeof(savedL));
		memcpy(savedR, rtable, sizeof(savedR));
		INSTR savedA, savedB;
		memcpy(&savedA, &ginstr[1], sizeof(INSTR));
		memcpy(&savedB, &ginstr[2], sizeof(INSTR));

		// Source instr 1 with a 3-row wavetable at pos 0.
		memset(ltable, 0, sizeof(savedL));
		memset(rtable, 0, sizeof(savedR));
		ltable[WTBL][0] = 0x11; ltable[WTBL][1] = 0x12;
		ltable[WTBL][2] = 0xff; rtable[WTBL][2] = 0x01;   // jump to row 0 (pos 1)
		memset(&ginstr[1], 0, sizeof(INSTR));
		ginstr[1].ad = 0xA1; ginstr[1].ptr[WTBL] = 1;
		memset(&ginstr[2], 0, sizeof(INSTR));

		static INSTRPACKAGE pkg;
		instrpackage_capture(1, &pkg);
		instrpackage_apply(2, &pkg);

		// instr 2 got ad and an independent wavetable slice (ptr != 1).
		int p2 = ginstr[2].ptr[WTBL] - 1;
		bool ok = (ginstr[2].ad == 0xA1) && (ginstr[2].ptr[WTBL] != 0) &&
		          (ginstr[2].ptr[WTBL] != 1) &&
		          (ltable[WTBL][p2] == 0x11) &&
		          (ltable[WTBL][p2+2] == 0xff) &&
		          (rtable[WTBL][p2+2] == (unsigned char)(p2 + 1)); // jump rebased
		char msg[160];
		snprintf(msg, sizeof(msg),
			"pkg apply: g2.ad=%02X ptr=%d l0=%02X jump=%02X",
			ginstr[2].ad, ginstr[2].ptr[WTBL], ltable[WTBL][p2],
			rtable[WTBL][p2+2]);
		StepCompleted(step, ok, msg);

		memcpy(ltable, savedL, sizeof(savedL));
		memcpy(rtable, savedR, sizeof(savedR));
		memcpy(&ginstr[1], &savedA, sizeof(INSTR));
		memcpy(&ginstr[2], &savedB, sizeof(INSTR));
		if (!ok) { restoreAll(); TestCompleted(false, msg); return; }
	}

	// --- Test 7: favorites add -> save -> reload -> apply round-trips ---
	step++;
	{
		INSTR savedA, savedB;
		memcpy(&savedA, &ginstr[1], sizeof(INSTR));
		memcpy(&savedB, &ginstr[2], sizeof(INSTR));

		memset(&ginstr[1], 0, sizeof(INSTR));
		ginstr[1].ad = 0xB7; ginstr[1].sr = 0xC3;
		strcpy(ginstr[1].name, "FAVTEST");

		CGT2Favorites fav;
		fav.entries.clear();
		fav.AddFromInstrument(1);
		fav.Save();

		CGT2Favorites fav2;
		fav2.Load();
		bool found = false;
		for (size_t i = 0; i < fav2.entries.size(); i++)
		{
			if (fav2.entries[i].displayName == "FAVTEST")
			{
				fav2.ApplyTo(2, (int)i);
				found = true;
				break;
			}
		}
		bool ok = found && (ginstr[2].ad == 0xB7) && (ginstr[2].sr == 0xC3);
		char msg[160];
		snprintf(msg, sizeof(msg), "favorites round-trip: found=%d g2.ad=%02X g2.sr=%02X",
			(int)found, ginstr[2].ad, ginstr[2].sr);
		StepCompleted(step, ok, msg);

		memcpy(&ginstr[1], &savedA, sizeof(INSTR));
		memcpy(&ginstr[2], &savedB, sizeof(INSTR));
		if (!ok) { restoreAll(); TestCompleted(false, msg); return; }
	}

	// --- Test 8: settings migration moves a key into gt2Config ---
	step++;
	{
		bool ok = true;
		char msg[160];
		if (pluginGoatTracker && pluginGoatTracker->gt2Config && viewC64 && viewC64->config)
		{
			// gt2Config must hold GT2ArpColumns and the global must not.
			ok = pluginGoatTracker->gt2Config->E_x_i_s_t_s("GT2ArpColumns")
			  && !viewC64->config->E_x_i_s_t_s("GT2ArpColumns");
			snprintf(msg, sizeof(msg), "migration: gt2 has key=%d, global has key=%d",
				(int)pluginGoatTracker->gt2Config->E_x_i_s_t_s("GT2ArpColumns"),
				(int)viewC64->config->E_x_i_s_t_s("GT2ArpColumns"));
		}
		else { snprintf(msg, sizeof(msg), "migration: plugin/config not ready - skipped"); }
		StepCompleted(step, ok, msg);
		if (!ok) { restoreAll(); TestCompleted(false, msg); return; }
	}

	restoreAll();
	TestCompleted(true, "All GT2InstrumentOps tests passed");
}
