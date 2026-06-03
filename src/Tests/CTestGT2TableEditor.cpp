#include "CTestGT2TableEditor.h"
#include "SYS_Main.h"
#include <cstdio>
#include <cstring>

extern "C" {
#include "gcommon.h"
#include "gsong.h"
#include "gtable.h"
}

void CTestGT2TableEditor::Run(ITestCallback *cb)
{
	this->callback = cb;
	this->isRunning = true;
	this->currentStep = 0;
	int step = 0;

	// --- Test 1: inserttable shifts a later instrument's pointer +1 ---
	step++;
	{
		unsigned char savedL[MAX_TABLES][MAX_TABLELEN];
		unsigned char savedR[MAX_TABLES][MAX_TABLELEN];
		memcpy(savedL, ltable, sizeof(savedL));
		memcpy(savedR, rtable, sizeof(savedR));
		INSTR savedA, savedB;
		memcpy(&savedA, &ginstr[1], sizeof(INSTR));
		memcpy(&savedB, &ginstr[2], sizeof(INSTR));

		memset(ltable, 0, sizeof(savedL));
		memset(rtable, 0, sizeof(savedR));
		for (int i = 0; i < 8; i++) ltable[WTBL][i] = 0x10 + i;
		memset(&ginstr[1], 0, sizeof(INSTR)); ginstr[1].ptr[WTBL] = 1; // pos 0
		memset(&ginstr[2], 0, sizeof(INSTR)); ginstr[2].ptr[WTBL] = 5; // pos 4

		inserttable(WTBL, 2, 1);    // insert one row inside instr1's slice

		// instr2 pointer (was 5) must shift to 6.
		bool ok = (ginstr[2].ptr[WTBL] == 6) && (ginstr[1].ptr[WTBL] == 1);
		char msg[160];
		snprintf(msg, sizeof(msg), "inserttable: g1.ptr=%d (1) g2.ptr=%d (6)",
			ginstr[1].ptr[WTBL], ginstr[2].ptr[WTBL]);
		StepCompleted(step, ok, msg);

		memcpy(ltable, savedL, sizeof(savedL));
		memcpy(rtable, savedR, sizeof(savedR));
		memcpy(&ginstr[1], &savedA, sizeof(INSTR));
		memcpy(&ginstr[2], &savedB, sizeof(INSTR));
		if (!ok) { TestCompleted(false, msg); return; }
	}

	// --- Test 2: deletetable shifts a later instrument's pointer -1 ---
	step++;
	{
		unsigned char savedL[MAX_TABLES][MAX_TABLELEN];
		unsigned char savedR[MAX_TABLES][MAX_TABLELEN];
		memcpy(savedL, ltable, sizeof(savedL));
		memcpy(savedR, rtable, sizeof(savedR));
		INSTR savedA, savedB;
		memcpy(&savedA, &ginstr[1], sizeof(INSTR));
		memcpy(&savedB, &ginstr[2], sizeof(INSTR));

		memset(ltable, 0, sizeof(savedL));
		memset(rtable, 0, sizeof(savedR));
		for (int i = 0; i < 8; i++) ltable[WTBL][i] = 0x10 + i;
		memset(&ginstr[1], 0, sizeof(INSTR)); ginstr[1].ptr[WTBL] = 1; // pos 0
		memset(&ginstr[2], 0, sizeof(INSTR)); ginstr[2].ptr[WTBL] = 5; // pos 4

		deletetable(WTBL, 2);       // delete one row inside instr1's slice

		bool ok = (ginstr[2].ptr[WTBL] == 4) && (ginstr[1].ptr[WTBL] == 1);
		char msg[160];
		snprintf(msg, sizeof(msg), "deletetable: g1.ptr=%d (1) g2.ptr=%d (4)",
			ginstr[1].ptr[WTBL], ginstr[2].ptr[WTBL]);
		StepCompleted(step, ok, msg);

		memcpy(ltable, savedL, sizeof(savedL));
		memcpy(rtable, savedR, sizeof(savedR));
		memcpy(&ginstr[1], &savedA, sizeof(INSTR));
		memcpy(&ginstr[2], &savedB, sizeof(INSTR));
		if (!ok) { TestCompleted(false, msg); return; }
	}

	TestCompleted(true, "All GT2TableEditor tests passed");
}
