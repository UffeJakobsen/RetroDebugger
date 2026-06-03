#pragma once

#include "CTest.h"
#include "../Emulators/c64u/Transport/C64UPort64Client.h"

#include <cstring>
#include <string>
#include <vector>

class CTestC64UTcp64Protocol : public CTest
{
public:
	virtual const char *GetName() override { return "C64UTcp64Protocol"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

		// Step 1: SerializeAuthenticate("secret")
		// Expected: [0x1F, 0xFF, 0x06, 0x00, 's', 'e', 'c', 'r', 'e', 't']
		{
			std::vector<uint8_t> pkt = C64UPort64Client::SerializeAuthenticate("secret");
			uint8_t expected[] = {0x1F, 0xFF, 0x06, 0x00, 's', 'e', 'c', 'r', 'e', 't'};
			if (pkt.size() != sizeof(expected))
			{
				TestCompleted(false, "SerializeAuthenticate size mismatch");
				return;
			}
			if (memcmp(pkt.data(), expected, sizeof(expected)) != 0)
			{
				TestCompleted(false, "SerializeAuthenticate content mismatch");
				return;
			}
		}
		StepCompleted(1, true, "SerializeAuthenticate produces correct wire format");

		// Step 2: SerializeIdentify()
		// Expected: [0x0E, 0xFF, 0x00, 0x00]
		{
			std::vector<uint8_t> pkt = C64UPort64Client::SerializeIdentify();
			uint8_t expected[] = {0x0E, 0xFF, 0x00, 0x00};
			if (pkt.size() != sizeof(expected))
			{
				TestCompleted(false, "SerializeIdentify size mismatch");
				return;
			}
			if (memcmp(pkt.data(), expected, sizeof(expected)) != 0)
			{
				TestCompleted(false, "SerializeIdentify content mismatch");
				return;
			}
		}
		StepCompleted(2, true, "SerializeIdentify produces correct wire format");

		// Step 3: SerializeVideoStart(0, "192.168.1.100:11000")
		// Expected: [0x20, 0xFF, len_lo, len_hi, 0x00, 0x00, '1','9','2',...]
		{
			std::string dest = "192.168.1.100:11000";
			std::vector<uint8_t> pkt = C64UPort64Client::SerializeVideoStart(0, dest);
			// payload = 2 bytes bufsize + dest string
			uint16_t payloadLen = 2 + (uint16_t)dest.size();
			if (pkt.size() != (size_t)(4 + payloadLen))
			{
				TestCompleted(false, "SerializeVideoStart size mismatch");
				return;
			}
			if (pkt[0] != 0x20 || pkt[1] != 0xFF)
			{
				TestCompleted(false, "SerializeVideoStart cmd bytes wrong");
				return;
			}
			if (pkt[2] != (uint8_t)(payloadLen & 0xFF) || pkt[3] != (uint8_t)((payloadLen >> 8) & 0xFF))
			{
				TestCompleted(false, "SerializeVideoStart length bytes wrong");
				return;
			}
			// buffer size should be 0x0000
			if (pkt[4] != 0x00 || pkt[5] != 0x00)
			{
				TestCompleted(false, "SerializeVideoStart buffer size bytes wrong");
				return;
			}
			// destination string
			std::string gotDest((const char *)&pkt[6], dest.size());
			if (gotDest != dest)
			{
				TestCompleted(false, "SerializeVideoStart destination string wrong");
				return;
			}
		}
		StepCompleted(3, true, "SerializeVideoStart produces correct wire format");

		// Step 4: SerializeVideoStop()
		// Expected: [0x30, 0xFF, 0x00, 0x00]
		{
			std::vector<uint8_t> pkt = C64UPort64Client::SerializeVideoStop();
			uint8_t expected[] = {0x30, 0xFF, 0x00, 0x00};
			if (pkt.size() != sizeof(expected))
			{
				TestCompleted(false, "SerializeVideoStop size mismatch");
				return;
			}
			if (memcmp(pkt.data(), expected, sizeof(expected)) != 0)
			{
				TestCompleted(false, "SerializeVideoStop content mismatch");
				return;
			}
		}
		StepCompleted(4, true, "SerializeVideoStop produces correct wire format");

		// Step 5: SerializeDmaWrite(0x0400, data, 4)
		// Expected: [0x06, 0xFF, 0x06, 0x00, 0x00, 0x04, d0, d1, d2, d3]
		{
			uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
			std::vector<uint8_t> pkt = C64UPort64Client::SerializeDmaWrite(0x0400, data, 4);
			uint8_t expected[] = {0x06, 0xFF, 0x06, 0x00, 0x00, 0x04, 0xAA, 0xBB, 0xCC, 0xDD};
			if (pkt.size() != sizeof(expected))
			{
				TestCompleted(false, "SerializeDmaWrite size mismatch");
				return;
			}
			if (memcmp(pkt.data(), expected, sizeof(expected)) != 0)
			{
				TestCompleted(false, "SerializeDmaWrite content mismatch");
				return;
			}
		}
		StepCompleted(5, true, "SerializeDmaWrite produces correct wire format");

		// Step 6: SerializeKeyboardInject("HELLO")
		// Expected: [0x03, 0xFF, 0x05, 0x00, 'H', 'E', 'L', 'L', 'O']
		{
			std::vector<uint8_t> pkt = C64UPort64Client::SerializeKeyboardInject("HELLO");
			uint8_t expected[] = {0x03, 0xFF, 0x05, 0x00, 'H', 'E', 'L', 'L', 'O'};
			if (pkt.size() != sizeof(expected))
			{
				TestCompleted(false, "SerializeKeyboardInject(HELLO) size mismatch");
				return;
			}
			if (memcmp(pkt.data(), expected, sizeof(expected)) != 0)
			{
				TestCompleted(false, "SerializeKeyboardInject(HELLO) content mismatch");
				return;
			}
		}
		StepCompleted(6, true, "SerializeKeyboardInject produces correct wire format");

		// Step 7: SerializeKeyboardInject("12345678901234") -- truncated to 10 chars
		{
			std::vector<uint8_t> pkt = C64UPort64Client::SerializeKeyboardInject("12345678901234");
			// Header: [0x03, 0xFF, 0x0A, 0x00] + 10 chars = 14 bytes
			if (pkt.size() != 14)
			{
				TestCompleted(false, "SerializeKeyboardInject truncation size mismatch");
				return;
			}
			if (pkt[0] != 0x03 || pkt[1] != 0xFF)
			{
				TestCompleted(false, "SerializeKeyboardInject truncation cmd bytes wrong");
				return;
			}
			// Length should be 10 = 0x0A
			if (pkt[2] != 0x0A || pkt[3] != 0x00)
			{
				TestCompleted(false, "SerializeKeyboardInject truncation length bytes wrong");
				return;
			}
			// First 10 chars of "12345678901234" are "1234567890"
			std::string got((const char *)&pkt[4], 10);
			if (got != "1234567890")
			{
				TestCompleted(false, "SerializeKeyboardInject truncation content wrong");
				return;
			}
		}
		StepCompleted(7, true, "SerializeKeyboardInject truncates to max 10 chars");

		// Step 8: SerializeMountImage with 174848 bytes (D64 size)
		// 24-bit length: 174848 = 0x02AB00 -> len bytes: 0x00, 0xAB, 0x02
		{
			// We don't need real data, just size verification
			std::vector<uint8_t> fakeData(174848, 0x00);
			std::vector<uint8_t> pkt = C64UPort64Client::SerializeMountImage(fakeData.data(), (int)fakeData.size());
			// 5-byte header + 174848 payload
			if (pkt.size() != (size_t)(5 + 174848))
			{
				TestCompleted(false, "SerializeMountImage size mismatch");
				return;
			}
			if (pkt[0] != 0x0A || pkt[1] != 0xFF)
			{
				TestCompleted(false, "SerializeMountImage cmd bytes wrong");
				return;
			}
			// 24-bit length: 174848 = 0x02AB00
			if (pkt[2] != 0x00 || pkt[3] != 0xAB || pkt[4] != 0x02)
			{
				char buf[128];
				snprintf(buf, sizeof(buf), "SerializeMountImage 24-bit length wrong: got %02X %02X %02X, expected 00 AB 02",
						 pkt[2], pkt[3], pkt[4]);
				TestCompleted(false, buf);
				return;
			}
		}
		StepCompleted(8, true, "SerializeMountImage uses 24-bit length encoding correctly");

		// Step 9: ParseIdentifyResponse with Pascal string
		{
			// Pascal string: [len_byte=0x12, ...18 chars...]
			uint8_t resp[19];
			resp[0] = 0x12; // 18
			const char *name = "Ultimate 64 v3.10";
			// name is 17 chars, but len says 18, so pad one
			memcpy(&resp[1], name, 17);
			resp[18] = 'a';
			std::string result = C64UPort64Client::ParseIdentifyResponse(resp, 19);
			if (result.size() != 18)
			{
				char buf[128];
				snprintf(buf, sizeof(buf), "ParseIdentifyResponse length wrong: got %d expected 18", (int)result.size());
				TestCompleted(false, buf);
				return;
			}
			if (result.substr(0, 17) != "Ultimate 64 v3.10")
			{
				TestCompleted(false, "ParseIdentifyResponse content wrong");
				return;
			}

			// Empty data
			std::string empty = C64UPort64Client::ParseIdentifyResponse(nullptr, 0);
			if (!empty.empty())
			{
				TestCompleted(false, "ParseIdentifyResponse should return empty for null data");
				return;
			}
		}
		StepCompleted(9, true, "ParseIdentifyResponse extracts Pascal string correctly");

		// Step 10: ParseAuthResponse
		{
			uint8_t success[] = {0x01};
			if (!C64UPort64Client::ParseAuthResponse(success, 1))
			{
				TestCompleted(false, "ParseAuthResponse should return true for 0x01");
				return;
			}

			uint8_t failure[] = {0x00};
			if (C64UPort64Client::ParseAuthResponse(failure, 1))
			{
				TestCompleted(false, "ParseAuthResponse should return false for 0x00");
				return;
			}

			// Empty
			if (C64UPort64Client::ParseAuthResponse(nullptr, 0))
			{
				TestCompleted(false, "ParseAuthResponse should return false for empty");
				return;
			}
		}
		StepCompleted(10, true, "ParseAuthResponse handles success, failure, and empty");

		// Step 11: SerializeAudioStop and SerializeReset
		{
			std::vector<uint8_t> audioPkt = C64UPort64Client::SerializeAudioStop();
			uint8_t expectedAudio[] = {0x31, 0xFF, 0x00, 0x00};
			if (audioPkt.size() != sizeof(expectedAudio) ||
				memcmp(audioPkt.data(), expectedAudio, sizeof(expectedAudio)) != 0)
			{
				TestCompleted(false, "SerializeAudioStop content mismatch");
				return;
			}

			std::vector<uint8_t> resetPkt = C64UPort64Client::SerializeReset();
			uint8_t expectedReset[] = {0x04, 0xFF, 0x00, 0x00};
			if (resetPkt.size() != sizeof(expectedReset) ||
				memcmp(resetPkt.data(), expectedReset, sizeof(expectedReset)) != 0)
			{
				TestCompleted(false, "SerializeReset content mismatch");
				return;
			}
		}
		StepCompleted(11, true, "SerializeAudioStop and SerializeReset produce correct wire format");

		// Step 12: SerializeRunImage and SerializeRunCrt use 24-bit headers
		{
			uint8_t smallData[] = {0x01, 0x02, 0x03};
			std::vector<uint8_t> runImg = C64UPort64Client::SerializeRunImage(smallData, 3);
			// 5-byte header + 3 payload
			if (runImg.size() != 8)
			{
				TestCompleted(false, "SerializeRunImage size mismatch");
				return;
			}
			if (runImg[0] != 0x0B || runImg[1] != 0xFF)
			{
				TestCompleted(false, "SerializeRunImage cmd bytes wrong");
				return;
			}
			// 24-bit length: 3 = 0x000003
			if (runImg[2] != 0x03 || runImg[3] != 0x00 || runImg[4] != 0x00)
			{
				TestCompleted(false, "SerializeRunImage 24-bit length wrong");
				return;
			}
			if (runImg[5] != 0x01 || runImg[6] != 0x02 || runImg[7] != 0x03)
			{
				TestCompleted(false, "SerializeRunImage payload wrong");
				return;
			}

			std::vector<uint8_t> runCrt = C64UPort64Client::SerializeRunCrt(smallData, 3);
			if (runCrt.size() != 8)
			{
				TestCompleted(false, "SerializeRunCrt size mismatch");
				return;
			}
			if (runCrt[0] != 0x0D || runCrt[1] != 0xFF)
			{
				TestCompleted(false, "SerializeRunCrt cmd bytes wrong");
				return;
			}
			if (runCrt[2] != 0x03 || runCrt[3] != 0x00 || runCrt[4] != 0x00)
			{
				TestCompleted(false, "SerializeRunCrt 24-bit length wrong");
				return;
			}
		}
		StepCompleted(12, true, "SerializeRunImage and SerializeRunCrt use 24-bit headers");

		// Step 13: SerializeDmaLoad and SerializeDmaLoadRun
		{
			uint8_t prgData[] = {0x00, 0x08, 0x0A, 0x00};
			std::vector<uint8_t> dmaLoad = C64UPort64Client::SerializeDmaLoad(prgData, 4);
			if (dmaLoad.size() != 8)
			{
				TestCompleted(false, "SerializeDmaLoad size mismatch");
				return;
			}
			if (dmaLoad[0] != 0x01 || dmaLoad[1] != 0xFF)
			{
				TestCompleted(false, "SerializeDmaLoad cmd bytes wrong");
				return;
			}
			if (dmaLoad[2] != 0x04 || dmaLoad[3] != 0x00)
			{
				TestCompleted(false, "SerializeDmaLoad length bytes wrong");
				return;
			}

			std::vector<uint8_t> dmaLoadRun = C64UPort64Client::SerializeDmaLoadRun(prgData, 4);
			if (dmaLoadRun[0] != 0x02 || dmaLoadRun[1] != 0xFF)
			{
				TestCompleted(false, "SerializeDmaLoadRun cmd bytes wrong");
				return;
			}
		}
		StepCompleted(13, true, "SerializeDmaLoad and SerializeDmaLoadRun produce correct wire format");

		// Step 14: Fixture mode flag
		{
			C64UPort64Client client;
			if (client.IsFixtureMode())
			{
				TestCompleted(false, "Client should not be in fixture mode by default");
				return;
			}
			client.SetFixtureMode(true);
			if (!client.IsFixtureMode())
			{
				TestCompleted(false, "SetFixtureMode(true) did not enable fixture mode");
				return;
			}
			client.SetFixtureMode(false);
			if (client.IsFixtureMode())
			{
				TestCompleted(false, "SetFixtureMode(false) did not disable fixture mode");
				return;
			}
		}
		StepCompleted(14, true, "Fixture mode flag works correctly");

		// Step 15: SerializeDebugStreamStart
		// Expected: [0x22, 0xFF, len_lo, len_hi, 0x00, 0x00, '1','9','2',...]
		{
			std::string dest = "192.168.1.100:11002";
			std::vector<uint8_t> pkt = C64UPort64Client::SerializeDebugStreamStart(0, dest);
			// payload = 2 bytes bufsize + dest string
			uint16_t payloadLen = 2 + (uint16_t)dest.size();
			if (pkt.size() != (size_t)(4 + payloadLen))
			{
				TestCompleted(false, "SerializeDebugStreamStart size mismatch");
				return;
			}
			if (pkt[0] != 0x22 || pkt[1] != 0xFF)
			{
				char buf[128];
				snprintf(buf, sizeof(buf), "SerializeDebugStreamStart cmd bytes wrong: got %02X %02X, expected 22 FF",
						 pkt[0], pkt[1]);
				TestCompleted(false, buf);
				return;
			}
			if (pkt[2] != (uint8_t)(payloadLen & 0xFF) || pkt[3] != (uint8_t)((payloadLen >> 8) & 0xFF))
			{
				TestCompleted(false, "SerializeDebugStreamStart length bytes wrong");
				return;
			}
			// buffer size should be 0x0000
			if (pkt[4] != 0x00 || pkt[5] != 0x00)
			{
				TestCompleted(false, "SerializeDebugStreamStart buffer size bytes wrong");
				return;
			}
			// destination string
			std::string gotDest((const char *)&pkt[6], dest.size());
			if (gotDest != dest)
			{
				TestCompleted(false, "SerializeDebugStreamStart destination string wrong");
				return;
			}
		}
		StepCompleted(15, true, "SerializeDebugStreamStart produces correct wire format with cmd 0x22");

		// Step 16: SerializeDebugStreamStop
		// Expected: [0x32, 0xFF, 0x00, 0x00]
		{
			std::vector<uint8_t> pkt = C64UPort64Client::SerializeDebugStreamStop();
			uint8_t expected[] = {0x32, 0xFF, 0x00, 0x00};
			if (pkt.size() != sizeof(expected))
			{
				TestCompleted(false, "SerializeDebugStreamStop size mismatch");
				return;
			}
			if (memcmp(pkt.data(), expected, sizeof(expected)) != 0)
			{
				char buf[128];
				snprintf(buf, sizeof(buf), "SerializeDebugStreamStop content wrong: got %02X %02X %02X %02X, expected 32 FF 00 00",
						 pkt[0], pkt[1], pkt[2], pkt[3]);
				TestCompleted(false, buf);
				return;
			}
		}
		StepCompleted(16, true, "SerializeDebugStreamStop produces correct wire format with cmd 0x32");

		TestCompleted(true, "All TCP64 protocol serializers and parsers are correct");
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
