#ifndef _CTestSidStatusWaveform_h_
#define _CTestSidStatusWaveform_h_

#include "CTest.h"

// Regression guard for the C64 "SID Status" oscilloscope.
//
// The plain reSID engine (the default SID engine) lost its waveform feed during
// the VICE 3.10 reSID upgrade: sid/resid.cpp created the reSID::SID instance
// without registering any per-sample voice callback, so c64d_sid_channels_data()
// was never called and the SID Status view rendered flat horizontal lines. Only
// fastsid and reSID-FP still fed the view. This test forces the reSID engine,
// plays a sustained tone, and asserts that the voice + mix waveform buffers in
// the C64 debug interface actually move.
class CTestSidStatusWaveform : public CTest
{
public:
	virtual const char *GetName() { return "SidStatusWaveform"; }
	virtual void Run(ITestCallback *callback);
	virtual void Cancel() {}
};

#endif
