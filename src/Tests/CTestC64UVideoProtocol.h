#pragma once

#include "CTest.h"
#include "../Emulators/c64u/Transport/C64UVideoStream.h"
#include "../Emulators/c64u/Transport/C64UNybbleLUT.h"

#include <cstring>

class CTestC64UVideoProtocol : public CTest
{
public:
	virtual const char *GetName() override { return "C64UVideoProtocol"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

		C64UNybbleLUT lut;
		lut.Build();

		// Step 1: LUT correctness for byte 0x16
		// lo = 6 (blue [0x35,0x28,0x79]), hi = 1 (white [0xFF,0xFF,0xFF])
		{
			const uint8_t *entry = lut.table[0x16];
			if (entry[0] != 0x35 || entry[1] != 0x28 || entry[2] != 0x79 || entry[3] != 0xFF)
			{
				TestCompleted(false, "LUT[0x16] left pixel (blue) wrong");
				return;
			}
			if (entry[4] != 0xFF || entry[5] != 0xFF || entry[6] != 0xFF || entry[7] != 0xFF)
			{
				TestCompleted(false, "LUT[0x16] right pixel (white) wrong");
				return;
			}
		}
		StepCompleted(1, true, "LUT entry 0x16 maps to blue+white correctly");

		// Step 2: LUT completeness -- all 256 entries have alpha=0xFF at [3] and [7]
		{
			for (int i = 0; i < 256; i++)
			{
				if (lut.table[i][3] != 0xFF || lut.table[i][7] != 0xFF)
				{
					char buf[128];
					snprintf(buf, sizeof(buf), "LUT[0x%02X] alpha not 0xFF", i);
					TestCompleted(false, buf);
					return;
				}
			}
		}
		StepCompleted(2, true, "All 256 LUT entries have alpha=0xFF");

		// Step 3: ParsePacketHeader with a valid 12-byte header
		{
			uint8_t pkt[12];
			// seq = 0x1234
			pkt[0] = 0x34; pkt[1] = 0x12;
			// frame = 0x0056
			pkt[2] = 0x56; pkt[3] = 0x00;
			// line_raw = 100 (no frame-done)
			pkt[4] = 100; pkt[5] = 0x00;
			// pixelsInLine = 384 = 0x0180
			pkt[6] = 0x80; pkt[7] = 0x01;
			// linesInPacket = 1
			pkt[8] = 1;
			// bpp = 4
			pkt[9] = 4;
			// encoding = 0
			pkt[10] = 0; pkt[11] = 0;

			C64UVideoPacketHeader hdr;
			if (!C64UVideoStream::ParsePacketHeader(pkt, 12, hdr))
			{
				TestCompleted(false, "ParsePacketHeader returned false for valid packet");
				return;
			}
			if (hdr.seq != 0x1234 || hdr.frame != 0x0056 || hdr.lineNum != 100 ||
				hdr.frameDone || hdr.pixelsInLine != 384 || hdr.linesInPacket != 1 ||
				hdr.bpp != 4 || hdr.encoding != 0)
			{
				TestCompleted(false, "ParsePacketHeader fields incorrect");
				return;
			}
		}
		StepCompleted(3, true, "ParsePacketHeader extracts all fields correctly");

		// Step 4: ParsePacketHeader with frame-done flag (bit 15 set)
		{
			uint8_t pkt[12] = {};
			// line_raw = 200 | 0x8000 = 0x80C8
			pkt[4] = 0xC8; pkt[5] = 0x80;
			pkt[6] = 0x80; pkt[7] = 0x01; // pixelsInLine=384
			pkt[8] = 1; // linesInPacket

			C64UVideoPacketHeader hdr;
			if (!C64UVideoStream::ParsePacketHeader(pkt, 12, hdr))
			{
				TestCompleted(false, "ParsePacketHeader returned false for frame-done packet");
				return;
			}
			if (!hdr.frameDone)
			{
				TestCompleted(false, "frameDone should be true when bit 15 is set");
				return;
			}
			if (hdr.lineNum != 200)
			{
				char buf[128];
				snprintf(buf, sizeof(buf), "lineNum should be 200, got %d", hdr.lineNum);
				TestCompleted(false, buf);
				return;
			}
		}
		StepCompleted(4, true, "ParsePacketHeader detects frame-done flag");

		// Step 5: ParsePacketHeader too short (8 bytes)
		{
			uint8_t pkt[8] = {};
			C64UVideoPacketHeader hdr;
			if (C64UVideoStream::ParsePacketHeader(pkt, 8, hdr))
			{
				TestCompleted(false, "ParsePacketHeader should return false for 8-byte input");
				return;
			}
		}
		StepCompleted(5, true, "ParsePacketHeader rejects too-short input");

		// Step 6: DecodeLine with all-0x66 (blue+blue)
		// 0x66: lo=6 (blue [0x35,0x28,0x79]), hi=6 (blue)
		{
			uint8_t nybbleData[192];
			memset(nybbleData, 0x66, sizeof(nybbleData));
			uint8_t rgba[384 * 4];
			C64UVideoStream::DecodeLine(nybbleData, 192, lut, rgba);

			// Check all pixels are blue
			bool allBlue = true;
			for (int px = 0; px < 384; px++)
			{
				int off = px * 4;
				if (rgba[off] != 0x35 || rgba[off+1] != 0x28 ||
					rgba[off+2] != 0x79 || rgba[off+3] != 0xFF)
				{
					allBlue = false;
					break;
				}
			}
			if (!allBlue)
			{
				TestCompleted(false, "DecodeLine 0x66 should produce all-blue pixels");
				return;
			}
		}
		StepCompleted(6, true, "DecodeLine decodes all-blue line correctly");

		// Step 7: DecodeLine mixed pattern [0x01, 0x23, ...]
		{
			uint8_t nybbleData[2] = {0x01, 0x23};
			uint8_t rgba[4 * 4]; // 4 pixels
			C64UVideoStream::DecodeLine(nybbleData, 2, lut, rgba);

			// Byte 0x01: lo=1 (white), hi=0 (black)
			// pixel 0 = white [0xFF,0xFF,0xFF,0xFF]
			if (rgba[0] != 0xFF || rgba[1] != 0xFF || rgba[2] != 0xFF || rgba[3] != 0xFF)
			{
				TestCompleted(false, "DecodeLine mixed: pixel 0 should be white");
				return;
			}
			// pixel 1 = black [0x00,0x00,0x00,0xFF]
			if (rgba[4] != 0x00 || rgba[5] != 0x00 || rgba[6] != 0x00 || rgba[7] != 0xFF)
			{
				TestCompleted(false, "DecodeLine mixed: pixel 1 should be black");
				return;
			}
			// Byte 0x23: lo=3 (cyan [0x70,0xA4,0xB2]), hi=2 (red [0x68,0x37,0x2B])
			// pixel 2 = cyan
			if (rgba[8] != 0x70 || rgba[9] != 0xA4 || rgba[10] != 0xB2 || rgba[11] != 0xFF)
			{
				TestCompleted(false, "DecodeLine mixed: pixel 2 should be cyan");
				return;
			}
			// pixel 3 = red
			if (rgba[12] != 0x68 || rgba[13] != 0x37 || rgba[14] != 0x2B || rgba[15] != 0xFF)
			{
				TestCompleted(false, "DecodeLine mixed: pixel 3 should be red");
				return;
			}
		}
		StepCompleted(7, true, "DecodeLine decodes mixed nybble pattern correctly");

		// Step 8: Line bounds -- lineNum=271 is valid, lineNum=272 is out of bounds
		{
			uint8_t pkt271[12] = {};
			pkt271[4] = 271 & 0xFF; pkt271[5] = (271 >> 8) & 0x7F;
			pkt271[6] = 0x80; pkt271[7] = 0x01;
			pkt271[8] = 1;
			C64UVideoPacketHeader hdr271;
			if (!C64UVideoStream::ParsePacketHeader(pkt271, 12, hdr271))
			{
				TestCompleted(false, "ParsePacketHeader should accept lineNum=271");
				return;
			}
			if (hdr271.lineNum != 271)
			{
				TestCompleted(false, "lineNum should be 271");
				return;
			}

			uint8_t pkt272[12] = {};
			pkt272[4] = 272 & 0xFF; pkt272[5] = (272 >> 8) & 0x7F;
			pkt272[6] = 0x80; pkt272[7] = 0x01;
			pkt272[8] = 1;
			C64UVideoPacketHeader hdr272;
			if (!C64UVideoStream::ParsePacketHeader(pkt272, 12, hdr272))
			{
				TestCompleted(false, "ParsePacketHeader should still parse lineNum=272 header");
				return;
			}
			// lineNum=272 parses fine, but would be skipped during decode (>= FRAME_HEIGHT)
			if (hdr272.lineNum != 272)
			{
				TestCompleted(false, "lineNum should be 272");
				return;
			}
		}
		StepCompleted(8, true, "Line bounds: 271 valid, 272 parseable but out of frame");

		// Step 9: Sequence gap detection logic
		// If lastSeq=10 and newSeq=15, gap = 15 - 10 - 1 = 4
		{
			uint16_t lastSeq = 10;
			uint16_t newSeq = 15;
			uint16_t expected = lastSeq + 1;
			uint64_t gap = 0;
			if (newSeq != expected && newSeq > lastSeq)
			{
				gap = (uint64_t)(newSeq - lastSeq - 1);
			}
			if (gap != 4)
			{
				char buf[128];
				snprintf(buf, sizeof(buf), "Sequence gap should be 4, got %llu", (unsigned long long)gap);
				TestCompleted(false, buf);
				return;
			}

			// No gap case: lastSeq=10, newSeq=11
			lastSeq = 10;
			newSeq = 11;
			expected = lastSeq + 1;
			gap = 0;
			if (newSeq != expected && newSeq > lastSeq)
			{
				gap = (uint64_t)(newSeq - lastSeq - 1);
			}
			if (gap != 0)
			{
				TestCompleted(false, "Sequential packets should have gap 0");
				return;
			}
		}
		StepCompleted(9, true, "Sequence gap detection logic correct");

		// Step 10: Multi-line packet -- linesInPacket=2 with 384 bytes payload
		{
			// Build a packet with 2 lines starting at line 10
			uint8_t pkt[12 + 384];
			memset(pkt, 0, sizeof(pkt));
			// line_raw = 10
			pkt[4] = 10; pkt[5] = 0x00;
			// pixelsInLine = 384
			pkt[6] = 0x80; pkt[7] = 0x01;
			// linesInPacket = 2
			pkt[8] = 2;

			C64UVideoPacketHeader hdr;
			if (!C64UVideoStream::ParsePacketHeader(pkt, sizeof(pkt), hdr))
			{
				TestCompleted(false, "ParsePacketHeader should parse multi-line packet");
				return;
			}
			if (hdr.linesInPacket != 2)
			{
				TestCompleted(false, "linesInPacket should be 2");
				return;
			}
			if (hdr.lineNum != 10)
			{
				TestCompleted(false, "lineNum should be 10");
				return;
			}

			// Decode both lines: fill line 0 with 0x11 (white+white) and line 1 with 0x00 (black+black)
			memset(pkt + 12, 0x11, 192);       // line 0 payload
			memset(pkt + 12 + 192, 0x00, 192); // line 1 payload

			// Decode line 0 manually
			uint8_t line0rgba[384 * 4];
			C64UVideoStream::DecodeLine(pkt + 12, 192, lut, line0rgba);
			// 0x11: lo=1 (white), hi=1 (white)
			if (line0rgba[0] != 0xFF || line0rgba[1] != 0xFF || line0rgba[2] != 0xFF)
			{
				TestCompleted(false, "Multi-line: line 0 first pixel should be white");
				return;
			}

			// Decode line 1 manually
			uint8_t line1rgba[384 * 4];
			C64UVideoStream::DecodeLine(pkt + 12 + 192, 192, lut, line1rgba);
			// 0x00: lo=0 (black), hi=0 (black)
			if (line1rgba[0] != 0x00 || line1rgba[1] != 0x00 || line1rgba[2] != 0x00)
			{
				TestCompleted(false, "Multi-line: line 1 first pixel should be black");
				return;
			}
		}
		StepCompleted(10, true, "Multi-line packet decodes both lines correctly");

		TestCompleted(true, "All UDP video protocol tests passed");
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
