#ifndef _CVIEWC64UALLGRAPHICSSCREENS_H_
#define _CVIEWC64UALLGRAPHICSSCREENS_H_

#include "CGuiViewMovingPane.h"

#include <vector>

class CDebugInterfaceC64U;
class CImageData;
class CSlrImage;

class CViewC64UAllGraphicsScreens : public CGuiViewMovingPane
{
public:
	CViewC64UAllGraphicsScreens(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface);
	virtual ~CViewC64UAllGraphicsScreens();

	virtual void Render() override;
	virtual void Render(float posX, float posY) override;
	virtual void RenderImGui() override;
	virtual void DoLogic() override;
	virtual bool DoTap(float x, float y) override;
	virtual bool DoNotTouchedMove(float x, float y) override;

	void ZoomToFit();

protected:
	void EnsureImagesAllocated();
	int GetItemIdAt(float x, float y);

	CDebugInterfaceC64U *debugInterface;
	std::vector<CImageData *> screenImageData;
	std::vector<CSlrImage *> screenImages;
	int selectedScreenId;
	bool imagesAllocated;
};

#endif
