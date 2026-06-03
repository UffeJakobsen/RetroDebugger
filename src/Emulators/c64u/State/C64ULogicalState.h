#ifndef _C64ULOGICALSTATE_H_
#define _C64ULOGICALSTATE_H_

#include <cstdint>

struct C64UVicState
{
	uint8_t registers[0x40];

	uint8_t GetD011() const { return registers[0x11]; }
	uint8_t GetD016() const { return registers[0x16]; }
	uint8_t GetD018() const { return registers[0x18]; }
	uint8_t GetD020() const { return registers[0x20]; }
	uint8_t GetD021() const { return registers[0x21]; }

	uint16_t GetRasterLine() const
	{
		return ((uint16_t)(registers[0x11] & 0x80) << 1) | registers[0x12];
	}

	// Graphics mode
	const char *GetModeName() const
	{
		bool ecm = (registers[0x11] >> 6) & 1;
		bool bmm = (registers[0x11] >> 5) & 1;
		bool mcm = (registers[0x16] >> 4) & 1;
		if (ecm && !bmm && !mcm) return "Extended BG Color";
		if (!ecm && bmm && !mcm) return "Hi-Res Bitmap";
		if (!ecm && bmm && mcm)  return "Multicolor Bitmap";
		if (!ecm && !bmm && mcm) return "Multicolor Text";
		if (!ecm && !bmm && !mcm) return "Standard Text";
		return "Invalid Mode";
	}

	bool IsBitmapMode() const { return (registers[0x11] >> 5) & 1; }
	bool IsMulticolorMode() const { return (registers[0x16] >> 4) & 1; }
	bool IsExtColorMode() const { return (registers[0x11] >> 6) & 1; }
	bool IsDisplayEnabled() const { return (registers[0x11] >> 4) & 1; }
	uint8_t GetYScroll() const { return registers[0x11] & 0x07; }
	uint8_t GetXScroll() const { return registers[0x16] & 0x07; }

	// Sprites
	bool IsSpriteEnabled(int n) const { return (registers[0x15] >> n) & 1; }
	uint16_t GetSpriteX(int n) const
	{
		uint16_t x = registers[n * 2];
		if ((registers[0x10] >> n) & 1) x |= 0x100;
		return x;
	}
	uint8_t GetSpriteY(int n) const { return registers[1 + n * 2]; }
	bool IsSpriteMulticolor(int n) const { return (registers[0x1C] >> n) & 1; }
	bool IsSpriteXExpand(int n) const { return (registers[0x1D] >> n) & 1; }
	bool IsSpriteYExpand(int n) const { return (registers[0x17] >> n) & 1; }
	bool IsSpriteBgPriority(int n) const { return (registers[0x1B] >> n) & 1; }

	// Colors
	uint8_t GetBorderColor() const { return registers[0x20] & 0x0F; }
	uint8_t GetBackgroundColor(int n = 0) const { return registers[0x21 + n] & 0x0F; }
	uint8_t GetSpriteMulticolor0() const { return registers[0x25] & 0x0F; }
	uint8_t GetSpriteMulticolor1() const { return registers[0x26] & 0x0F; }
	uint8_t GetSpriteColor(int n) const { return registers[0x27 + n] & 0x0F; }
};

struct C64UCiaState
{
	uint8_t registers[0x10];
};

struct C64USidState
{
	uint8_t registers[0x20];
};

struct C64UBankState
{
	uint8_t processorPort01;
	bool exrom;
	bool game;

	uint16_t screenAddress;
	uint16_t charsetAddress;
	uint16_t bitmapAddress;

	uint16_t GetVicBank() const
	{
		// VIC bank derived from CIA2 port A
		// This is already computed in DeriveBank, just make it accessible
		return screenAddress & 0xC000;
	}
};

struct C64ULogicalState
{
	C64UVicState vic;
	C64UCiaState cia1;
	C64UCiaState cia2;
	C64USidState sid;
	C64UBankState bank;
};

#endif
