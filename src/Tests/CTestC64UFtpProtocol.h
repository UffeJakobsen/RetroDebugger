#pragma once

#include "CTest.h"
#include "../Emulators/c64u/Transport/C64UFtpClient.h"

#include <string>
#include <vector>

class CTestC64UFtpProtocol : public CTest
{
public:
	virtual const char *GetName() override { return "C64UFtpProtocol"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

		// Step 1: Response code parsing
		{
			if (C64UFtpClient::ParseResponseCode("220 Welcome") != 220)
			{ TestCompleted(false, "ParseResponseCode(220) failed"); return; }
			if (C64UFtpClient::ParseResponseCode("331 User name okay") != 331)
			{ TestCompleted(false, "ParseResponseCode(331) failed"); return; }
			if (C64UFtpClient::ParseResponseCode("230 Login successful") != 230)
			{ TestCompleted(false, "ParseResponseCode(230) failed"); return; }
			if (C64UFtpClient::ParseResponseCode("150 Opening data") != 150)
			{ TestCompleted(false, "ParseResponseCode(150) failed"); return; }
			if (C64UFtpClient::ParseResponseCode("226 Transfer complete") != 226)
			{ TestCompleted(false, "ParseResponseCode(226) failed"); return; }
			if (C64UFtpClient::ParseResponseCode("550 File not found") != 550)
			{ TestCompleted(false, "ParseResponseCode(550) failed"); return; }
			if (C64UFtpClient::ParseResponseCode("") != 0)
			{ TestCompleted(false, "ParseResponseCode(empty) should return 0"); return; }
			if (C64UFtpClient::ParseResponseCode("garbage") != 0)
			{ TestCompleted(false, "ParseResponseCode(garbage) should return 0"); return; }
			if (C64UFtpClient::ParseResponseCode("12") != 0)
			{ TestCompleted(false, "ParseResponseCode(short) should return 0"); return; }
		}
		StepCompleted(1, true, "Response code parsing handles all standard codes and edge cases");

		// Step 2: PASV response parsing
		{
			std::string host;
			int port;

			if (!C64UFtpClient::ParsePasvResponse("227 Entering Passive Mode (192,168,1,64,4,1)", host, port))
			{ TestCompleted(false, "ParsePasvResponse failed on valid input"); return; }
			if (host != "192.168.1.64")
			{ TestCompleted(false, (std::string("PASV host wrong: ") + host).c_str()); return; }
			if (port != 1025)
			{ TestCompleted(false, "PASV port wrong, expected 1025"); return; }

			if (!C64UFtpClient::ParsePasvResponse("227 Entering Passive Mode (10,0,0,1,0,21)", host, port))
			{ TestCompleted(false, "ParsePasvResponse second input failed"); return; }
			if (host != "10.0.0.1")
			{ TestCompleted(false, (std::string("PASV host 2 wrong: ") + host).c_str()); return; }
			if (port != 21)
			{ TestCompleted(false, "PASV port 2 wrong, expected 21"); return; }

			// High port: p1=255, p2=255 → 65535
			if (!C64UFtpClient::ParsePasvResponse("227 Entering Passive Mode (1,2,3,4,255,255)", host, port))
			{ TestCompleted(false, "ParsePasvResponse high port failed"); return; }
			if (port != 65535)
			{ TestCompleted(false, "PASV high port wrong, expected 65535"); return; }

			// Should fail on non-227 response
			if (C64UFtpClient::ParsePasvResponse("200 OK", host, port))
			{ TestCompleted(false, "ParsePasvResponse should reject non-227"); return; }

			// Should fail on missing parentheses
			if (C64UFtpClient::ParsePasvResponse("227 No parens here", host, port))
			{ TestCompleted(false, "ParsePasvResponse should reject missing parens"); return; }
		}
		StepCompleted(2, true, "PASV response parsing extracts host and port correctly");

		// Step 3: LIST output parsing — Unix ls -l format
		{
			std::string listData =
				"drwxr-xr-x  2 root root  4096 Jan 01 00:00 Flash\r\n"
				"-rw-r--r--  1 root root 32768 Jan 01 00:00 game.prg\r\n"
				"-rw-r--r--  1 root root 174848 Feb 15 12:30 demo.d64\r\n"
				"drwxr-xr-x  3 root root  4096 Mar 01 09:00 usb0\r\n";

			auto entries = C64UFtpClient::ParseListOutput(listData);
			if (entries.size() != 4)
			{ TestCompleted(false, ("Expected 4 entries, got " + std::to_string(entries.size())).c_str()); return; }

			if (entries[0].name != "Flash")
			{ TestCompleted(false, ("Entry 0 name: " + entries[0].name).c_str()); return; }
			if (!entries[0].isDirectory)
			{ TestCompleted(false, "Entry 0 should be directory"); return; }

			if (entries[1].name != "game.prg")
			{ TestCompleted(false, ("Entry 1 name: " + entries[1].name).c_str()); return; }
			if (entries[1].isDirectory)
			{ TestCompleted(false, "Entry 1 should be file"); return; }
			if (entries[1].size != 32768)
			{ TestCompleted(false, "Entry 1 size wrong"); return; }

			if (entries[2].size != 174848)
			{ TestCompleted(false, "Entry 2 size wrong"); return; }

			if (entries[3].name != "usb0")
			{ TestCompleted(false, ("Entry 3 name: " + entries[3].name).c_str()); return; }
			if (!entries[3].isDirectory)
			{ TestCompleted(false, "Entry 3 should be directory"); return; }
		}
		StepCompleted(3, true, "LIST output parsing handles Unix ls -l format correctly");

		// Step 4: Empty and edge-case LIST parsing
		{
			auto empty = C64UFtpClient::ParseListOutput("");
			if (!empty.empty())
			{ TestCompleted(false, "Empty should give 0 entries"); return; }

			auto singleLine = C64UFtpClient::ParseListOutput(
				"-rw-r--r--  1 root root 100 Jan 01 00:00 test.prg\r\n");
			if (singleLine.size() != 1)
			{ TestCompleted(false, "Single line count wrong"); return; }
			if (singleLine[0].name != "test.prg")
			{ TestCompleted(false, ("Single line name: " + singleLine[0].name).c_str()); return; }
		}
		StepCompleted(4, true, "Edge-case LIST parsing handles empty and single-line input");

		// Step 5: LIST parsing skips . and .. entries
		{
			std::string listWithDots =
				"drwxr-xr-x  2 root root 4096 Jan 01 00:00 .\r\n"
				"drwxr-xr-x  2 root root 4096 Jan 01 00:00 ..\r\n"
				"-rw-r--r--  1 root root  512 Jan 01 00:00 file.prg\r\n";

			auto entries = C64UFtpClient::ParseListOutput(listWithDots);
			if (entries.size() != 1)
			{ TestCompleted(false, ("Dot entries should be skipped, got " + std::to_string(entries.size())).c_str()); return; }
			if (entries[0].name != "file.prg")
			{ TestCompleted(false, "Entry after dots should be file.prg"); return; }
		}
		StepCompleted(5, true, "LIST parsing skips . and .. entries");

		// Step 6: LIST parsing fallback — simple name-per-line format
		{
			std::string simpleList = "Flash\r\nusb0\r\ngame.prg\r\n";
			auto entries = C64UFtpClient::ParseListOutput(simpleList);
			if (entries.size() != 3)
			{ TestCompleted(false, ("Simple list expected 3 entries, got " + std::to_string(entries.size())).c_str()); return; }
			if (entries[0].name != "Flash")
			{ TestCompleted(false, "Simple list entry 0 name wrong"); return; }
			// In simple mode, we can't determine directory status
			if (entries[0].isDirectory)
			{ TestCompleted(false, "Simple list entries should default to file"); return; }
		}
		StepCompleted(6, true, "LIST parsing handles simple name-per-line fallback format");

		// Step 7: Filenames with spaces in LIST
		{
			std::string listWithSpaces =
				"-rw-r--r--  1 root root 1024 Jan 01 00:00 my game file.prg\r\n";
			auto entries = C64UFtpClient::ParseListOutput(listWithSpaces);
			if (entries.size() != 1)
			{ TestCompleted(false, "Filename with spaces: wrong count"); return; }
			if (entries[0].name != "my game file.prg")
			{ TestCompleted(false, ("Filename with spaces: " + entries[0].name).c_str()); return; }
		}
		StepCompleted(7, true, "LIST parsing handles filenames with spaces");

		TestCompleted(true, "All FTP protocol parsing tests passed");
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
