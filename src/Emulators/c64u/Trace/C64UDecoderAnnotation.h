#ifndef _C64UDECODERANNOTATION_H_
#define _C64UDECODERANNOTATION_H_

#include <cstdint>

struct C64UDecoderAnnotation
{
	enum Type
	{
		UNKNOWN = 0,
		OPCODE_FETCH,
		OPERAND,
		ADDRESS_CALC,
		DATA_READ,
		DATA_WRITE,
		INTERRUPT
	};

	Type type;
	uint16_t instructionPC;  // PC of the instruction this cycle belongs to
};

#endif
