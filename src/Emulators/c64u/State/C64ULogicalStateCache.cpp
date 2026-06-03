#include "C64ULogicalStateCache.h"
#include "C64UMemoryCache.h"
#include <cstring>

C64ULogicalStateCache::C64ULogicalStateCache()
	: memoryCache(nullptr)
{
	memset(&state, 0, sizeof(state));
}

void C64ULogicalStateCache::SetMemoryCache(C64UMemoryCache *cache)
{
	memoryCache = cache;
}

void C64ULogicalStateCache::RefreshFromMemory()
{
	if (memoryCache == nullptr)
		return;

	std::lock_guard<std::mutex> lock(stateMutex);

	// VIC-II registers at $D000-$D03F
	for (int i = 0; i < 0x40; i++)
		state.vic.registers[i] = memoryCache->ReadByte(0xD000 + i);

	// CIA1 at $DC00-$DC0F
	for (int i = 0; i < 0x10; i++)
		state.cia1.registers[i] = memoryCache->ReadByte(0xDC00 + i);

	// CIA2 at $DD00-$DD0F
	for (int i = 0; i < 0x10; i++)
		state.cia2.registers[i] = memoryCache->ReadByte(0xDD00 + i);

	// SID at $D400-$D41F
	for (int i = 0; i < 0x20; i++)
		state.sid.registers[i] = memoryCache->ReadByte(0xD400 + i);

	// Processor port
	state.bank.processorPort01 = memoryCache->ReadByte(0x0001);

	DeriveBank();
}

void C64ULogicalStateCache::FillWithTestPattern()
{
	std::lock_guard<std::mutex> lock(stateMutex);

	// Set recognizable VIC test values
	state.vic.registers[0x11] = 0x1B;  // D011
	state.vic.registers[0x16] = 0xC8;  // D016
	state.vic.registers[0x18] = 0x14;  // D018 -> screen at $0400, charset at $1000
	state.vic.registers[0x20] = 0x0E;  // D020 border = light blue
	state.vic.registers[0x21] = 0x06;  // D021 bg = blue

	// CIA defaults
	memset(state.cia1.registers, 0, sizeof(state.cia1.registers));
	memset(state.cia2.registers, 0, sizeof(state.cia2.registers));
	state.cia2.registers[0x00] = 0x03;  // CIA2 PRA: VIC bank 0

	// SID zeros
	memset(state.sid.registers, 0, sizeof(state.sid.registers));

	// Bank
	state.bank.processorPort01 = 0x37;
	state.bank.exrom = true;
	state.bank.game = true;

	DeriveBank();
}

void C64ULogicalStateCache::DeriveBank()
{
	uint8_t d018 = state.vic.registers[0x18];
	uint8_t cia2Pra = state.cia2.registers[0x00];
	uint16_t vicBank = (uint16_t)(3 - (cia2Pra & 0x03)) * 0x4000;

	state.bank.screenAddress = vicBank + (uint16_t)((d018 >> 4) & 0x0F) * 0x0400;
	state.bank.charsetAddress = vicBank + (uint16_t)((d018 >> 1) & 0x07) * 0x0800;
	state.bank.bitmapAddress = vicBank + (uint16_t)((d018 >> 3) & 0x01) * 0x2000;
}

C64ULogicalState C64ULogicalStateCache::GetState() const
{
	std::lock_guard<std::mutex> lock(stateMutex);
	return state;
}

C64UVicState C64ULogicalStateCache::GetVicState() const
{
	std::lock_guard<std::mutex> lock(stateMutex);
	return state.vic;
}

C64UCiaState C64ULogicalStateCache::GetCia1State() const
{
	std::lock_guard<std::mutex> lock(stateMutex);
	return state.cia1;
}

C64UCiaState C64ULogicalStateCache::GetCia2State() const
{
	std::lock_guard<std::mutex> lock(stateMutex);
	return state.cia2;
}

C64USidState C64ULogicalStateCache::GetSidState() const
{
	std::lock_guard<std::mutex> lock(stateMutex);
	return state.sid;
}

C64UBankState C64ULogicalStateCache::GetBankState() const
{
	std::lock_guard<std::mutex> lock(stateMutex);
	return state.bank;
}
