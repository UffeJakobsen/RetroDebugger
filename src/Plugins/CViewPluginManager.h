#ifndef _CViewPluginManager_h_
#define _CViewPluginManager_h_

#include "CGuiView.h"

class CViewPluginManager : public CGuiView
{
public:
	CViewPluginManager(float posX, float posY, float posZ, float sizeX, float sizeY);
	virtual ~CViewPluginManager();

	virtual void Render() {}
	virtual void Render(float posX, float posY) {}
	virtual void RenderImGui();
	virtual void DoLogic() {}
};

#endif //_CViewPluginManager_h_
