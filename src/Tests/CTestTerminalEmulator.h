#pragma once

#include "CTest.h"

extern "C" {
#include "tmt.h"
}

#include <string>
#include <cstring>

class CTestTerminalEmulator : public CTest
{
public:
	virtual const char *GetName() override { return "TerminalEmulator"; }

	// Track if callback was called
	static bool callbackCalled;

	static void TmtCallback(tmt_msg_t msg, TMT *vt, const void *a, void *p)
	{
		callbackCalled = true;
	}

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

		// Step 1: Create TMT instance and write text, verify screen cells
		{
			callbackCalled = false;
			TMT *vt = tmt_open(25, 80, TmtCallback, NULL, NULL);
			if (!vt)
			{ TestCompleted(false, "tmt_open returned NULL"); return; }

			tmt_write(vt, "Hello", 5);

			const TMTSCREEN *s = tmt_screen(vt);
			if (!s)
			{ tmt_close(vt); TestCompleted(false, "tmt_screen returned NULL"); return; }
			if (s->nline < 25 || s->ncol < 80)
			{ tmt_close(vt); TestCompleted(false, "Screen dimensions wrong"); return; }

			// Check "Hello" at row 0, cols 0-4
			if (s->lines[0]->chars[0].c != L'H')
			{ tmt_close(vt); TestCompleted(false, "Cell 0,0 should be 'H'"); return; }
			if (s->lines[0]->chars[1].c != L'e')
			{ tmt_close(vt); TestCompleted(false, "Cell 0,1 should be 'e'"); return; }
			if (s->lines[0]->chars[2].c != L'l')
			{ tmt_close(vt); TestCompleted(false, "Cell 0,2 should be 'l'"); return; }
			if (s->lines[0]->chars[3].c != L'l')
			{ tmt_close(vt); TestCompleted(false, "Cell 0,3 should be 'l'"); return; }
			if (s->lines[0]->chars[4].c != L'o')
			{ tmt_close(vt); TestCompleted(false, "Cell 0,4 should be 'o'"); return; }

			if (!callbackCalled)
			{ tmt_close(vt); TestCompleted(false, "TMT callback was not called"); return; }

			tmt_close(vt);
		}
		StepCompleted(1, true, "Write text and verify screen cells");

		// Step 2: Cursor movement via escape sequence
		{
			TMT *vt = tmt_open(25, 80, TmtCallback, NULL, NULL);
			if (!vt)
			{ TestCompleted(false, "tmt_open returned NULL"); return; }

			// Move cursor to row 5, col 10 (1-indexed in VT100)
			tmt_write(vt, "\033[5;10H", 7);

			const TMTPOINT *c = tmt_cursor(vt);
			if (!c)
			{ tmt_close(vt); TestCompleted(false, "tmt_cursor returned NULL"); return; }
			// TMT uses 0-indexed cursor positions
			if (c->r != 4)
			{ tmt_close(vt); TestCompleted(false, ("Cursor row expected 4 (0-indexed), got " + std::to_string(c->r)).c_str()); return; }
			if (c->c != 9)
			{ tmt_close(vt); TestCompleted(false, ("Cursor col expected 9 (0-indexed), got " + std::to_string(c->c)).c_str()); return; }

			tmt_close(vt);
		}
		StepCompleted(2, true, "Cursor movement via ESC[5;10H");

		// Step 3: Color attribute via escape sequence
		{
			TMT *vt = tmt_open(25, 80, TmtCallback, NULL, NULL);
			if (!vt)
			{ TestCompleted(false, "tmt_open returned NULL"); return; }

			// Set foreground to red (31), write "Red"
			tmt_write(vt, "\033[31mRed", 8);

			const TMTSCREEN *s = tmt_screen(vt);
			if (!s)
			{ tmt_close(vt); TestCompleted(false, "tmt_screen returned NULL"); return; }

			// Verify character content
			if (s->lines[0]->chars[0].c != L'R')
			{ tmt_close(vt); TestCompleted(false, "Cell 0,0 should be 'R'"); return; }
			if (s->lines[0]->chars[1].c != L'e')
			{ tmt_close(vt); TestCompleted(false, "Cell 0,1 should be 'e'"); return; }
			if (s->lines[0]->chars[2].c != L'd')
			{ tmt_close(vt); TestCompleted(false, "Cell 0,2 should be 'd'"); return; }

			// Verify foreground color is red (TMT_COLOR_RED = 2)
			if (s->lines[0]->chars[0].a.fg != TMT_COLOR_RED)
			{ tmt_close(vt); TestCompleted(false, ("Cell 0,0 fg expected RED(2), got " + std::to_string(s->lines[0]->chars[0].a.fg)).c_str()); return; }

			tmt_close(vt);
		}
		StepCompleted(3, true, "Color attribute ESC[31m sets red foreground");

		// Step 4: Clear screen via escape sequence
		{
			TMT *vt = tmt_open(25, 80, TmtCallback, NULL, NULL);
			if (!vt)
			{ TestCompleted(false, "tmt_open returned NULL"); return; }

			// Write something, then clear
			tmt_write(vt, "ABCDEF", 6);
			tmt_write(vt, "\033[2J", 4);

			const TMTSCREEN *s = tmt_screen(vt);
			if (!s)
			{ tmt_close(vt); TestCompleted(false, "tmt_screen returned NULL"); return; }

			// After clear, cells should be spaces
			bool allSpaces = true;
			for (size_t col = 0; col < 6; col++)
			{
				if (s->lines[0]->chars[col].c != L' ')
				{
					allSpaces = false;
					break;
				}
			}
			if (!allSpaces)
			{ tmt_close(vt); TestCompleted(false, "After ESC[2J, cells should be spaces"); return; }

			tmt_close(vt);
		}
		StepCompleted(4, true, "Clear screen ESC[2J clears all cells to spaces");

		// Step 5: Reset via tmt_reset
		{
			TMT *vt = tmt_open(25, 80, TmtCallback, NULL, NULL);
			if (!vt)
			{ TestCompleted(false, "tmt_open returned NULL"); return; }

			tmt_write(vt, "\033[31mColored", 12);
			tmt_reset(vt);

			const TMTSCREEN *s = tmt_screen(vt);
			const TMTPOINT *c = tmt_cursor(vt);

			// After reset, cursor should be at 0,0
			if (c->r != 0 || c->c != 0)
			{ tmt_close(vt); TestCompleted(false, "After reset, cursor should be at 0,0"); return; }

			// After reset, first cell should be space
			if (s->lines[0]->chars[0].c != L' ')
			{ tmt_close(vt); TestCompleted(false, "After reset, first cell should be space"); return; }

			tmt_close(vt);
		}
		StepCompleted(5, true, "tmt_reset clears screen and resets cursor");

		TestCompleted(true, "All terminal emulator tests passed");
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};

bool CTestTerminalEmulator::callbackCalled = false;
