#ifndef _CVIEWC64UALLGRAPHICSBITMAPS_H_
#define _CVIEWC64UALLGRAPHICSBITMAPS_H_

#include "CGuiViewMovingPane.h"

#include <vector>

class CDebugInterfaceC64U;
class CImageData;
class CSlrImage;

class CViewC64UAllGraphicsBitmaps : public CGuiViewMovingPane
{
public:
	CViewC64UAllGraphicsBitmaps(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface);
	virtual ~CViewC64UAllGraphicsBitmaps();

	virtual void Render() override;
	virtual void Render(float posX, float posY) override;
	virtual void RenderImGui() override;
	virtual void DoLogic() override;
	virtual bool DoTap(float x, float y) override;
	virtual bool DoNotTouchedMove(float x, float y) override;

	void ZoomToFit();

protected:
	void EnsureImagesAllocated();
	void UpdateBitmaps(bool useColors, uint16_t screenAddr, uint16_t colorAddr, uint8_t colorD021);
	int GetItemIdAt(float x, float y);

	CDebugInterfaceC64U *debugInterface;
	std::vector<CImageData *> bitmapImageData;
	std::vector<CSlrImage *> bitmapImages;
	int selectedBitmapId;
	bool imagesAllocated;
};

#endif
