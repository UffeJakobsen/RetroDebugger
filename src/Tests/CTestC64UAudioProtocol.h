#pragma once

#include "CTest.h"
#include "../Emulators/c64u/Transport/C64UAudioStream.h"
#include "EmulatorsConfig.h"

#include <cmath>
#include <cstring>
#include <vector>

class CTestC64UAudioProtocol : public CTest
{
public:
	virtual const char *GetName() override { return "C64UAudioProtocol"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

#ifndef RUN_COMMODORE64
		TestCompleted(true, "Skipped (C64 not enabled)");
		return;
#else
		// Step 1: ParsePacket with 10 known i16 stereo pairs -- verify float output
		{
			// Build a packet: 2-byte seq header + 10 stereo pairs (40 bytes payload)
			uint8_t packet[2 + 10 * 4];
			packet[0] = 0x01;  // seq = 1
			packet[1] = 0x00;

			// Known i16 stereo pairs
			int16_t testValues[10][2] = {
				{0, 0},
				{1000, -1000},
				{16384, -16384},
				{32767, -32768},
				{100, 200},
				{-100, -200},
				{10000, 20000},
				{-10000, -20000},
				{1, -1},
				{32000, -32000}
			};

			for (int i = 0; i < 10; i++)
			{
				int off = 2 + i * 4;
				packet[off + 0] = (uint8_t)(testValues[i][0] & 0xFF);
				packet[off + 1] = (uint8_t)((testValues[i][0] >> 8) & 0xFF);
				packet[off + 2] = (uint8_t)(testValues[i][1] & 0xFF);
				packet[off + 3] = (uint8_t)((testValues[i][1] >> 8) & 0xFF);
			}

			float outLR[20];
			int decoded = C64UAudioStream::ParsePacket(packet, sizeof(packet), outLR, 10);

			if (decoded != 10)
			{
				TestCompleted(false, "Step 1: ParsePacket must decode 10 stereo pairs");
				return;
			}

			float tolerance = 1e-4f;
			for (int i = 0; i < 10; i++)
			{
				float expectedL = (float)testValues[i][0] / 32768.0f;
				float expectedR = (float)testValues[i][1] / 32768.0f;
				if (fabsf(outLR[i * 2] - expectedL) > tolerance ||
					fabsf(outLR[i * 2 + 1] - expectedR) > tolerance)
				{
					TestCompleted(false, "Step 1: Float output mismatch at pair index");
					return;
				}
			}

			StepCompleted(1, true, "ParsePacket with 10 known pairs verified");
		}

		// Step 2: ParsePacket with silence (all zeros)
		{
			uint8_t packet[2 + 8 * 4];
			memset(packet, 0, sizeof(packet));

			float outLR[16];
			int decoded = C64UAudioStream::ParsePacket(packet, sizeof(packet), outLR, 8);

			if (decoded != 8)
			{
				TestCompleted(false, "Step 2: ParsePacket must decode 8 silent stereo pairs");
				return;
			}

			bool allZero = true;
			for (int i = 0; i < 16; i++)
			{
				if (outLR[i] != 0.0f)
				{
					allZero = false;
					break;
				}
			}
			if (!allZero)
			{
				TestCompleted(false, "Step 2: Silence must decode to all 0.0f");
				return;
			}

			StepCompleted(2, true, "ParsePacket silence verified as 0.0f");
		}

		// Step 3: ParsePacket max positive 0x7FFF -> ~0.99997f
		{
			uint8_t packet[6];
			packet[0] = 0x00;  // seq
			packet[1] = 0x00;
			packet[2] = 0xFF;  // 0x7FFF LE = 0xFF, 0x7F
			packet[3] = 0x7F;
			packet[4] = 0xFF;  // right channel same
			packet[5] = 0x7F;

			float outLR[2];
			int decoded = C64UAudioStream::ParsePacket(packet, sizeof(packet), outLR, 1);

			if (decoded != 1)
			{
				TestCompleted(false, "Step 3: ParsePacket must decode 1 pair");
				return;
			}

			float expected = 32767.0f / 32768.0f;  // ~0.99997f
			float tolerance = 1e-4f;
			if (fabsf(outLR[0] - expected) > tolerance || fabsf(outLR[1] - expected) > tolerance)
			{
				TestCompleted(false, "Step 3: Max positive 0x7FFF must be ~0.99997f");
				return;
			}

			StepCompleted(3, true, "Max positive 0x7FFF converts to ~0.99997f");
		}

		// Step 4: ParsePacket max negative 0x8000 -> -1.0f
		{
			uint8_t packet[6];
			packet[0] = 0x00;
			packet[1] = 0x00;
			packet[2] = 0x00;  // 0x8000 LE = 0x00, 0x80
			packet[3] = 0x80;
			packet[4] = 0x00;
			packet[5] = 0x80;

			float outLR[2];
			int decoded = C64UAudioStream::ParsePacket(packet, sizeof(packet), outLR, 1);

			if (decoded != 1)
			{
				TestCompleted(false, "Step 4: ParsePacket must decode 1 pair");
				return;
			}

			float tolerance = 1e-4f;
			if (fabsf(outLR[0] - (-1.0f)) > tolerance || fabsf(outLR[1] - (-1.0f)) > tolerance)
			{
				TestCompleted(false, "Step 4: Max negative 0x8000 must be -1.0f");
				return;
			}

			StepCompleted(4, true, "Max negative 0x8000 converts to -1.0f");
		}

		// Step 5: ParsePacket too-short data (<4 bytes total, less than header + 1 pair)
		{
			uint8_t packet[3] = {0x00, 0x00, 0x01};  // just seq + 1 byte payload
			float outLR[2];
			int decoded = C64UAudioStream::ParsePacket(packet, 3, outLR, 1);

			if (decoded != 0)
			{
				TestCompleted(false, "Step 5: Too-short packet must return 0 decoded pairs");
				return;
			}

			StepCompleted(5, true, "Too-short packet returns 0 decoded pairs");
		}

		// Step 6: ParsePacket odd byte count -- truncates to complete pairs
		{
			// 2-byte seq + 5 bytes payload (only 1 complete i16 stereo pair, 1 leftover byte)
			uint8_t packet[7];
			packet[0] = 0x00;
			packet[1] = 0x00;
			// First pair
			packet[2] = 0xE8;  // 1000 LE = 0xE8, 0x03
			packet[3] = 0x03;
			packet[4] = 0x18;  // -1000 LE = 0x18, 0xFC
			packet[5] = 0xFC;
			// Leftover byte
			packet[6] = 0xAA;

			float outLR[4];
			int decoded = C64UAudioStream::ParsePacket(packet, sizeof(packet), outLR, 2);

			if (decoded != 1)
			{
				TestCompleted(false, "Step 6: Odd byte count must truncate to 1 complete pair");
				return;
			}

			float tolerance = 1e-4f;
			float expectedL = 1000.0f / 32768.0f;
			float expectedR = -1000.0f / 32768.0f;
			if (fabsf(outLR[0] - expectedL) > tolerance || fabsf(outLR[1] - expectedR) > tolerance)
			{
				TestCompleted(false, "Step 6: Truncated packet values incorrect");
				return;
			}

			StepCompleted(6, true, "Odd byte count truncates to complete pairs");
		}

		TestCompleted(true, "C64U audio protocol parsing works correctly");
#endif
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
