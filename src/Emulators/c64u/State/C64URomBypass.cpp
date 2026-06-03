#include "C64URomBypass.h"
#include "../Transport/C64URestClient.h"
#include "SYS_Main.h"
#include "DBG_Log.h"

#include <cstring>
#include <algorithm>

#define LOGU(...) LOGD(__VA_ARGS__)

C64URomBypass::C64URomBypass(C64URestClient *restClient)
	: restClient(restClient)
{
}

bool C64URomBypass::OverlapsRom(uint16_t addr, uint16_t length)
{
	if (length == 0)
		return false;

	uint32_t rangeStart = (uint32_t)addr;
	uint32_t rangeEnd = rangeStart + (uint32_t)length; // exclusive end, may be up to 0x10000

	// BASIC ROM: $A000-$BFFF (range [$A000, $C000))
	if (rangeStart < 0xC000 && rangeEnd > 0xA000)
		return true;

	// KERNAL ROM: $E000-$FFFF (range [$E000, $10000))
	if (rangeStart < 0x10000 && rangeEnd > 0xE000)
		return true;

	return false;
}

std::vector<uint8_t> C64URomBypass::BuildCopyStub(uint16_t src, uint16_t dst, uint16_t length, uint16_t originalNmi)
{
	// 6502 copy stub ported from ultimate64-manager screenshot_api.rs
	//
	// The stub runs as an NMI handler:
	// - Saves A/X/Y on stack, saves processor port $01
	// - Banks out all ROMs (LDA #$34, STA $01)
	// - Sets up ZP pointers ($FB/$FC = src, $FD/$FE = dst)
	// - Page-loop copy: outer iterates pages, inner copies 256 bytes
	// - Restores $01, writes completion marker $42 to $02
	// - Restores A/X/Y, jumps to original NMI handler

	uint8_t numPages = (uint8_t)((length + 0xFF) >> 8);
	uint8_t srcLo = (uint8_t)(src & 0xFF);
	uint8_t srcHi = (uint8_t)((src >> 8) & 0xFF);
	uint8_t dstLo = (uint8_t)(dst & 0xFF);
	uint8_t dstHi = (uint8_t)((dst >> 8) & 0xFF);
	uint8_t nmiLo = (uint8_t)(originalNmi & 0xFF);
	uint8_t nmiHi = (uint8_t)((originalNmi >> 8) & 0xFF);

	std::vector<uint8_t> code;
	code.reserve(64);

	// Save registers
	code.push_back(0x48);        // PHA
	code.push_back(0x8A);        // TXA
	code.push_back(0x48);        // PHA
	code.push_back(0x98);        // TYA
	code.push_back(0x48);        // PHA

	// Save processor port and bank out ROMs
	code.push_back(0xA5);        // LDA $01
	code.push_back(0x01);
	code.push_back(0x48);        // PHA
	code.push_back(0xA9);        // LDA #$34
	code.push_back(0x34);
	code.push_back(0x85);        // STA $01
	code.push_back(0x01);

	// Set up ZP pointers: $FB/$FC = src, $FD/$FE = dst
	code.push_back(0xA9);        // LDA #srcLo
	code.push_back(srcLo);
	code.push_back(0x85);        // STA $FB
	code.push_back(0xFB);
	code.push_back(0xA9);        // LDA #srcHi
	code.push_back(srcHi);
	code.push_back(0x85);        // STA $FC
	code.push_back(0xFC);
	code.push_back(0xA9);        // LDA #dstLo
	code.push_back(dstLo);
	code.push_back(0x85);        // STA $FD
	code.push_back(0xFD);
	code.push_back(0xA9);        // LDA #dstHi
	code.push_back(dstHi);
	code.push_back(0x85);        // STA $FE
	code.push_back(0xFE);

	// LDX #numPages (page counter)
	code.push_back(0xA2);        // LDX #numPages
	code.push_back(numPages);

	// outer: LDY #$00
	code.push_back(0xA0);        // LDY #$00
	code.push_back(0x00);

	// inner: LDA ($FB),Y
	code.push_back(0xB1);        // LDA ($FB),Y
	code.push_back(0xFB);

	// STA ($FD),Y
	code.push_back(0x91);        // STA ($FD),Y
	code.push_back(0xFD);

	// INY
	code.push_back(0xC8);        // INY

	// BNE inner: branch back to LDA ($FB),Y
	// Inner loop body: LDA(2) + STA(2) + INY(1) + BNE(2) = 7 bytes
	// PC after BNE = BNE_addr + 2; target = BNE_addr - 5
	// displacement = -7 = 0xF9
	code.push_back(0xD0);        // BNE inner
	code.push_back(0xF9);        // displacement -7

	// INC $FC (src high byte)
	code.push_back(0xE6);        // INC $FC
	code.push_back(0xFC);

	// INC $FE (dst high byte)
	code.push_back(0xE6);        // INC $FE
	code.push_back(0xFE);

	// DEX
	code.push_back(0xCA);        // DEX

	// BNE outer: branch back to LDY #$00
	// Outer loop body: LDY(2) + inner(7) + INC(2) + INC(2) + DEX(1) + BNE(2) = 16 bytes
	// PC after BNE = BNE_addr + 2; target = BNE_addr - 14
	// displacement = -16 = 0xF0
	code.push_back(0xD0);        // BNE outer
	code.push_back(0xF0);        // displacement -16

	// Restore processor port
	code.push_back(0x68);        // PLA
	code.push_back(0x85);        // STA $01
	code.push_back(0x01);

	// Write completion marker: LDA #$42, STA $02
	code.push_back(0xA9);        // LDA #$42
	code.push_back(0x42);
	code.push_back(0x85);        // STA $02
	code.push_back(0x02);

	// Restore registers
	code.push_back(0x68);        // PLA
	code.push_back(0xA8);        // TAY
	code.push_back(0x68);        // PLA
	code.push_back(0xAA);        // TAX
	code.push_back(0x68);        // PLA

	// JMP originalNmi
	code.push_back(0x4C);        // JMP
	code.push_back(nmiLo);
	code.push_back(nmiHi);

	return code;
}

bool C64URomBypass::ReadUnderRom(uint16_t srcAddr, uint16_t length, uint8_t *outBuffer)
{
	if (restClient == NULL || outBuffer == NULL || length == 0)
		return false;

	std::lock_guard<std::mutex> lock(bypassMutex);

	LOGU("C64URomBypass::ReadUnderRom: srcAddr=$%04X length=%d", srcAddr, length);

	// For reads > MAX_COPY_SIZE, split into chunks
	uint16_t offset = 0;
	while (offset < length)
	{
		uint16_t chunkSize = std::min((uint16_t)(length - offset), MAX_COPY_SIZE);
		uint16_t chunkAddr = srcAddr + offset;

		if (!InjectAndRun(chunkAddr, chunkSize, outBuffer + offset))
		{
			LOGU("C64URomBypass::ReadUnderRom: chunk at $%04X failed", chunkAddr);
			return false;
		}

		offset += chunkSize;
	}

	LOGU("C64URomBypass::ReadUnderRom: completed successfully");
	return true;
}

bool C64URomBypass::SmartRead(uint16_t addr, uint16_t length, uint8_t *outBuffer)
{
	if (restClient == NULL || outBuffer == NULL || length == 0)
		return false;

	if (!OverlapsRom(addr, length))
	{
		// No ROM overlap -- use direct REST readmem
		LOGU("C64URomBypass::SmartRead: no ROM overlap at $%04X len=%d, using REST readmem", addr, length);
		restClient->ScheduleReadMemory(addr, length);
		// NOTE: REST client is async. Caller must poll GetLastResult() or use memory cache.
		// Return true to indicate the request was scheduled.
		return true;
	}

	// ROM overlap -- use bypass
	LOGU("C64URomBypass::SmartRead: ROM overlap at $%04X len=%d, using bypass", addr, length);
	return ReadUnderRom(addr, length, outBuffer);
}

bool C64URomBypass::InjectAndRun(uint16_t srcAddr, uint16_t length, uint8_t *outBuffer)
{
	// Full bypass sequence:
	// 1. Pause machine
	// 2. Backup all memory areas we will modify
	// 3. Build and inject copy stub
	// 4. Redirect NMI vector to stub
	// 5. Set up CIA2 timer to trigger NMI
	// 6. Resume machine briefly
	// 7. Sleep to allow stub to execute
	// 8. Pause machine
	// 9. Verify completion marker
	// 10. Read copied data from buffer
	// 11. Restore all backed-up memory

	LOGU("C64URomBypass::InjectAndRun: src=$%04X len=%d", srcAddr, length);

	// Step 1: Pause
	restClient->SchedulePause();
	SYS_Sleep(200);

	// Step 2: Backup memory areas

	// Read NMI vector to get original handler address
	restClient->ScheduleReadMemory(0x0318, 2);
	SYS_Sleep(200);
	C64URestCommandResult nmiResult = restClient->GetLastResult();
	uint16_t originalNmi = 0xFE47; // default KERNAL NMI handler
	if (nmiResult.success && nmiResult.body.size() >= 2)
	{
		originalNmi = nmiResult.body[0] | ((uint16_t)nmiResult.body[1] << 8);
	}
	LOGU("C64URomBypass::InjectAndRun: original NMI=$%04X", originalNmi);

	// Backup stub area (128 bytes at $0340)
	restClient->ScheduleReadMemory(STUB_ADDR, 128);
	SYS_Sleep(200);
	C64URestCommandResult stubBackup = restClient->GetLastResult();

	// Backup copy buffer (length bytes at $4000)
	restClient->ScheduleReadMemory(COPY_BUFFER, length);
	SYS_Sleep(200);
	C64URestCommandResult bufferBackup = restClient->GetLastResult();

	// Backup ZP $FB-$FE
	restClient->ScheduleReadMemory(0x00FB, 4);
	SYS_Sleep(200);
	C64URestCommandResult zpBackup = restClient->GetLastResult();

	// Backup marker $02
	restClient->ScheduleReadMemory(0x0002, 1);
	SYS_Sleep(100);
	C64URestCommandResult markerBackup = restClient->GetLastResult();

	// Backup CIA2 timer $DD04-$DD06 (lo, hi, control)
	restClient->ScheduleReadMemory(0xDD04, 3);
	SYS_Sleep(100);
	C64URestCommandResult ciaTimerBackup = restClient->GetLastResult();

	// Backup CIA2 ICR $DD0D
	restClient->ScheduleReadMemory(0xDD0D, 1);
	SYS_Sleep(100);
	C64URestCommandResult ciaIcrBackup = restClient->GetLastResult();

	// Step 3: Build and inject the copy stub
	std::vector<uint8_t> stub = BuildCopyStub(srcAddr, COPY_BUFFER, length, originalNmi);
	LOGU("C64URomBypass::InjectAndRun: stub size=%d bytes", (int)stub.size());

	restClient->ScheduleWriteMemory(STUB_ADDR, stub.data(), (int)stub.size());
	SYS_Sleep(100);

	// Step 4: Redirect NMI vector to stub
	uint8_t nmiVector[2];
	nmiVector[0] = (uint8_t)(STUB_ADDR & 0xFF);
	nmiVector[1] = (uint8_t)((STUB_ADDR >> 8) & 0xFF);
	restClient->ScheduleWriteMemory(0x0318, nmiVector, 2);
	SYS_Sleep(100);

	// Step 5: Clear completion marker
	uint8_t zero = 0x00;
	restClient->ScheduleWriteMemory(0x0002, &zero, 1);
	SYS_Sleep(50);

	// Acknowledge pending CIA2 IRQs by reading $DD0D
	restClient->ScheduleReadMemory(0xDD0D, 1);
	SYS_Sleep(50);

	// Set CIA2 Timer A: $0002 lo, $0000 hi (2 cycles)
	uint8_t timerValue[2] = { 0x02, 0x00 };
	restClient->ScheduleWriteMemory(0xDD04, timerValue, 2);
	SYS_Sleep(50);

	// Enable CIA2 NMI: write $81 to $DD0D (bit 7 = set, bit 0 = Timer A)
	uint8_t enableNmi = 0x81;
	restClient->ScheduleWriteMemory(0xDD0D, &enableNmi, 1);
	SYS_Sleep(50);

	// Start timer: write $11 to $DD0E (bit 0 = start, bit 4 = force load)
	uint8_t startTimer = 0x11;
	restClient->ScheduleWriteMemory(0xDD0E, &startTimer, 1);
	SYS_Sleep(100);

	// Step 6: Resume machine
	restClient->ScheduleResume();

	// Step 7: Sleep 600ms to allow the stub to execute
	SYS_Sleep(600);

	// Step 8: Pause machine again
	restClient->SchedulePause();
	SYS_Sleep(200);

	// Step 9: Verify completion marker at $02
	restClient->ScheduleReadMemory(0x0002, 1);
	SYS_Sleep(200);
	C64URestCommandResult markerResult = restClient->GetLastResult();
	if (markerResult.success && markerResult.body.size() >= 1)
	{
		if (markerResult.body[0] != 0x42)
		{
			LOGU("C64URomBypass::InjectAndRun: WARNING completion marker=$%02X expected=$42", markerResult.body[0]);
		}
		else
		{
			LOGU("C64URomBypass::InjectAndRun: completion marker verified OK");
		}
	}
	else
	{
		LOGU("C64URomBypass::InjectAndRun: WARNING could not read completion marker");
	}

	// Step 10: Read copied data from buffer at $4000
	restClient->ScheduleReadMemory(COPY_BUFFER, length);
	SYS_Sleep(200);
	C64URestCommandResult dataResult = restClient->GetLastResult();
	bool success = false;
	if (dataResult.success && (int)dataResult.body.size() >= length)
	{
		memcpy(outBuffer, dataResult.body.data(), length);
		success = true;
		LOGU("C64URomBypass::InjectAndRun: read %d bytes from copy buffer", length);
	}
	else
	{
		LOGU("C64URomBypass::InjectAndRun: ERROR failed to read copied data");
	}

	// Step 11: Restore all backed-up memory (always restore, even on failure)

	// Disable CIA2 NMI sources
	uint8_t disableNmi = 0x01;
	restClient->ScheduleWriteMemory(0xDD0D, &disableNmi, 1);
	SYS_Sleep(50);

	// Restore CIA2 timer
	if (ciaTimerBackup.success && ciaTimerBackup.body.size() >= 3)
	{
		restClient->ScheduleWriteMemory(0xDD04, ciaTimerBackup.body.data(), 2);
		SYS_Sleep(50);
		restClient->ScheduleWriteMemory(0xDD0E, ciaTimerBackup.body.data() + 2, 1);
		SYS_Sleep(50);
	}

	// Restore CIA2 ICR (set original bits with bit 7 = "set" mode)
	if (ciaIcrBackup.success && ciaIcrBackup.body.size() >= 1)
	{
		uint8_t restoreIcr = ciaIcrBackup.body[0] | 0x80;
		restClient->ScheduleWriteMemory(0xDD0D, &restoreIcr, 1);
		SYS_Sleep(50);
	}

	// Restore NMI vector
	if (nmiResult.success && nmiResult.body.size() >= 2)
	{
		restClient->ScheduleWriteMemory(0x0318, nmiResult.body.data(), 2);
		SYS_Sleep(50);
	}

	// Restore ZP $FB-$FE
	if (zpBackup.success && zpBackup.body.size() >= 4)
	{
		restClient->ScheduleWriteMemory(0x00FB, zpBackup.body.data(), 4);
		SYS_Sleep(50);
	}

	// Restore marker $02
	if (markerBackup.success && markerBackup.body.size() >= 1)
	{
		restClient->ScheduleWriteMemory(0x0002, markerBackup.body.data(), 1);
		SYS_Sleep(50);
	}

	// Restore stub area
	if (stubBackup.success && stubBackup.body.size() >= 128)
	{
		restClient->ScheduleWriteMemory(STUB_ADDR, stubBackup.body.data(), 128);
		SYS_Sleep(50);
	}

	// Restore copy buffer
	if (bufferBackup.success && (int)bufferBackup.body.size() >= length)
	{
		restClient->ScheduleWriteMemory(COPY_BUFFER, bufferBackup.body.data(), length);
		SYS_Sleep(50);
	}

	LOGU("C64URomBypass::InjectAndRun: restore complete, success=%d", success);
	return success;
}
