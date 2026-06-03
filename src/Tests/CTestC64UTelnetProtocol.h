#pragma once

#include "CTest.h"
#include "../Emulators/c64u/Transport/C64UTelnetClient.h"

#include <string>
#include <vector>
#include <cstring>

class CTestC64UTelnetProtocol : public CTest
{
public:
	virtual const char *GetName() override { return "C64UTelnetProtocol"; }

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

		// Step 1: Basic IAC stripping — mixed data with WILL ECHO
		{
			// "Hello" + IAC WILL ECHO + "World"
			uint8_t input[] = { 'H', 'e', 'l', 'l', 'o',
								0xFF, 0xFB, 0x01,
								'W', 'o', 'r', 'l', 'd' };
			std::vector<uint8_t> clean;
			std::vector<std::vector<uint8_t>> cmds;
			C64UTelnetClient::StripIAC(input, sizeof(input), clean, cmds);

			std::string cleanStr(clean.begin(), clean.end());
			if (cleanStr != "HelloWorld")
			{ TestCompleted(false, ("StripIAC basic: expected 'HelloWorld', got '" + cleanStr + "'").c_str()); return; }
			if (cmds.size() != 1)
			{ TestCompleted(false, "StripIAC basic: expected 1 IAC command"); return; }
			if (cmds[0].size() != 2 || cmds[0][0] != 0xFB || cmds[0][1] != 0x01)
			{ TestCompleted(false, "StripIAC basic: IAC command should be WILL ECHO"); return; }
		}
		StepCompleted(1, true, "Basic IAC stripping separates clean data from WILL ECHO");

		// Step 2: Multiple IAC commands in sequence
		{
			// IAC WILL ECHO + IAC WILL SGA + IAC DO TTYPE
			uint8_t input[] = { 0xFF, 0xFB, 0x01,
								0xFF, 0xFB, 0x03,
								0xFF, 0xFD, 0x18 };
			std::vector<uint8_t> clean;
			std::vector<std::vector<uint8_t>> cmds;
			C64UTelnetClient::StripIAC(input, sizeof(input), clean, cmds);

			if (!clean.empty())
			{ TestCompleted(false, "StripIAC multi: expected no clean data"); return; }
			if (cmds.size() != 3)
			{ TestCompleted(false, ("StripIAC multi: expected 3 cmds, got " + std::to_string(cmds.size())).c_str()); return; }
			// WILL ECHO
			if (cmds[0][0] != 0xFB || cmds[0][1] != 0x01)
			{ TestCompleted(false, "StripIAC multi: cmd 0 should be WILL ECHO"); return; }
			// WILL SGA
			if (cmds[1][0] != 0xFB || cmds[1][1] != 0x03)
			{ TestCompleted(false, "StripIAC multi: cmd 1 should be WILL SGA"); return; }
			// DO TTYPE
			if (cmds[2][0] != 0xFD || cmds[2][1] != 0x18)
			{ TestCompleted(false, "StripIAC multi: cmd 2 should be DO TTYPE"); return; }
		}
		StepCompleted(2, true, "Multiple IAC commands parsed correctly");

		// Step 3: Escaped IAC (0xFF 0xFF → literal 0xFF in output)
		{
			uint8_t input[] = { 'A', 0xFF, 0xFF, 'B' };
			std::vector<uint8_t> clean;
			std::vector<std::vector<uint8_t>> cmds;
			C64UTelnetClient::StripIAC(input, sizeof(input), clean, cmds);

			if (clean.size() != 3)
			{ TestCompleted(false, ("StripIAC escaped: expected 3 bytes, got " + std::to_string(clean.size())).c_str()); return; }
			if (clean[0] != 'A' || clean[1] != 0xFF || clean[2] != 'B')
			{ TestCompleted(false, "StripIAC escaped: should be A, 0xFF, B"); return; }
			if (!cmds.empty())
			{ TestCompleted(false, "StripIAC escaped: should have no IAC commands"); return; }
		}
		StepCompleted(3, true, "Escaped IAC (0xFF 0xFF) produces literal 0xFF");

		// Step 4: Subnegotiation parsing (TTYPE SEND)
		{
			// IAC SB TTYPE SEND IAC SE → subneg {TTYPE, 1}
			uint8_t input[] = { 0xFF, 0xFA, 0x18, 0x01, 0xFF, 0xF0 };
			std::vector<uint8_t> clean;
			std::vector<std::vector<uint8_t>> cmds;
			C64UTelnetClient::StripIAC(input, sizeof(input), clean, cmds);

			if (!clean.empty())
			{ TestCompleted(false, "StripIAC subneg: expected no clean data"); return; }
			if (cmds.size() != 1)
			{ TestCompleted(false, ("StripIAC subneg: expected 1 cmd, got " + std::to_string(cmds.size())).c_str()); return; }
			// Subneg command: {SB, TTYPE, SEND} → stored as {0xFA, 0x18, 0x01}
			if (cmds[0].size() != 3 || cmds[0][0] != 0xFA || cmds[0][1] != 0x18 || cmds[0][2] != 0x01)
			{ TestCompleted(false, "StripIAC subneg: cmd should be SB+TTYPE+SEND"); return; }
		}
		StepCompleted(4, true, "Subnegotiation (TTYPE SEND) parsed correctly");

		// Step 5: WONT and DONT commands
		{
			// IAC WONT ECHO + IAC DONT SGA
			uint8_t input[] = { 0xFF, 0xFC, 0x01,
								0xFF, 0xFE, 0x03 };
			std::vector<uint8_t> clean;
			std::vector<std::vector<uint8_t>> cmds;
			C64UTelnetClient::StripIAC(input, sizeof(input), clean, cmds);

			if (!clean.empty())
			{ TestCompleted(false, "StripIAC wont/dont: expected no clean data"); return; }
			if (cmds.size() != 2)
			{ TestCompleted(false, "StripIAC wont/dont: expected 2 cmds"); return; }
			if (cmds[0][0] != 0xFC || cmds[0][1] != 0x01)
			{ TestCompleted(false, "StripIAC wont/dont: cmd 0 should be WONT ECHO"); return; }
			if (cmds[1][0] != 0xFE || cmds[1][1] != 0x03)
			{ TestCompleted(false, "StripIAC wont/dont: cmd 1 should be DONT SGA"); return; }
		}
		StepCompleted(5, true, "WONT and DONT commands parsed correctly");

		// Step 6: Empty input
		{
			std::vector<uint8_t> clean;
			std::vector<std::vector<uint8_t>> cmds;
			C64UTelnetClient::StripIAC(nullptr, 0, clean, cmds);

			if (!clean.empty() || !cmds.empty())
			{ TestCompleted(false, "StripIAC empty: expected empty results"); return; }
		}
		StepCompleted(6, true, "Empty input handled correctly");

		// Step 7: Data with interleaved IAC and clean bytes
		{
			// "AB" + IAC DO NAWS + "CD" + IAC WILL SGA + "EF"
			uint8_t input[] = { 'A', 'B',
								0xFF, 0xFD, 0x1F,
								'C', 'D',
								0xFF, 0xFB, 0x03,
								'E', 'F' };
			std::vector<uint8_t> clean;
			std::vector<std::vector<uint8_t>> cmds;
			C64UTelnetClient::StripIAC(input, sizeof(input), clean, cmds);

			std::string cleanStr(clean.begin(), clean.end());
			if (cleanStr != "ABCDEF")
			{ TestCompleted(false, ("StripIAC interleaved: expected 'ABCDEF', got '" + cleanStr + "'").c_str()); return; }
			if (cmds.size() != 2)
			{ TestCompleted(false, "StripIAC interleaved: expected 2 cmds"); return; }
			// DO NAWS
			if (cmds[0][0] != 0xFD || cmds[0][1] != 0x1F)
			{ TestCompleted(false, "StripIAC interleaved: cmd 0 should be DO NAWS"); return; }
			// WILL SGA
			if (cmds[1][0] != 0xFB || cmds[1][1] != 0x03)
			{ TestCompleted(false, "StripIAC interleaved: cmd 1 should be WILL SGA"); return; }
		}
		StepCompleted(7, true, "Interleaved IAC and clean data handled correctly");

		TestCompleted(true, "All telnet protocol parsing tests passed");
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
