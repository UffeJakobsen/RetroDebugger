#include "CViewC64UAllGraphicsBitmaps.h"

#include "VID_Main.h"
#include "VID_ImageBinding.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/State/C64ULogicalStateCache.h"
#include "../../Tools/C64Tools.h"

CViewC64UAllGraphicsBitmaps::CViewC64UAllGraphicsBitmaps(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
: CGuiViewMovingPane(name, posX, posY, posZ, sizeX, sizeY, 2 * 320, 4 * 200)
{
	this->debugInterface = debugInterface;
	this->selectedBitmapId = 0;
	this->consumeTapBackground = false;
	this->allowFocus = false;
	this->imGuiNoWindowPadding = true;
	this->imGuiNoScrollbar = true;
	this->minZoom = 0.25f;
	this->maxZoom = 60.0f;
	this->imagesAllocated = false;
}

void CViewC64UAllGraphicsBitmaps::EnsureImagesAllocated()
{
	if (imagesAllocated)
		return;
	imagesAllocated = true;

	for (int i = 0; i < 8; i++)
	{
		CImageData *imageData = new CImageData(320, 200, IMG_TYPE_RGBA);
		imageData->AllocImage(false, true);
		bitmapImageData.push_back(imageData);

		CSlrImage *image = new CSlrImage(true, false);
		image->LoadImageForRebinding(imageData, RESOURCE_PRIORITY_STATIC);
		image->resourceType = RESOURCE_TYPE_IMAGE_DYNAMIC;
		image->resourcePriority = RESOURCE_PRIORITY_STATIC;
		VID_PostImageBinding(image, NULL, BINDING_MODE_DONT_FREE_IMAGEDATA);
		bitmapImages.push_back(image);
	}
}

CViewC64UAllGraphicsBitmaps::~CViewC64UAllGraphicsBitmaps()
{
}

void CViewC64UAllGraphicsBitmaps::RenderImGui()
{
	PreRenderImGui();
	VID_SetClipping(posX, posY, sizeX, sizeY);
	Render();
	VID_ResetClipping();
	PostRenderImGui();
}

void CViewC64UAllGraphicsBitmaps::Render()
{
	EnsureImagesAllocated();

	C64ULogicalStateCache *stateCache = debugInterface->GetLogicalStateCache();
	if (stateCache == NULL)
	{
		CGuiView::Render();
		return;
	}

	C64ULogicalState state = stateCache->GetState();
	bool useColors;
	uint16_t bitmapAddr;
	uint16_t screenAddr;
	uint16_t colorAddr;
	uint8_t colorD021;
	GetC64UBitmapRenderParams(state, &useColors, &bitmapAddr, &screenAddr, &colorAddr, &colorD021);
	UpdateBitmaps(useColors, screenAddr, colorAddr, colorD021);

	float bitmapSizeX = 320.0f * currentZoom;
	float bitmapSizeY = 200.0f * currentZoom;
	float startX = renderMapPosX;
	float startY = renderMapPosY;
	float px = startX;
	float py = startY;

	for (int i = 0; i < 8; i++)
	{
		Blit(bitmapImages[i], px, py, posZ, bitmapSizeX, bitmapSizeY);
		if (i == selectedBitmapId)
		{
			BlitRectangle(px, py, posZ, bitmapSizeX, bitmapSizeY, 1.0f, 0.0f, 0.0f, 0.7f);
		}

		py += bitmapSizeY;
		if ((i & 3) == 3)
		{
			py = startY;
			px += bitmapSizeX;
		}
	}

	CGuiView::Render();
}

void CViewC64UAllGraphicsBitmaps::Render(float posX, float posY)
{
	CGuiView::Render(posX, posY);
}

void CViewC64UAllGraphicsBitmaps::DoLogic()
{
	CGuiView::DoLogic();
}

bool CViewC64UAllGraphicsBitmaps::DoTap(float x, float y)
{
	int itemId = GetItemIdAt(x, y);
	if (itemId != -1)
	{
		selectedBitmapId = itemId;
		return true;
	}
	return CGuiViewMovingPane::DoTap(x, y);
}

bool CViewC64UAllGraphicsBitmaps::DoNotTouchedMove(float x, float y)
{
	if (IsInsideView(x, y))
	{
		int itemId = GetItemIdAt(x, y);
		if (itemId != -1)
			selectedBitmapId = itemId;
	}
	return CGuiViewMovingPane::DoNotTouchedMove(x, y);
}

void CViewC64UAllGraphicsBitmaps::ZoomToFit()
{
	float zoomX = sizeX / (2.0f * 320.0f);
	float zoomY = sizeY / (4.0f * 200.0f);
	currentZoom = (zoomX < zoomY) ? zoomX : zoomY;
}

void CViewC64UAllGraphicsBitmaps::UpdateBitmaps(bool useColors, uint16_t screenAddr, uint16_t colorAddr, uint8_t colorD021)
{
	for (int i = 0; i < 8; i++)
	{
		CopyBitmapIndexFromAdapterToImage(debugInterface->dataAdapterC64DirectRam, i, screenAddr, colorAddr,
						  bitmapImageData[i], useColors, colorD021, debugInterface);
		bitmapImages[i]->ReBindImageData(bitmapImageData[i]);
	}
}

int CViewC64UAllGraphicsBitmaps::GetItemIdAt(float x, float y)
{
	float bitmapSizeX = 320.0f * currentZoom;
	float bitmapSizeY = 200.0f * currentZoom;
	int itemId = 0;
	float px = renderMapPosX;
	for (int col = 0; col < 2; col++)
	{
		float py = renderMapPosY;
		for (int row = 0; row < 4; row++)
		{
			if (x >= px && x <= px + bitmapSizeX && y >= py && y <= py + bitmapSizeY)
				return itemId;
			itemId++;
			py += bitmapSizeY;
		}
		px += bitmapSizeX;
	}
	return -1;
}
