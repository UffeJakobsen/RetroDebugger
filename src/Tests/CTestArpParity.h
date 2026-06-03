#pragma once

#include "CTest.h"

// Phase 7 parity verification: drive the C reference player (gplay.c)
// and the 6502 player (player.s assembled into a .prg and loaded into
// embedded VICE) on programmatically-constructed GT2 songs, then diff
// the per-tick SID register writes. See docs/gt2/16-parity-verification.md.
//
// First deliverable: strategy B (cycling math in isolation) plus the
// fixture builder skeleton. Strategy A scenarios land progressively.
class CTestArpParity : public CTest
{
public:
	virtual const char *GetName() override { return "ArpParity"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
