#ifndef _C64UNYBBLELUT_H_
#define _C64UNYBBLELUT_H_

#include <cstdint>

// Pre-computed lookup table that converts a packed nybble byte (two C64 palette
// indices) into 8 bytes of RGBA output.  For packed byte `i`:
//   lo = i & 0x0F  -> left pixel
//   hi = (i >> 4)  -> right pixel
//   table[i] = { palette[lo].r, palette[lo].g, palette[lo].b, 0xFF,
//                palette[hi].r, palette[hi].g, palette[hi].b, 0xFF }

struct C64UNybbleLUT
{
	uint8_t table[256][8];

	static constexpr uint8_t PALETTE[16][3] = {
		{0x00, 0x00, 0x00},  //  0 black
		{0xFF, 0xFF, 0xFF},  //  1 white
		{0x68, 0x37, 0x2B},  //  2 red
		{0x70, 0xA4, 0xB2},  //  3 cyan
		{0x6F, 0x3D, 0x86},  //  4 purple
		{0x58, 0x8D, 0x43},  //  5 green
		{0x35, 0x28, 0x79},  //  6 blue
		{0xB8, 0xC7, 0x6F},  //  7 yellow
		{0x6F, 0x4F, 0x25},  //  8 orange
		{0x43, 0x39, 0x00},  //  9 brown
		{0x9A, 0x67, 0x59},  // 10 light red
		{0x44, 0x44, 0x44},  // 11 dark grey
		{0x6C, 0x6C, 0x6C},  // 12 grey
		{0x9A, 0xD2, 0x84},  // 13 light green
		{0x6C, 0x5E, 0xB5},  // 14 light blue
		{0x95, 0x95, 0x95},  // 15 light grey
	};

	void Build()
	{
		BuildFromPalette(PALETTE);
	}

	void BuildFromPalette(const uint8_t externalPalette[16][3])
	{
		for (int i = 0; i < 256; i++)
		{
			int lo = i & 0x0F;
			int hi = (i >> 4) & 0x0F;
			table[i][0] = externalPalette[lo][0];
			table[i][1] = externalPalette[lo][1];
			table[i][2] = externalPalette[lo][2];
			table[i][3] = 0xFF;
			table[i][4] = externalPalette[hi][0];
			table[i][5] = externalPalette[hi][1];
			table[i][6] = externalPalette[hi][2];
			table[i][7] = 0xFF;
		}
	}
};

#endif
