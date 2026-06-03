#ifndef _CVIEWC64USTATECPU_H_
#define _CVIEWC64USTATECPU_H_

#include "CViewBaseStateCPU.h"

class CDebugInterfaceC64U;

class CViewC64UStateCPU : public CViewBaseStateCPU
{
public:
	CViewC64UStateCPU(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterfaceC64U);

	virtual void RenderRegisters();
	virtual void SetRegisterValue(StateCPURegister reg, int value);
	virtual int GetRegisterValue(StateCPURegister reg);

	CDebugInterfaceC64U *debugInterfaceC64U;
};

#endif
