#include "CViewC64UStateCPU.h"
#include "SYS_Main.h"
#include "CSlrFont.h"
#include "CViewC64.h"
#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/Trace/C64U6502Decoder.h"

#define NUM_C64U_REGS 6

static register_def c64u_cpu_regs[NUM_C64U_REGS] = {
	{	STATE_CPU_REGISTER_PC,		0.0,  4 },
	{	STATE_CPU_REGISTER_A,		5.0,  2 },
	{	STATE_CPU_REGISTER_X,		8.0,  2 },
	{	STATE_CPU_REGISTER_Y,		11.0, 2 },
	{	STATE_CPU_REGISTER_SP,		14.0, 2 },
	{	STATE_CPU_REGISTER_FLAGS,	17.0, 8 }
};

CViewC64UStateCPU::CViewC64UStateCPU(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterfaceC64U)
: CViewBaseStateCPU(name, posX, posY, posZ, sizeX, sizeY, debugInterfaceC64U)
{
	this->debugInterfaceC64U = debugInterfaceC64U;

	imGuiNoWindowPadding = true;
	imGuiNoScrollbar = true;

	this->numRegisters = NUM_C64U_REGS;
	regs = (register_def *)c64u_cpu_regs;
	numCharacterColumns = 26.0f;

	this->font = viewC64->fontDisassembly;
}

void CViewC64UStateCPU::RenderRegisters()
{
	float px = this->posX;
	float py = this->posY;

	C64U6502Decoder *decoder = debugInterfaceC64U->GetDecoder6510();

	char buf[64];

	if (decoder == NULL || !decoder->IsSynced() || !decoder->AreRegsValid())
	{
		strcpy(buf, "PC   AR XR YR SP NV-BDIZC");
		font->BlitText(buf, px, py, -1, fontSize);
		py += fontSize;
		strcpy(buf, "---- -- -- -- -- --------");
		font->BlitText(buf, px, py, -1, fontSize);
		return;
	}

	strcpy(buf, "PC   AR XR YR SP NV-BDIZC");
	font->BlitText(buf, px, py, -1, fontSize);
	py += fontSize;

	char *bufPtr = buf;
	sprintfHexCode16WithoutZeroEnding(bufPtr, decoder->GetCurrentPC()); bufPtr += 5;
	sprintfHexCode8WithoutZeroEnding(bufPtr, decoder->GetRegA()); bufPtr += 3;
	sprintfHexCode8WithoutZeroEnding(bufPtr, decoder->GetRegX()); bufPtr += 3;
	sprintfHexCode8WithoutZeroEnding(bufPtr, decoder->GetRegY()); bufPtr += 3;
	sprintfHexCode8WithoutZeroEnding(bufPtr, decoder->GetRegSP()); bufPtr += 3;
	Byte2BitsWithoutEndingZero(decoder->GetRegP(), bufPtr);

	font->BlitText(buf, px, py, -1, fontSize);
}

int CViewC64UStateCPU::GetRegisterValue(StateCPURegister reg)
{
	C64U6502Decoder *decoder = debugInterfaceC64U->GetDecoder6510();
	if (decoder == NULL || !decoder->AreRegsValid())
		return 0;

	switch (reg)
	{
		case STATE_CPU_REGISTER_PC:    return decoder->GetCurrentPC();
		case STATE_CPU_REGISTER_A:     return decoder->GetRegA();
		case STATE_CPU_REGISTER_X:     return decoder->GetRegX();
		case STATE_CPU_REGISTER_Y:     return decoder->GetRegY();
		case STATE_CPU_REGISTER_SP:    return decoder->GetRegSP();
		case STATE_CPU_REGISTER_FLAGS: return decoder->GetRegP();
		default: return 0;
	}
}

void CViewC64UStateCPU::SetRegisterValue(StateCPURegister reg, int value)
{
	// Cannot set registers on real hardware via trace bus
}
