#ifndef _IGT2AudioEffect_H_
#define _IGT2AudioEffect_H_

class IGT2AudioEffect
{
public:
	virtual ~IGT2AudioEffect() {}
	virtual const char* GetName() = 0;
	virtual void Process(float *bufferL, float *bufferR, int numSamples) = 0;
	virtual void RenderParamsImGui() = 0;
	virtual void Reset() = 0;
};

#endif
