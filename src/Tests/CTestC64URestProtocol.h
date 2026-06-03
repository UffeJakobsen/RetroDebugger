#pragma once

#include "CTest.h"
#include "../Emulators/c64u/Transport/C64URestClient.h"

#include <cstring>
#include <vector>

class CTestC64URestProtocol : public CTest
{
public:
	virtual const char *GetName() override { return "C64URestProtocol"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

		// Step 1: FormatReadMemoryUrl with typical address
		{
			std::string url = C64URestClient::FormatReadMemoryUrl(0x0400, 256);
			if (url != "/v1/machine:readmem?address=400&length=256")
			{
				TestCompleted(false, (std::string("FormatReadMemoryUrl(0x0400,256) returned: ") + url).c_str());
				return;
			}
		}
		// FormatReadMemoryUrl with zero address: should be "0" not "0000"
		{
			std::string url = C64URestClient::FormatReadMemoryUrl(0x0000, 256);
			if (url != "/v1/machine:readmem?address=0&length=256")
			{
				TestCompleted(false, (std::string("FormatReadMemoryUrl(0x0000,256) returned: ") + url).c_str());
				return;
			}
		}
		StepCompleted(1, true, "FormatReadMemoryUrl produces correct uppercase hex without 0x prefix");

		// Step 2: FormatWriteMemoryUrl
		{
			std::string url = C64URestClient::FormatWriteMemoryUrl(0xD020);
			if (url != "/v1/machine:writemem?address=D020")
			{
				TestCompleted(false, (std::string("FormatWriteMemoryUrl(0xD020) returned: ") + url).c_str());
				return;
			}
		}
		StepCompleted(2, true, "FormatWriteMemoryUrl produces correct address format");

		// Step 3: FormatStreamStartUrl with "video" stream name (preserves existing behavior)
		{
			std::string url = C64URestClient::FormatStreamStartUrl("video", "192.168.1.100", 11000);
			if (url != "/v1/streams/video:start?ip=192.168.1.100:11000")
			{
				TestCompleted(false, (std::string("FormatStreamStartUrl(video) returned: ") + url).c_str());
				return;
			}
		}
		StepCompleted(3, true, "FormatStreamStartUrl with video stream includes ip:port parameter");

		// Step 4: FormatStreamStopUrl with "video" stream name
		{
			std::string url = C64URestClient::FormatStreamStopUrl("video");
			if (url != "/v1/streams/video:stop")
			{
				TestCompleted(false, (std::string("FormatStreamStopUrl(video) returned: ") + url).c_str());
				return;
			}
		}
		StepCompleted(4, true, "FormatStreamStopUrl with video stream is correct");

		// Step 4b: FormatStreamStartUrl with "debug" stream name
		{
			std::string url = C64URestClient::FormatStreamStartUrl("debug", "192.168.1.100", 11002);
			if (url != "/v1/streams/debug:start?ip=192.168.1.100:11002")
			{
				TestCompleted(false, (std::string("FormatStreamStartUrl(debug) returned: ") + url).c_str());
				return;
			}
		}
		StepCompleted(4, true, "FormatStreamStartUrl with debug stream uses correct stream name");

		// Step 4c: FormatStreamStopUrl with "debug" stream name
		{
			std::string url = C64URestClient::FormatStreamStopUrl("debug");
			if (url != "/v1/streams/debug:stop")
			{
				TestCompleted(false, (std::string("FormatStreamStopUrl(debug) returned: ") + url).c_str());
				return;
			}
		}
		StepCompleted(4, true, "FormatStreamStopUrl with debug stream uses correct stream name");

		// Step 5: FormatRunPrgUrl
		{
			std::string url = C64URestClient::FormatRunPrgUrl("/path/to/file.prg");
			if (url != "/v1/runners:run_prg?file=/path/to/file.prg")
			{
				TestCompleted(false, (std::string("FormatRunPrgUrl returned: ") + url).c_str());
				return;
			}
		}
		StepCompleted(5, true, "FormatRunPrgUrl appends file path");

		// Step 6: FormatRunCrtUrl
		{
			std::string url = C64URestClient::FormatRunCrtUrl("/carts/game.crt");
			if (url != "/v1/runners:run_crt?file=/carts/game.crt")
			{
				TestCompleted(false, (std::string("FormatRunCrtUrl returned: ") + url).c_str());
				return;
			}
		}
		StepCompleted(6, true, "FormatRunCrtUrl appends file path");

		// Step 7: FormatMountDiskUrl
		{
			std::string url = C64URestClient::FormatMountDiskUrl(0, "/disks/test.d64", "rw");
			if (url != "/v1/drives/a:mount?image=/disks/test.d64&mode=rw")
			{
				TestCompleted(false, (std::string("FormatMountDiskUrl returned: ") + url).c_str());
				return;
			}
		}
		StepCompleted(7, true, "FormatMountDiskUrl formats drive letter and parameters");

		// Step 8: Password header key/value
		{
			std::string key = C64URestClient::GetPasswordHeaderKey();
			if (key != "X-Password")
			{
				TestCompleted(false, (std::string("GetPasswordHeaderKey returned: ") + key).c_str());
				return;
			}
			std::string val = C64URestClient::GetPasswordHeaderValue("secret123");
			if (val != "secret123")
			{
				TestCompleted(false, (std::string("GetPasswordHeaderValue returned: ") + val).c_str());
				return;
			}
			// Empty password
			std::string emptyVal = C64URestClient::GetPasswordHeaderValue("");
			if (!emptyVal.empty())
			{
				TestCompleted(false, "GetPasswordHeaderValue for empty password should be empty");
				return;
			}
		}
		StepCompleted(8, true, "Password header key is X-Password, value is pass-through");

		// Step 9: ParseReadMemoryResponse with canned 256-byte body
		{
			std::string body(256, '\0');
			for (int i = 0; i < 256; i++)
			{
				body[i] = (char)(uint8_t)i;
			}

			std::vector<uint8_t> outData;
			bool ok = C64URestClient::ParseReadMemoryResponse(body, outData);
			if (!ok)
			{
				TestCompleted(false, "ParseReadMemoryResponse failed on 256-byte body");
				return;
			}
			if (outData.size() != 256)
			{
				TestCompleted(false, "ParseReadMemoryResponse output size mismatch");
				return;
			}
			if (outData[0] != 0x00 || outData[127] != 0x7F || outData[255] != 0xFF)
			{
				TestCompleted(false, "ParseReadMemoryResponse data content mismatch");
				return;
			}

			// Empty body should fail
			std::vector<uint8_t> emptyOut;
			bool emptyOk = C64URestClient::ParseReadMemoryResponse("", emptyOut);
			if (emptyOk)
			{
				TestCompleted(false, "ParseReadMemoryResponse should fail on empty body");
				return;
			}
		}
		StepCompleted(9, true, "ParseReadMemoryResponse correctly extracts binary data");

		// Step 10: ParseDeviceInfoResponse with canned JSON
		{
			std::string json = "{\"board\": \"Ultimate 64\", \"version\": \"3.10a\", \"fpga\": \"Artix-7\"}";
			std::string boardType, version;
			bool ok = C64URestClient::ParseDeviceInfoResponse(json, boardType, version);
			if (!ok)
			{
				TestCompleted(false, "ParseDeviceInfoResponse failed on valid JSON");
				return;
			}
			if (boardType != "Ultimate 64")
			{
				TestCompleted(false, (std::string("ParseDeviceInfoResponse board: ") + boardType).c_str());
				return;
			}
			if (version != "3.10a")
			{
				TestCompleted(false, (std::string("ParseDeviceInfoResponse version: ") + version).c_str());
				return;
			}

			// Partial JSON (only board)
			std::string partialJson = "{\"board\": \"U2+L\"}";
			std::string b2, v2;
			bool partialOk = C64URestClient::ParseDeviceInfoResponse(partialJson, b2, v2);
			if (!partialOk || b2 != "U2+L")
			{
				TestCompleted(false, "ParseDeviceInfoResponse should handle partial JSON");
				return;
			}

			// Empty JSON should fail
			std::string emptyB, emptyV;
			bool emptyOk = C64URestClient::ParseDeviceInfoResponse("{}", emptyB, emptyV);
			if (emptyOk)
			{
				TestCompleted(false, "ParseDeviceInfoResponse should fail on empty JSON");
				return;
			}
		}
		StepCompleted(10, true, "ParseDeviceInfoResponse extracts board and version from JSON");

		// Step 11: FormatReadMemoryUrl with high addresses
		{
			std::string url = C64URestClient::FormatReadMemoryUrl(0xFFFF, 1);
			if (url != "/v1/machine:readmem?address=FFFF&length=1")
			{
				TestCompleted(false, (std::string("FormatReadMemoryUrl(0xFFFF,1) returned: ") + url).c_str());
				return;
			}
		}
		StepCompleted(11, true, "FormatReadMemoryUrl handles high addresses correctly");

		// Step 12: FormatWriteMemoryHexUrl
		{
			std::string url = C64URestClient::FormatWriteMemoryHexUrl(0xD020, "0504");
			if (url != "/v1/machine:writemem?address=D020&data=0504")
			{
				TestCompleted(false, (std::string("FormatWriteMemoryHexUrl(0xD020,0504) returned: ") + url).c_str());
				return;
			}
		}
		StepCompleted(12, true, "FormatWriteMemoryHexUrl produces correct address and hex data in URL");

		TestCompleted(true, "All REST protocol URL formatters and response parsers are correct");
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
