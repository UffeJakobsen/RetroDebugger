#ifndef _CGT2FontAtlas_H_
#define _CGT2FontAtlas_H_

#include "SYS_Defs.h"
#include "imgui.h"

class CSlrImage;
class CImageData;

#define GT2_CHAR_W 8
#define GT2_CHAR_H 16
#define GT2_ATLAS_COLS 16
#define GT2_ATLAS_ROWS 16
#define GT2_ATLAS_W (GT2_ATLAS_COLS * GT2_CHAR_W)   // 128
#define GT2_ATLAS_H (GT2_ATLAS_ROWS * GT2_CHAR_H)   // 256
#define GT2_NUM_COLORS 16

class CGT2FontAtlas
{
public:
	CGT2FontAtlas();
	~CGT2FontAtlas();

	bool Load();
	bool isLoaded;
	bool TryLoad();  // lazy init — tries to load if not yet loaded, returns true if ready
	void BuildPalette();

	// Texture
	CImageData *imageData;
	CSlrImage *image;

	// UV coords per char
	float uvX1[256], uvY1[256], uvX2[256], uvY2[256];

	// Color palette (16 GT2 colors as ImU32)
	ImU32 palette[GT2_NUM_COLORS];
};

#endif
