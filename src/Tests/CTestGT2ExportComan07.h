#pragma once

#include "CTest.h"

// Smoke test: load tests/data/gt2/coman07.sng and run relocator_export.
// The song trips "Assembly failed." in the editor's Export dialog; this
// test reproduces the failure under tests/run_test.sh so the diagnostic
// path is automated and the exact error message lands in the test log.
//
// Pass condition: export returns 0 (success).
// Fail mode: returns non-zero -> message body is the verbatim error
// from greloc.c (assembler diagnostic, pool overflow, etc.).
class CTestGT2ExportComan07 : public CTest
{
public:
	virtual const char *GetName() override { return "GT2ExportComan07"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override {}
};
