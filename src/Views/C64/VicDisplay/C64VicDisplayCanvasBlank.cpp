#include "C64VicDisplayCanvasBlank.h"
#include "CViewC64VicDisplay.h"
#include "CDebugInterfaceC64.h"
#include "CGuiMain.h"
#include "CViewC64.h"
#include "CImageData.h"

C64VicDisplayCanvasBlank::C64VicDisplayCanvasBlank(CViewC64VicDisplay *vicDisplay)
: C64VicDisplayCanvas(vicDisplay, C64_CANVAS_TYPE_BLANK, false, false)
{
	
}

void C64VicDisplayCanvasBlank::ClearScreen()
{
	LOGWarning("C64VicDisplayCanvasBlank::ClearScreen");
}

void C64VicDisplayCanvasBlank::RefreshScreen(vicii_cycle_state_t *viciiState, CImageData *imageDataScreen,
											 u8 backgroundColorAlpha, u8 foregroundColorAlpha)
{
	this->viciiState = viciiState;

	// refresh texture of C64's character mode screen
	u8 *screen_ptr;
	u8 *color_ram_ptr;
	u8 *chargen_ptr;
	u8 *bitmap_low_ptr;
	u8 *bitmap_high_ptr;
	u8 colors[0x0F];

	vicDisplay->GetViciiPointers(viciiState, &screen_ptr, &color_ram_ptr, &chargen_ptr, &bitmap_low_ptr, &bitmap_high_ptr, colors);

	u8 bgcolor = colors[1];
	u8 bgColorR, bgColorG, bgColorB;
	debugInterface->GetCBMColor(bgcolor, &bgColorR, &bgColorG, &bgColorB);

	// Fill with single uint32_t value instead of per-pixel SetPixelResultRGBA
	uint32_t bgRGBA = (uint32_t)bgColorR | ((uint32_t)bgColorG << 8) | ((uint32_t)bgColorB << 16) | ((uint32_t)backgroundColorAlpha << 24);
	u8 *imageData = (u8 *)imageDataScreen->resultData;
	int imgWidth = imageDataScreen->width;

	for (int y = 0; y < 200; y++)
	{
		uint32_t *row = (uint32_t *)(imageData + y * imgWidth * 4);
		for (int x = 0; x < 320; x++)
		{
			row[x] = bgRGBA;
		}
	}
}

void C64VicDisplayCanvasBlank::RenderCanvasSpecificGridLines()
{
	// blank screen, render only interior screen rectangle
	float cys = vicDisplay->displayPosWithScrollY + 0.0f * vicDisplay->rasterScaleFactorY  + vicDisplay->rasterCrossOffsetY;
//	float cye = vicDisplay->displayPosWithScrollY + 200.0f * vicDisplay->rasterScaleFactorY  + vicDisplay->rasterCrossOffsetY;
	float cysz = 200.0f * vicDisplay->rasterScaleFactorY  + vicDisplay->rasterCrossOffsetY;
	
	float cxs = vicDisplay->displayPosWithScrollX + 0.0f * vicDisplay->rasterScaleFactorX  + vicDisplay->rasterCrossOffsetX;
//	float cxe = vicDisplay->displayPosWithScrollX + 320.0f * vicDisplay->rasterScaleFactorX  + vicDisplay->rasterCrossOffsetX;
	float cxsz = 320.0f * vicDisplay->rasterScaleFactorX  + vicDisplay->rasterCrossOffsetX;

	BlitRectangle(cxs, cys, -1, cxsz, cysz, vicDisplay->gridLinesColorR, vicDisplay->gridLinesColorG, vicDisplay->gridLinesColorB, vicDisplay->gridLinesColorA);
}

void C64VicDisplayCanvasBlank::RenderCanvasSpecificGridValues()
{
	// nothing
}

u8 C64VicDisplayCanvasBlank::GetColorAtPixel(int x, int y)
{
	// nothing
	return 0;
}

void C64VicDisplayCanvasBlank::PutColorAtPixel(int x, int y, u8 paintColor)
{
	// nothing
}

// @returns painting status (ok, replaced color, blocked)
u8 C64VicDisplayCanvasBlank::PutColorAtPixel(bool forceColorReplace, int x, int y, u8 colorLMB, u8 colorRMB, u8 colorSource, int charValue)
{
	return PAINT_RESULT_ERROR;
}

// @returns painting status (ok, replaced color, blocked)
u8 C64VicDisplayCanvasBlank::PaintDither(bool forceColorReplace, int x, int y, u8 colorLMB, u8 colorRMB, u8 colorSource, int charValue)
{
	return PAINT_RESULT_ERROR;
}

