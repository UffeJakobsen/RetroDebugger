#pragma once

#include "CTest.h"

#include "C64Tools.h"
#include "CImageData.h"
#include "CDebugInterfaceC64.h"
#include "CDebugInterfaceC64U.h"
#include "CDataAdapter.h"
#include "C64ULogicalState.h"
#include "CViewC64.h"
#include "C64Palette.h"
#include "C64SettingsStorage.h"

#include <cstdio>
#include <cstring>

// Declared here first so the test can go red before the production header is updated.
void CopyHiresBitmapToImage(u8 *bitmapData, u8 *screenData,
					CImageData *imageData,
					u8 colorBg, CDebugInterfaceC64 *debugInterface);

void CopyMultiBitmapToImage(u8 *bitmapData, u8 *screenData, u8 *colorData,
					CImageData *imageData,
					u8 d021, CDebugInterfaceC64 *debugInterface);

void CopyCharsetFromAdapterToImage(CDataAdapter *dataAdapter, int charsetAddr,
					   CImageData *imageData, bool useColors,
					   u8 colorD021, u8 colorD022, u8 colorD023, u8 colorD800,
					   CDebugInterfaceC64 *debugInterface);

void CopySpriteFromAdapterToImage(CDataAdapter *dataAdapter, int spriteAddr,
					  CImageData *imageData, bool useColors,
					  u8 colorD021, u8 colorD025, u8 colorD026, u8 colorD027,
					  CDebugInterfaceC64 *debugInterface, int gap, u8 alpha);

void CopyBitmapFromAdapterToImage(CDataAdapter *dataAdapter, int bitmapAddr, int screenAddr, int colorAddr,
					  CImageData *imageData, bool useColors,
					  u8 colorD021, CDebugInterfaceC64 *debugInterface);

void CopyScreenFromAdapterToImage(CDataAdapter *dataAdapter, int screenAddr, int charsetAddr, int colorAddr,
					  CImageData *imageData,
					  u8 colorD021, CDebugInterfaceC64 *debugInterface);

void CopyBitmapIndexFromAdapterToImage(CDataAdapter *dataAdapter, int bitmapIndex, int screenAddr, int colorAddr,
					   CImageData *imageData, bool useColors,
					   u8 colorD021, CDebugInterfaceC64 *debugInterface);

void CopyScreenIndexFromAdapterToImage(CDataAdapter *dataAdapter, int screenIndex, int charsetAddr, int colorAddr,
					   CImageData *imageData,
					   u8 colorD021, CDebugInterfaceC64 *debugInterface);

void GetC64UBitmapRenderParams(const C64ULogicalState &state, bool *useColors,
					   uint16_t *bitmapAddr, uint16_t *screenAddr,
					   uint16_t *colorAddr, uint8_t *colorD021);

void GetC64UScreenRenderParams(const C64ULogicalState &state,
					   uint16_t *screenAddr, uint16_t *charsetAddr,
					   uint16_t *colorAddr, uint8_t *colorD021);

void GetC64UCharsetRenderParams(const C64ULogicalState &state, bool *useColors,
					   uint16_t *charsetAddr,
					   uint8_t *colorD021, uint8_t *colorD022,
					   uint8_t *colorD023, uint8_t *colorD800);

void GetC64USpriteRenderParams(const C64ULogicalState &state, bool *useColors,
					  uint8_t *colorD021, uint8_t *colorD025,
					  uint8_t *colorD026, uint8_t *colorD027);

void CopyScreenTextToImage(u8 *screenData, u8 *charsetData, u8 *colorData,
					   CImageData *imageData,
					   u8 d021, CDebugInterfaceC64 *debugInterface);

class CTestC64GraphicsRendering : public CTest
{
public:
	class CTestMemoryAdapter : public CDataAdapter
	{
	public:
		CTestMemoryAdapter() : CDataAdapter("CTestMemoryAdapter")
		{
			memset(data, 0, sizeof(data));
		}

		virtual int AdapterGetDataLength() override
		{
			return sizeof(data);
		}

		virtual void AdapterReadByte(int pointer, uint8 *value) override
		{
			*value = data[pointer & 0xFFFF];
		}

		virtual void AdapterWriteByte(int pointer, uint8 value) override
		{
			data[pointer & 0xFFFF] = value;
		}

		u8 data[0x10000];
	};

	virtual const char *GetName() override { return "C64GraphicsRendering"; }

	static bool PixelMatchesColor(CImageData *imageData, int x, int y,
					  u8 expectedColor, CDebugInterfaceC64 *debugInterface,
					  char *message, size_t messageSize)
	{
		u8 expectedR, expectedG, expectedB;
		debugInterface->GetCBMColor(expectedColor, &expectedR, &expectedG, &expectedB);

		u8 actualR, actualG, actualB, actualA;
		imageData->GetPixelResultRGBA(x, y, &actualR, &actualG, &actualB, &actualA);

		if (actualR == expectedR && actualG == expectedG && actualB == expectedB && actualA == 255)
			return true;

		snprintf(message, messageSize,
				 "Pixel (%d,%d) expected color %u -> RGB(%u,%u,%u), got RGBA(%u,%u,%u,%u)",
				 x, y, expectedColor,
				 expectedR, expectedG, expectedB,
				 actualR, actualG, actualB, actualA);
		return false;
	}

	virtual void Run(ITestCallback *callback) override
	{
		this->callback = callback;
		this->isRunning = true;
		this->currentStep = 0;

		CDebugInterfaceC64 *debugInterface = (CDebugInterfaceC64 *)viewC64->debugInterfaceC64;
		if (debugInterface == NULL)
		{
			TestCompleted(false, "C64 debug interface is NULL");
			return;
		}

		u8 screenData[1000];
		u8 charsetData[2048];
		u8 colorData[1000];
		memset(screenData, 0, sizeof(screenData));
		memset(charsetData, 0, sizeof(charsetData));
		memset(colorData, 0, sizeof(colorData));

		// Character #1: only the left-most pixel on the first row is set.
		screenData[0] = 1;
		charsetData[8] = 0x80;
		colorData[0] = 0x06;

		CImageData imageData(320, 200, IMG_TYPE_RGBA);
		imageData.AllocImage(false, true);

		CopyScreenTextToImage(screenData, charsetData, colorData, &imageData, 0x0B, debugInterface);

		char failureMessage[256];
		if (!PixelMatchesColor(&imageData, 0, 0, 0x06, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		if (!PixelMatchesColor(&imageData, 1, 0, 0x0B, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		if (!PixelMatchesColor(&imageData, 8, 0, 0x0B, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		StepCompleted(1, true, "CopyScreenTextToImage renders charset bits and colors");

		u8 hiresBitmapData[8000];
		u8 hiresScreenData[1000];
		memset(hiresBitmapData, 0, sizeof(hiresBitmapData));
		memset(hiresScreenData, 0, sizeof(hiresScreenData));

		hiresBitmapData[0] = 0x80;
		hiresScreenData[0] = 0x63;

		CImageData hiresImageData(320, 200, IMG_TYPE_RGBA);
		hiresImageData.AllocImage(false, true);

		CopyHiresBitmapToImage(hiresBitmapData, hiresScreenData, &hiresImageData, 0x0B, debugInterface);

		if (!PixelMatchesColor(&hiresImageData, 0, 0, 0x06, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		if (!PixelMatchesColor(&hiresImageData, 1, 0, 0x03, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		StepCompleted(2, true, "CopyHiresBitmapToImage maps screen nibbles to cell colors");

		u8 multiBitmapData[8000];
		u8 multiScreenData[1000];
		u8 multiColorData[1000];
		memset(multiBitmapData, 0, sizeof(multiBitmapData));
		memset(multiScreenData, 0, sizeof(multiScreenData));
		memset(multiColorData, 0, sizeof(multiColorData));

		multiBitmapData[0] = 0x1B;
		multiScreenData[0] = 0x65;
		multiColorData[0] = 0x02;

		CImageData multiImageData(320, 200, IMG_TYPE_RGBA);
		multiImageData.AllocImage(false, true);

		CopyMultiBitmapToImage(multiBitmapData, multiScreenData, multiColorData, &multiImageData, 0x0B, debugInterface);

		if (!PixelMatchesColor(&multiImageData, 0, 0, 0x0B, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		if (!PixelMatchesColor(&multiImageData, 2, 0, 0x06, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		if (!PixelMatchesColor(&multiImageData, 4, 0, 0x05, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		if (!PixelMatchesColor(&multiImageData, 6, 0, 0x02, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		StepCompleted(3, true, "CopyMultiBitmapToImage maps 2-bit pixels to the four bitmap colors");

		CTestMemoryAdapter adapter;
		adapter.data[0x2800] = 0x80;

		CImageData adapterCharsetImage(256, 64, IMG_TYPE_RGBA);
		adapterCharsetImage.AllocImage(false, true);

		CopyCharsetFromAdapterToImage(&adapter, 0x2800, &adapterCharsetImage, false,
						  0x00, 0x00, 0x00, 0x00, debugInterface);

		if (!PixelMatchesColor(&adapterCharsetImage, 0, 0, 0x01, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		StepCompleted(4, true, "CopyCharsetFromAdapterToImage reads charset bytes from injected adapter");

		adapter.data[0x3000] = 0x80;

		CImageData adapterSpriteImage(32, 32, IMG_TYPE_RGBA);
		adapterSpriteImage.AllocImage(false, true);

		CopySpriteFromAdapterToImage(&adapter, 0x3000, &adapterSpriteImage, false,
						  0x00, 0x00, 0x00, 0x00, debugInterface, 4, 255);

		if (!PixelMatchesColor(&adapterSpriteImage, 4, 4, 0x01, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		StepCompleted(5, true, "CopySpriteFromAdapterToImage reads sprite bytes from injected adapter");

		adapter.data[0x4000] = 0x80;
		adapter.data[0x4400] = 0x63;

		CImageData adapterBitmapImage(320, 200, IMG_TYPE_RGBA);
		adapterBitmapImage.AllocImage(false, true);

		CopyBitmapFromAdapterToImage(&adapter, 0x4000, 0x4400, 0xD800, &adapterBitmapImage, false, 0x0B, debugInterface);

		if (!PixelMatchesColor(&adapterBitmapImage, 0, 0, 0x06, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		if (!PixelMatchesColor(&adapterBitmapImage, 1, 0, 0x03, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		StepCompleted(6, true, "CopyBitmapFromAdapterToImage reads bitmap and screen bytes from injected adapter");

		adapter.data[0x4800] = 1;
		adapter.data[0x5008] = 0x80;
		adapter.data[0x4C00] = 0x06;

		CImageData adapterScreenImage(320, 200, IMG_TYPE_RGBA);
		adapterScreenImage.AllocImage(false, true);

		CopyScreenFromAdapterToImage(&adapter, 0x4800, 0x5000, 0x4C00, &adapterScreenImage, 0x0B, debugInterface);

		if (!PixelMatchesColor(&adapterScreenImage, 0, 0, 0x06, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		if (!PixelMatchesColor(&adapterScreenImage, 1, 0, 0x0B, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}

		StepCompleted(7, true, "CopyScreenFromAdapterToImage reads screen, charset, and color bytes from injected adapter");

		C64ULogicalState c64uBitmapState = {};
		c64uBitmapState.bank.bitmapAddress = 0x2000;
		c64uBitmapState.bank.screenAddress = 0x0400;
		c64uBitmapState.vic.registers[0x11] = 0x20;
		c64uBitmapState.vic.registers[0x16] = 0x00;
		c64uBitmapState.vic.registers[0x21] = 0x0B;

		bool c64uBitmapUseColors = true;
		uint16_t c64uBitmapAddr = 0;
		uint16_t c64uScreenAddr = 0;
		uint16_t c64uColorAddr = 0;
		uint8_t c64uColorD021 = 0;
		GetC64UBitmapRenderParams(c64uBitmapState, &c64uBitmapUseColors, &c64uBitmapAddr, &c64uScreenAddr, &c64uColorAddr, &c64uColorD021);

		if (c64uBitmapUseColors != false || c64uBitmapAddr != 0x2000 || c64uScreenAddr != 0x0400 || c64uColorAddr != 0xD800 || c64uColorD021 != 0x0B)
		{
			TestCompleted(false, "GetC64UBitmapRenderParams did not derive hires bitmap parameters correctly");
			return;
		}

		c64uBitmapState.vic.registers[0x16] = 0x10;
		GetC64UBitmapRenderParams(c64uBitmapState, &c64uBitmapUseColors, &c64uBitmapAddr, &c64uScreenAddr, &c64uColorAddr, &c64uColorD021);
		if (c64uBitmapUseColors != true)
		{
			TestCompleted(false, "GetC64UBitmapRenderParams did not detect multicolor bitmap mode");
			return;
		}

		StepCompleted(8, true, "GetC64UBitmapRenderParams derives C64U bitmap addresses and mode");

		C64ULogicalState c64uScreenState = {};
		c64uScreenState.bank.screenAddress = 0x0800;
		c64uScreenState.bank.charsetAddress = 0x1800;
		c64uScreenState.vic.registers[0x21] = 0x06;

		uint16_t c64uTextScreenAddr = 0;
		uint16_t c64uCharsetAddr = 0;
		uint16_t c64uTextColorAddr = 0;
		uint8_t c64uTextColorD021 = 0;
		GetC64UScreenRenderParams(c64uScreenState, &c64uTextScreenAddr, &c64uCharsetAddr, &c64uTextColorAddr, &c64uTextColorD021);
		if (c64uTextScreenAddr != 0x0800 || c64uCharsetAddr != 0x1800 || c64uTextColorAddr != 0xD800 || c64uTextColorD021 != 0x06)
		{
			TestCompleted(false, "GetC64UScreenRenderParams did not derive C64U text screen parameters correctly");
			return;
		}

		StepCompleted(9, true, "GetC64UScreenRenderParams derives C64U screen addresses");

		C64ULogicalState c64uCharsetState = {};
		c64uCharsetState.bank.charsetAddress = 0x1000;
		c64uCharsetState.vic.registers[0x11] = 0x00;
		c64uCharsetState.vic.registers[0x16] = 0x10;
		c64uCharsetState.vic.registers[0x21] = 0x06;
		c64uCharsetState.vic.registers[0x22] = 0x07;
		c64uCharsetState.vic.registers[0x23] = 0x08;

		bool c64uCharsetUseColors = false;
		uint16_t c64uCharsetAddr2 = 0;
		uint8_t c64uCharsetD021 = 0;
		uint8_t c64uCharsetD022 = 0;
		uint8_t c64uCharsetD023 = 0;
		uint8_t c64uCharsetD800 = 0;
		GetC64UCharsetRenderParams(c64uCharsetState, &c64uCharsetUseColors, &c64uCharsetAddr2,
						  &c64uCharsetD021, &c64uCharsetD022, &c64uCharsetD023, &c64uCharsetD800);
		if (c64uCharsetUseColors != true || c64uCharsetAddr2 != 0x1000 || c64uCharsetD021 != 0x06 || c64uCharsetD022 != 0x07 || c64uCharsetD023 != 0x08 || c64uCharsetD800 != 0x01)
		{
			TestCompleted(false, "GetC64UCharsetRenderParams did not derive C64U charset render parameters correctly");
			return;
		}

		StepCompleted(10, true, "GetC64UCharsetRenderParams derives C64U charset render parameters");

		C64ULogicalState c64uSpriteState = {};
		c64uSpriteState.vic.registers[0x1C] = 0x01;
		c64uSpriteState.vic.registers[0x21] = 0x02;
		c64uSpriteState.vic.registers[0x25] = 0x03;
		c64uSpriteState.vic.registers[0x26] = 0x04;
		c64uSpriteState.vic.registers[0x27] = 0x05;

		bool c64uSpriteUseColors = false;
		uint8_t c64uSpriteD021 = 0;
		uint8_t c64uSpriteD025 = 0;
		uint8_t c64uSpriteD026 = 0;
		uint8_t c64uSpriteD027 = 0;
		GetC64USpriteRenderParams(c64uSpriteState, &c64uSpriteUseColors, &c64uSpriteD021, &c64uSpriteD025, &c64uSpriteD026, &c64uSpriteD027);
		if (c64uSpriteUseColors != true || c64uSpriteD021 != 0x02 || c64uSpriteD025 != 0x03 || c64uSpriteD026 != 0x04 || c64uSpriteD027 != 0x05)
		{
			TestCompleted(false, "GetC64USpriteRenderParams did not derive C64U sprite render parameters correctly");
			return;
		}

		StepCompleted(11, true, "GetC64USpriteRenderParams derives C64U sprite render parameters");

		memset(adapter.data, 0, sizeof(adapter.data));
		adapter.data[0x2000] = 0x80;
		adapter.data[0x4400] = 0x63;

		CImageData indexedBitmapImage(320, 200, IMG_TYPE_RGBA);
		indexedBitmapImage.AllocImage(false, true);
		CopyBitmapIndexFromAdapterToImage(&adapter, 1, 0x4400, 0xD800, &indexedBitmapImage, false, 0x0B, debugInterface);
		if (!PixelMatchesColor(&indexedBitmapImage, 0, 0, 0x06, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}
		StepCompleted(12, true, "CopyBitmapIndexFromAdapterToImage reads bitmap banks by index");

		memset(adapter.data, 0, sizeof(adapter.data));
		adapter.data[0x0800] = 1;
		adapter.data[0x1000 + 8] = 0x80;
		adapter.data[0xD800] = 0x06;

		CImageData indexedScreenImage(320, 200, IMG_TYPE_RGBA);
		indexedScreenImage.AllocImage(false, true);
		CopyScreenIndexFromAdapterToImage(&adapter, 2, 0x1000, 0xD800, &indexedScreenImage, 0x0B, debugInterface);
		if (!PixelMatchesColor(&indexedScreenImage, 0, 0, 0x06, debugInterface, failureMessage, sizeof(failureMessage)))
		{
			TestCompleted(false, failureMessage);
			return;
		}
		StepCompleted(13, true, "CopyScreenIndexFromAdapterToImage reads screens by index");

		CDebugInterfaceC64U *debugInterfaceC64U = (CDebugInterfaceC64U *)viewC64->debugInterfaceC64U;
		if (debugInterfaceC64U == NULL)
		{
			TestCompleted(false, "C64U debug interface is NULL");
			return;
		}

		uint8 expectedPalette[16][3];
		if (!C64GetPaletteRGB(c64SettingsVicPalette, expectedPalette))
		{
			TestCompleted(false, "Could not read current C64 palette");
			return;
		}

		uint8 actualR, actualG, actualB;
		debugInterfaceC64U->GetCBMColor(0x06, &actualR, &actualG, &actualB);
		if (actualR != expectedPalette[0x06][0] || actualG != expectedPalette[0x06][1] || actualB != expectedPalette[0x06][2])
		{
			TestCompleted(false, "CDebugInterfaceC64U::GetCBMColor did not return the active palette color");
			return;
		}

		StepCompleted(14, true, "CDebugInterfaceC64U::GetCBMColor returns the active C64 palette color");
		TestCompleted(true, "C64 graphics rendering helpers preserve text and bitmap colors");
	}

	virtual void Cancel() override
	{
		isRunning = false;
	}
};
