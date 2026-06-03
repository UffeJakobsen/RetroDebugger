#pragma once

#include "CTest.h"

// Regression test for autoload after InsertD64 with auto-jmp enabled.
//
// Uses tests/data/bitbreaker.d64 — a demo PRG that uses the BitBreaker
// fastloader. With a correct autoload (BASIC vectors set, drive ready),
// the loader runs and the depacker takes over at ~$0200-$03FF.
// With a broken autoload, the loader hangs at $103C BIT $DD00 / $103F BMI $103C
// waiting for the drive to pull the serial DATA line low.
class CTestAutoloadD64 : public CTest
{
public:
	virtual const char *GetName() override { return "AutoloadD64"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
