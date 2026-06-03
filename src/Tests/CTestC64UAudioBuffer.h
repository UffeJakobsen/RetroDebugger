#pragma once

#include "CTest.h"
#include "../Emulators/c64u/Audio/C64UAudioJitterBuffer.h"
#include "EmulatorsConfig.h"

#include <cmath>
#include <vector>

class CTestC64UAudioBuffer : public CTest
{
public:
	virtual const char *GetName() override { return "C64UAudioBuffer"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

#ifndef RUN_COMMODORE64
		TestCompleted(true, "Skipped (C64 not enabled)");
		return;
#else
		// Step 1: Write 500 stereo samples, verify fill level
		{
			C64UAudioJitterBuffer buf(1000);
			std::vector<float> interleaved(500 * 2);
			for (int i = 0; i < 500; i++)
			{
				interleaved[i * 2]     = i * 0.001f;
				interleaved[i * 2 + 1] = i * 0.002f;
			}
			buf.Write(interleaved.data(), 500);

			if (buf.GetFillLevel() != 500)
			{
				TestCompleted(false, "Step 1: Fill level must be 500 after writing 500 samples");
				return;
			}
			StepCompleted(1, true, "Write 500 stereo samples, fill level correct");
		}

		// Step 2: Write 500, read 300, verify output and remaining fill level
		{
			C64UAudioJitterBuffer buf(1000);
			std::vector<float> interleaved(500 * 2);
			for (int i = 0; i < 500; i++)
			{
				interleaved[i * 2]     = i * 0.001f;
				interleaved[i * 2 + 1] = i * 0.002f;
			}
			buf.Write(interleaved.data(), 500);

			std::vector<float> outL(300), outR(300);
			int actual = buf.Read(outL.data(), outR.data(), 300);

			if (actual != 300)
			{
				TestCompleted(false, "Step 2: Read must return 300 actual samples");
				return;
			}
			if (buf.GetFillLevel() != 200)
			{
				TestCompleted(false, "Step 2: Fill level must be 200 after reading 300 of 500");
				return;
			}

			// Verify first and last values of the read output
			bool valuesMatch = true;
			float tolerance = 1e-6f;
			if (fabsf(outL[0] - 0.0f) > tolerance || fabsf(outR[0] - 0.0f) > tolerance)
				valuesMatch = false;
			if (fabsf(outL[1] - 0.001f) > tolerance || fabsf(outR[1] - 0.002f) > tolerance)
				valuesMatch = false;
			if (fabsf(outL[299] - 299 * 0.001f) > tolerance || fabsf(outR[299] - 299 * 0.002f) > tolerance)
				valuesMatch = false;

			if (!valuesMatch)
			{
				TestCompleted(false, "Step 2: Read output values do not match expected pattern");
				return;
			}
			StepCompleted(2, true, "Read 300 samples, values and fill level correct");
		}

		// Step 3: Overflow test -- buffer(1000) with 200 already, write 900 more
		{
			C64UAudioJitterBuffer buf(1000);

			// Write initial 200
			std::vector<float> initial(200 * 2, 0.5f);
			buf.Write(initial.data(), 200);

			// Write 900 more to cause overflow
			std::vector<float> extra(900 * 2, 0.75f);
			buf.Write(extra.data(), 900);

			if (buf.GetFillLevel() != 1000)
			{
				TestCompleted(false, "Step 3: Fill level must stay at capacity (1000) after overflow");
				return;
			}
			if (buf.GetOverflowCount() <= 0)
			{
				TestCompleted(false, "Step 3: Overflow count must be > 0 after writing beyond capacity");
				return;
			}
			// Exactly 200 + 900 = 1100 written, capacity 1000, so 100 samples overflowed
			if (buf.GetOverflowCount() != 100)
			{
				TestCompleted(false, "Step 3: Overflow count must be exactly 100");
				return;
			}
			StepCompleted(3, true, "Overflow correctly drops oldest samples");
		}

		// Step 4: Underflow test -- buffer with 100 samples, read 200
		{
			C64UAudioJitterBuffer buf(1000);
			std::vector<float> data(100 * 2);
			for (int i = 0; i < 100; i++)
			{
				data[i * 2]     = (float)i;
				data[i * 2 + 1] = (float)(i + 1000);
			}
			buf.Write(data.data(), 100);

			std::vector<float> outL(200), outR(200);
			int actual = buf.Read(outL.data(), outR.data(), 200);

			if (actual != 100)
			{
				TestCompleted(false, "Step 4: Read must return 100 actual samples from 100 available");
				return;
			}
			if (buf.GetUnderflowCount() != 1)
			{
				TestCompleted(false, "Step 4: Underflow count must be 1");
				return;
			}

			// Verify the silence padding
			bool silenceCorrect = true;
			for (int i = 100; i < 200; i++)
			{
				if (outL[i] != 0.0f || outR[i] != 0.0f)
				{
					silenceCorrect = false;
					break;
				}
			}
			if (!silenceCorrect)
			{
				TestCompleted(false, "Step 4: Remainder must be filled with silence (0.0f)");
				return;
			}

			// Verify the actual samples
			float tolerance = 1e-6f;
			if (fabsf(outL[0] - 0.0f) > tolerance || fabsf(outR[0] - 1000.0f) > tolerance)
			{
				TestCompleted(false, "Step 4: First actual sample values incorrect");
				return;
			}
			StepCompleted(4, true, "Underflow returns 100 real + 100 silence");
		}

		// Step 5: PeekRecentSamples -- write 500, peek last 100, verify values and fill unchanged
		{
			C64UAudioJitterBuffer buf(1000);
			std::vector<float> interleaved(500 * 2);
			for (int i = 0; i < 500; i++)
			{
				interleaved[i * 2]     = i * 0.01f;
				interleaved[i * 2 + 1] = i * 0.02f;
			}
			buf.Write(interleaved.data(), 500);

			std::vector<float> peekL(100), peekR(100);
			int peeked = buf.PeekRecentSamples(peekL.data(), peekR.data(), 100);

			if (peeked != 100)
			{
				TestCompleted(false, "Step 5: PeekRecentSamples must return 100");
				return;
			}
			if (buf.GetFillLevel() != 500)
			{
				TestCompleted(false, "Step 5: Fill level must remain 500 after peek");
				return;
			}

			// The peeked samples should be the last 100 written (indices 400..499)
			bool peekCorrect = true;
			float tolerance = 1e-4f;
			for (int i = 0; i < 100; i++)
			{
				float expectedL = (400 + i) * 0.01f;
				float expectedR = (400 + i) * 0.02f;
				if (fabsf(peekL[i] - expectedL) > tolerance || fabsf(peekR[i] - expectedR) > tolerance)
				{
					peekCorrect = false;
					break;
				}
			}
			if (!peekCorrect)
			{
				TestCompleted(false, "Step 5: Peeked values do not match the last 100 written");
				return;
			}
			StepCompleted(5, true, "PeekRecentSamples reads last 100 without consuming");
		}

		// Step 6: Reset -- write some, reset, verify fill level is 0
		{
			C64UAudioJitterBuffer buf(1000);
			std::vector<float> data(300 * 2, 1.0f);
			buf.Write(data.data(), 300);

			if (buf.GetFillLevel() != 300)
			{
				TestCompleted(false, "Step 6: Fill level must be 300 before reset");
				return;
			}

			buf.Reset();

			if (buf.GetFillLevel() != 0)
			{
				TestCompleted(false, "Step 6: Fill level must be 0 after reset");
				return;
			}
			StepCompleted(6, true, "Reset clears fill level to 0");
		}

		TestCompleted(true, "C64U audio jitter buffer works correctly");
#endif
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
