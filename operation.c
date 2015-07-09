#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "operation.h"
#include "error_codes.h"

uint32_t operation_to_int(Operation* operation)
{
	if (!operation)
		return 0;

	if (operation->standard.number < 13)
	{
		return operation->standard.number | (operation->standard.a << 23) | (operation->standard.b << 26) | (operation->standard.c << 29);
	}
	else if (operation->put.number == 13)
	{
		return operation->put.number | (operation->put.a << 4) | (operation->put.value << 7);
	}
	else
	{
		fprintf(stderr, "FATAL: Invalid opcode passed to operation_to_int: %d\n", operation->standard.number);
		exit(ERR_INVALID_OPCODE);
	}

	return 0;
}

void int_to_operation(uint32_t value, Operation* operation)
{
	operation->standard.number = (value >> 28) & 0xF;

	if (operation->standard.number < 13)
	{
		operation->standard.a = (value >> 6) & 7;
		operation->standard.b = (value >> 3) & 7;
		operation->standard.c = (value) & 7;
	}
	else
	{
		operation->put.a = (value >> 25) & 7;
		operation->put.value = value & 0x1FFFFFF;
	}
}
