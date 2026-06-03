#include "CViewC64UAllGraphicsScreens.h"

#include "VID_Main.h"
#include "VID_ImageBinding.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/State/C64ULogicalStateCache.h"
#include "../../Tools/C64Tools.h"

CViewC64UAllGraphicsScreens::CViewC64UAllGraphicsScreens(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
: CGuiViewMovingPane(name, posX, posY, posZ, sizeX, sizeY, 8 * 320, 8 * 200)
{
	this->debugInterface = debugInterface;
	this->selectedScreenId = 0;
	this->consumeTapBackground = false;
	this->allowFocus = false;
	this->imGuiNoWindowPadding = true;
	this->imGuiNoScrollbar = true;
	this->minZoom = 0.125f;
	this->maxZoom = 60.0f;
	this->imagesAllocated = false;
}

void CViewC64UAllGraphicsScreens::EnsureImagesAllocated()
{
	if (imagesAllocated)
		return;
	imagesAllocated = true;

	for (int i = 0; i < 64; i++)
	{
		CImageData *imageData = new CImageData(320, 200, IMG_TYPE_RGBA);
		imageData->AllocImage(false, true);
		screenImageData.push_back(imageData);

		CSlrImage *image = new CSlrImage(true, false);
		image->LoadImageForRebinding(imageData, RESOURCE_PRIORITY_STATIC);
		image->resourceType = RESOURCE_TYPE_IMAGE_DYNAMIC;
		image->resourcePriority = RESOURCE_PRIORITY_STATIC;
		VID_PostImageBinding(image, NULL, BINDING_MODE_DONT_FREE_IMAGEDATA);
		screenImages.push_back(image);
	}
}

CViewC64UAllGraphicsScreens::~CViewC64UAllGraphicsScreens()
{
}

void CViewC64UAllGraphicsScreens::RenderImGui()
{
	PreRenderImGui();
	VID_SetClipping(posX, posY, sizeX, sizeY);
	Render();
	VID_ResetClipping();
	PostRenderImGui();
}

void CViewC64UAllGraphicsScreens::Render()
{
	EnsureImagesAllocated();

	C64ULogicalStateCache *stateCache = debugInterface->GetLogicalStateCache();
	if (stateCache == NULL)
	{
		CGuiView::Render();
		return;
	}

	C64ULogicalState state = stateCache->GetState();
	uint16_t screenAddr;
	uint16_t charsetAddr;
	uint16_t colorAddr;
	uint8_t colorD021;
	GetC64UScreenRenderParams(state, &screenAddr, &charsetAddr, &colorAddr, &colorD021);

	float screenSizeX = 320.0f * currentZoom;
	float screenSizeY = 200.0f * currentZoom;
	float startX = renderMapPosX;
	float startY = renderMapPosY;
	float px = startX;
	float py = startY;

	for (int i = 0; i < 64; i++)
	{
		if (!(py + screenSizeY < posY || py > posY + sizeY || px + screenSizeX < posX || px > posX + sizeX))
		{
			CopyScreenIndexFromAdapterToImage(((CDebugInterfaceC64U *)debugInterface)->dataAdapterC64UVicRam, i, charsetAddr, colorAddr,
						  screenImageData[i], colorD021, debugInterface);
			screenImages[i]->ReBindImageData(screenImageData[i]);
		}

		Blit(screenImages[i], px, py, posZ, screenSizeX, screenSizeY);
		if (i == selectedScreenId)
		{
			BlitRectangle(px, py, posZ, screenSizeX, screenSizeY, 1.0f, 0.0f, 0.0f, 0.7f);
		}

		py += screenSizeY;
		if ((i & 7) == 7)
		{
			py = startY;
			px += screenSizeX;
		}
	}

	CGuiView::Render();
}

void CViewC64UAllGraphicsScreens::Render(float posX, float posY)
{
	CGuiView::Render(posX, posY);
}

void CViewC64UAllGraphicsScreens::DoLogic()
{
	CGuiView::DoLogic();
}

bool CViewC64UAllGraphicsScreens::DoTap(float x, float y)
{
	int itemId = GetItemIdAt(x, y);
	if (itemId != -1)
	{
		selectedScreenId = itemId;
		return true;
	}
	return CGuiViewMovingPane::DoTap(x, y);
}

bool CViewC64UAllGraphicsScreens::DoNotTouchedMove(float x, float y)
{
	if (IsInsideView(x, y))
	{
		int itemId = GetItemIdAt(x, y);
		if (itemId != -1)
			selectedScreenId = itemId;
	}
	return CGuiViewMovingPane::DoNotTouchedMove(x, y);
}

void CViewC64UAllGraphicsScreens::ZoomToFit()
{
	float zoomX = sizeX / (8.0f * 320.0f);
	float zoomY = sizeY / (8.0f * 200.0f);
	currentZoom = (zoomX < zoomY) ? zoomX : zoomY;
}

int CViewC64UAllGraphicsScreens::GetItemIdAt(float x, float y)
{
	float screenSizeX = 320.0f * currentZoom;
	float screenSizeY = 200.0f * currentZoom;
	int itemId = 0;
	float px = renderMapPosX;
	for (int col = 0; col < 8; col++)
	{
		float py = renderMapPosY;
		for (int row = 0; row < 8; row++)
		{
			if (x >= px && x <= px + screenSizeX && y >= py && y <= py + screenSizeY)
				return itemId;
			itemId++;
			py += screenSizeY;
		}
		px += screenSizeX;
	}
	return -1;
}
