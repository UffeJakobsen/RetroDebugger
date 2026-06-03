#ifndef _C64UROMBYPASS_H_
#define _C64UROMBYPASS_H_

#include <cstdint>
#include <vector>
#include <mutex>

class C64URestClient;

class C64URomBypass
{
public:
	C64URomBypass(C64URestClient *restClient);

	// Check if address range overlaps BASIC ($A000-$BFFF) or KERNAL ($E000-$FFFF) ROM
	static bool OverlapsRom(uint16_t addr, uint16_t length);

	// Read RAM under ROM using NMI stub injection
	// Machine must be connected. Returns true on success.
	bool ReadUnderRom(uint16_t srcAddr, uint16_t length, uint8_t *outBuffer);

	// Smart read: uses bypass only when range overlaps ROM, otherwise direct REST readmem
	bool SmartRead(uint16_t addr, uint16_t length, uint8_t *outBuffer);

	// Build the 6502 copy stub (exposed for testing)
	static std::vector<uint8_t> BuildCopyStub(uint16_t src, uint16_t dst, uint16_t length, uint16_t originalNmi);

private:
	bool InjectAndRun(uint16_t srcAddr, uint16_t length, uint8_t *outBuffer);

	C64URestClient *restClient;
	std::mutex bypassMutex;

	// constexpr so these are inline-defined in C++17+ — otherwise std::min
	// odr-uses MAX_COPY_SIZE and the link fails without a separate
	// out-of-class definition.
	static constexpr uint16_t STUB_ADDR = 0x0340;
	static constexpr uint16_t COPY_BUFFER = 0x4000;
	static constexpr uint16_t MAX_COPY_SIZE = 0x2000; // 8KB
};

#endif
