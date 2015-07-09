#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

#include "error_codes.h"

#define REGISTERS_COUNT 8
#define EXTRA_REGISTERS 1
#define PC_REGISTER REGISTERS_COUNT
#define PROGRAM_ARRAY 0
#define OPCODES_COUNT 14

#define GET_REGISTERS_COUNT(var, extra) \
	uint8_t var;\
	\
	if (!extra)\
		var = REGISTERS_COUNT;\
	else\
		var = REGISTERS_COUNT + EXTRA_REGISTERS\

#ifdef DEBUG
#define TRACE(...) printf( __VA_ARGS__)
#else
#define TRACE(...) 0
#endif

#include "operation.h"

typedef struct Array {
	uint32_t 	size;
	int32_t*	content;
} Array;

typedef struct Memory {
	uint32_t 	size;
	Array*		arrays;
	uint32_t*	pool;
	uint32_t	pool_pointer;
} Memory;

typedef struct Machine {
	Memory 		memory;
	uint32_t	registers[REGISTERS_COUNT + EXTRA_REGISTERS];
} Machine;

void fatal(int code, Machine* machine);
void initialize_memory(Machine* machine);
void allocate_memory(uint32_t index, uint32_t size, Machine* machine);
void dump_memory(Machine* machine);
void dump_state(Machine* machine, Operation* operation, uint32_t inst);

uint32_t allocate_array(uint32_t size, Machine* machine);
Array* get_array(uint32_t index, Machine* machine);
uint32_t read_array(uint32_t index, uint32_t location, Machine* machine);

uint32_t get_register(uint8_t index, Machine* machine, int allow_extra);
uint32_t set_register(uint8_t index, uint32_t value, Machine* machine, int allow_extra);
int peek(Operation* operation, Machine* machine);

void conditional_move(Operation* op, Machine* machine);
void array_index(Operation* op, Machine* machine);
void array_amendment(Operation* op, Machine* machine);
void addition(Operation* op, Machine* machine);
void multiplication(Operation* op, Machine* machine);
void division(Operation* op, Machine* machine);
void not_and(Operation* op, Machine* machine);
void halt(Operation* op, Machine* machine);
void allocation(Operation* op, Machine* machine);
void abandoment(Operation* op, Machine* machine);
void output(Operation* op, Machine* machine);
void input(Operation* op, Machine* machine);
void load_program(Operation* op, Machine* machine);
void ortography(Operation* op, Machine* machine);

void (*opcodes_table[OPCODES_COUNT]) (Operation*, Machine*) = {
	conditional_move,
	array_index,
	array_amendment,
	addition,
	multiplication,
	division,
	not_and,
	halt,
	allocation,
	abandoment,
	output,
	input,
	load_program,
	ortography
};

uint32_t cycle = 0;

void sig_term_handler(int sig) {
	printf("SIGTERM/ABRT/INT received, halting Universal Machine!\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	signal(SIGTERM, &sig_term_handler);
	signal(SIGABRT, &sig_term_handler);
	signal(SIGINT, &sig_term_handler);

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s program_file\n", argv[0]);
		exit(ERR_MISSING_ARGUMENTS);
	}

	FILE* program_file = fopen(argv[1], "r");

	if (program_file == NULL)
	{
		fprintf(stderr, "FATAL: Can't open program file: %s\n", argv[0]);
		exit(ERR_INVALID_PROGRAM_FILE);
	}

	Machine machine;
	memset((void*)&machine, 0, sizeof(Machine));

	initialize_memory(&machine);

	fseek(program_file, 0, SEEK_END);
	uint32_t fsize = ftell(program_file);
	fseek(program_file, 0, SEEK_SET);

	allocate_memory(PROGRAM_ARRAY, fsize / sizeof(Operation), &machine);
	Array* program_array = get_array(PROGRAM_ARRAY, &machine);

	//fread(program_array->content, program_array->size * sizeof(Operation), 1, program_file);
	uint32_t i = 0;

	for (i = 0; i < (fsize / 4); i++)
	{
		uint8_t c1, c2, c3, c4;
		c1 = fgetc(program_file);
		c2 = fgetc(program_file);
		c3 = fgetc(program_file);
		c4 = fgetc(program_file);
		uint32_t instr = ((unsigned int)c1 << 24) + ((unsigned int)c2 << 16) + ((unsigned int)c3 << 8) + (unsigned int)c4;
		program_array->content[i] = instr;
	}

	fclose(program_file);

	Operation op;
	for(;;)
	{
		peek(&op, &machine);

		if (op.standard.number < 13)
			TRACE("Opcode: %d - A: %d, B: %d, C: %d (value: %x)\n", op.standard.number, op.standard.a, op.standard.b, op.standard.c, operation_to_int(&op));
		else
			TRACE("Opcode: %d - A: %d, Value: %d (value: %x)\n", op.put.number, op.put.a, op.put.value, operation_to_int(&op));

		if (op.standard.number >= OPCODES_COUNT)
		{
			uint32_t pc = (uint32_t)get_register(PC_REGISTER, &machine, 1);
			fprintf(stderr, "ERROR: Invalid opcode: %d (pc = 0x%x, offset = %zu)\n", op.standard.number, pc, pc * sizeof(uint32_t));
		}

		opcodes_table[op.standard.number](&op, &machine);
		cycle++;
	}

	#if defined(DEBUG)
		TRACE("Execution ended\n");
		dump_memory(&machine);
	#endif

	return 0;
}

void fatal(int code, Machine* machine)
{
	#ifndef DISABLE_MEMORY_DUMP
		dump_memory(machine);
	#endif

	exit(code);
}

void initialize_memory(Machine* machine)
{
	machine->memory.arrays = (Array*)malloc(sizeof(Array));

	#ifndef UNSAFE
	if (machine->memory.arrays == NULL)
	{
		fprintf(stderr, "FATAL: Error allocating memory pointers\n");
		fatal(ERR_OUT_OF_MEMORY, machine);
	}
	#endif

	machine->memory.pool = (uint32_t*)malloc(sizeof(uint32_t));

	#ifndef UNSAFE
	if (machine->memory.pool == NULL)
	{
		fprintf(stderr, "FATAL: Error allocating memory pool\n");
		fatal(ERR_OUT_OF_MEMORY, machine);
	}
	#endif

	memset(machine->memory.pool, 0, sizeof(uint32_t));
	memset(machine->memory.arrays, 0, sizeof(Array));
	machine->memory.pool_pointer = 0;
}

void allocate_memory(uint32_t index, uint32_t size, Machine* machine)
{
	if (machine->memory.size < (index + 1))
	{
		TRACE("memory needs to be resized from %u to %u\n", machine->memory.size, (index + 1));
		machine->memory.arrays = (Array*)realloc(machine->memory.arrays, (index + 1) * sizeof(Array));
		machine->memory.pool = (Array*)realloc(machine->memory.pool, (index + 1) * sizeof(uint32_t));

		#ifndef UNSAFE
		if (machine->memory.arrays == NULL)
		{
			fprintf(stderr, "FATAL: Error resizing memory pointers while "
				"allocating array %d with size of %d bytes\n", index, size);

			fatal(ERR_OUT_OF_MEMORY, machine);
		}
		#endif

		uint32_t i;
		for(i = machine->memory.size; i < index + 1; i++)
		{
			TRACE("initializing unallocated array %u\n", i);
			machine->memory.arrays[i].content = NULL;
			machine->memory.arrays[i].size = 0;
		}

		machine->memory.size = index + 1;
	}

	if (machine->memory.arrays[index].content == NULL)
	{
		machine->memory.arrays[index].content = (uint32_t*)malloc(size * sizeof(uint32_t));

		#ifndef UNSAFE
		if (!machine->memory.arrays[index].content)
		{
			fprintf(stderr, "FATAL: Error allocating array %d with size of %d bytes\n", index, size);
			fatal(ERR_OUT_OF_MEMORY, machine);
		}
		#endif
	}
	else
	{
		machine->memory.arrays[index].content = (uint32_t*)realloc(machine->memory.arrays[index].content, size * sizeof(uint32_t));

		#ifndef UNSAFE
		if (!machine->memory.arrays[index].content)
		{
			fprintf(stderr, "FATAL: Error resizing array %d with size of %d bytes\n", index, size);
			fatal(ERR_OUT_OF_MEMORY, machine);
		}
		#endif
	}

	memset((void*)machine->memory.arrays[index].content, 0, (size_t)(size * sizeof(uint32_t)));
	machine->memory.arrays[index].size = size;

	TRACE("allocate_memory(index = %u, size = %u)\n", index, size);
}

Array* get_array(uint32_t index, Machine* machine)
{
	#ifndef UNSAFE
	if ((machine->memory.size - 1) < index)
	{
		fprintf(stderr, "FATAL: Error accessing unallocated array at index %d. Last index is %d.\n", index, machine->memory.size - 1);
		fatal(ERR_OUT_OF_MEMORY, machine);
	}
	#endif

	return &machine->memory.arrays[index];
}

uint32_t read_array(uint32_t index, uint32_t location, Machine* machine)
{
	Array* array = get_array(index, machine);

	#ifndef UNSAFE
	if (location >= array->size)
	{
		fprintf(stderr, "FATAL: reading array %u at %u, which is beyond its last index %u.\n", index, location, array->size - 1);
		fatal(ERR_MEMORY_ACCESS_INVALID, machine);
	}
	#endif

	return array->content[location];
}

uint32_t allocate_array(uint32_t size, Machine* machine)
{
	uint32_t index = 0;

	if (machine->memory.pool_pointer)
	{
		index = machine->memory.pool[machine->memory.pool_pointer - 1];
		TRACE("resurrecting array %u from the pool", index);
		machine->memory.pool_pointer--;
	}
	else
	{
		uint32_t i;
		for(i = 1; i < machine->memory.size; i++)
		{
			if (machine->memory.arrays[i].content == NULL)
			{
				index = i;
				break;
			}
		}
	}

	if (!index)
		index = machine->memory.size;

	TRACE("allocate_array(%u) = %u\n", size, index);

	allocate_memory(index, size, machine);

	return index;
}

uint32_t get_register(uint8_t index, Machine* machine, int allow_extra)
{
	GET_REGISTERS_COUNT(max_register, allow_extra);

	#ifndef UNSAFE
	if (index >= max_register)
	{
		fprintf(stderr, "FATAL: trying to get invalid register %d, (extra = %d, last = %d)\n", index, allow_extra, max_register);
		fatal(ERR_INVALID_REGISTER_ACCESS, machine);
	}
	#endif

	return machine->registers[index];
}

uint32_t set_register(uint8_t index, uint32_t value, Machine* machine, int allow_extra)
{
	GET_REGISTERS_COUNT(max_register, allow_extra);

	#ifndef UNSAFE
	if (index >= max_register)
	{
		fprintf(stderr, "FATAL: trying to set invalid register %d", index);
		fatal(ERR_INVALID_REGISTER_ACCESS, machine);
	}
	#endif

	uint32_t old_value = machine->registers[index];
	machine->registers[index] = value;

	return old_value;
}

int peek(Operation* operation, Machine* machine)
{
	uint32_t pc = get_register(PC_REGISTER, machine, 1);
	Array* program = get_array(PROGRAM_ARRAY, machine);

	#ifndef UNSAFE
	if (pc >= program->size)
	{
		fprintf(stderr, "FATAL: program execution reached the end and no halt operation was encountered\n");
		fprintf(stderr, "pc = %u, last platter = %u\n", pc, program->size);
		fatal(ERR_PROGRAM_EXECUTION_ENDED_UNEXPECTEDLY, machine);
	}
	#endif

	int_to_operation(program->content[pc], operation);

	return set_register(PC_REGISTER, pc + 1, machine, 1);
}

void dump_memory(Machine* machine)
{
	printf("***DUMPING MEMORY***\n");

	FILE* out = fopen("memdump.txt", "w+");

	if (!out)
	{
		fprintf(stderr, "ERROR: Error opening memdump file.");
		return;
	}

	uint8_t i = 0;
	for(i = 0; i < REGISTERS_COUNT; i++)
	{
		int32_t value = get_register(i, machine, 0);
		fprintf(out, "R%d: %d (unsigned = %u) (hex = 0x%x)\n", i, value, value, value);
	}

	fprintf(out, "\n\nAllocated arrays: %d\n\n", machine->memory.size);

	uint32_t k = 0;
	uint32_t j = 0;

	for (j = 0; j < machine->memory.size; j++)
	{
		if (machine->memory.arrays[j].content != NULL)
		{
			fprintf(out, "-- Array %lu is %lu platters (%lu bytes) --\n\n", j, machine->memory.arrays[j].size, machine->memory.arrays[j].size * sizeof(uint32_t));

			for (k = 0; k < machine->memory.arrays[j].size; k++)
			{
				fprintf(out, "%x ", machine->memory.arrays[j].content[k]);
			}
		}
		else
		{
			fprintf(out, "-- Array %d is not allocated", j);
		}

		fprintf(out, "\n\n-- END --\n\n");
	}

	fclose(out);
}

void dump_state(Machine* machine, Operation* operation, uint32_t inst)
{
	if (operation->standard.number != 13) 
	{
		printf("code: %x, op: %u, a: %u, b: %u, c: %u, ", 
			inst,
			operation->standard.number,
			operation->standard.a,
			operation->standard.b,
			operation->standard.c
		);
	}
	else
	{
		printf("code: %x, op: %u, a: %u, data: %u, ", 
			inst,
			operation->put.number,
			operation->put.a,
			operation->put.value
		);
	}
	
	uint8_t i = 0;
	for(i = 0; i < REGISTERS_COUNT; i++)
	{
		int32_t value = get_register(i, machine, 0);
		printf("R%d: %u, ", i, value);
	}

	printf("pc: %u, cycle: %u\n", get_register(PC_REGISTER, machine, 1), cycle);
}

/**
 * The register A receives the value in register B,
 * unless the register C contains 0.
 */

void conditional_move(Operation* op, Machine* machine)
{
	TRACE("conditional_move r%d into r%d\n", op->standard.b, op->standard.a);

	if (get_register(op->standard.c, machine, 0))
		set_register(op->standard.a, get_register(op->standard.b, machine, 0), machine, 0);
}

/**
 * The register A receives the value stored at offset
 * in register C in the array identified by B.
 */

void array_index(Operation* op, Machine* machine)
{
	TRACE("array_index accessing array[%d][%d] into r%d\n", op->standard.b, op->standard.c, op->standard.a);

	uint32_t value = read_array(
		get_register(op->standard.b, machine, 0),
		get_register(op->standard.c, machine, 0),
		machine
	);

	set_register(op->standard.a, value, machine, 0);
}

/**
 * The array identified by A is amended at the offset
 * in register B to store the value in register C.
 */

void array_amendment(Operation* op, Machine* machine)
{
	Array* array = get_array(get_register(op->standard.a, machine, 0), machine);

	uint32_t location = get_register(op->standard.b, machine, 0);

	#ifndef UNSAFE
	if (location >= array->size)
	{
		fprintf(stderr, "FATAL: trying to set value of array in r%d at %d, beyond its last index %d.\n",
			op->standard.b, location, array->size - 1
		);

		fatal(ERR_MEMORY_ACCESS_INVALID, machine);
	}
	#endif

	uint32_t value = get_register(op->standard.c, machine, 0);
	TRACE("loading %u into array[%d][%d]\n", value, op->standard.a, location);
	array->content[location] = value;
}

/**
 * The register A receives the value in register B plus
 * the value in register C, modulo 2^32.
 */
void addition(Operation* op, Machine* machine)
{
	uint32_t c = get_register(op->standard.c, machine, 0);
	uint32_t b = get_register(op->standard.b, machine, 0);
	TRACE("setting r%d = %u + %u\n", op->standard.a, b, c);
	set_register(op->standard.a, b + c, machine, 0);
}

/**
 * The register A receives the value in register B times
 * the value in register C, modulo 2^32.
 */
void multiplication(Operation* op, Machine* machine)
{
	TRACE("setting r%d = r%d * r%d\n", op->standard.a, op->standard.b, op->standard.c);
	set_register(op->standard.a, get_register(op->standard.b, machine, 0) * get_register(op->standard.c, machine, 0), machine, 0);
}

/**
 * The register A receives the value in register B
 * divided by the value in register C, if any, where
 * each quantity is treated treated as an unsigned 32
 * bit number.
 */

void division(Operation* op, Machine* machine)
{
	uint32_t divisor = get_register(op->standard.c, machine, 0);
	uint32_t dividend = get_register(op->standard.b, machine, 0);

	TRACE("setting r%d = %u / %u\n", op->standard.a, divisor, dividend);

	if (divisor == 0)
	{
		fprintf(stderr, "FATAL: division by zero\n");
		fatal(ERR_DIVISION_BY_ZERO, machine);
	}

	set_register(op->standard.a, dividend / divisor, machine, 0);
}

/**
 * Each bit in the register A receives the 1 bit if
 * either register B or register C has a 0 bit in that
 * position.  Otherwise the bit in register A receives
 * the 0 bit.
 */

void not_and(Operation* op, Machine* machine)
{
	TRACE("setting r%d = ~(r%d & r%d)\n", op->standard.a, op->standard.b, op->standard.c);
	set_register(op->standard.a, ~(get_register(op->standard.b, machine, 0) & get_register(op->standard.c, machine, 0)), machine, 0);
}

/**
 * The universal machine stops computation.
 */

void halt(Operation* op, Machine* machine)
{
	TRACE("halting excution.\n");

#if defined(DEBUG)
	dump_memory(machine);
#endif

	exit(0);
}

/**
 * A new array is created with a capacity of platters
 * commensurate to the value in the register C. This
 * new array is initialized entirely with platters
 * holding the value 0. A bit pattern not consisting of
 * exclusively the 0 bit, and that identifies no other
 * active allocated array, is placed in the B register.
 */

void allocation(Operation* op, Machine* machine)
{
	TRACE("allocating new array with the size in r%d and puts its index in r%d\n", op->standard.c, op->standard.b);
	uint32_t index = allocate_array(get_register(op->standard.c, machine, 0), machine);
	set_register(op->standard.b, index, machine, 0);
}

/**
 * The array identified by the register C is abandoned.
 * Future allocations may then reuse that identifier.
 */

void abandoment(Operation* op, Machine* machine)
{
	TRACE("freeing array at index r%d\n", op->standard.c);

	uint32_t index = (uint32_t)get_register(op->standard.c, machine, 0);
	Array* a = get_array(index, machine);

	#ifndef UNSAFE
	if (a->content == NULL)
	{
		fprintf(stderr, "FATAL: deallocating a non allocated array %d.\n", index);
		fatal(ERR_MEMORY_ACCESS_INVALID, machine);
	}
	#endif

	free(machine->memory.arrays[index].content);
	machine->memory.arrays[index].content = NULL;
	machine->memory.arrays[index].size = 0;
	machine->memory.pool[machine->memory.pool_pointer++] = index;
}

/**
 * The value in the register C is displayed on the console
 * immediately. Only values between and including 0 and 255
 * are allowed.
 */

void output(Operation* op, Machine* machine)
{
	char c = (char)get_register(op->standard.c, machine, 0);
	printf("%c", c);
}

/**
 * The universal machine waits for input on the console.
 * When input arrives, the register C is loaded with the
 * input, which must be between and including 0 and 255.
 * If the end of input has been signaled, then the
 * register C is endowed with a uniform value pattern
 * where every place is pregnant with the 1 bit.
 */

void input(Operation* op, Machine* machine)
{
	char c = getc(stdin);
	set_register(op->standard.c, c == EOF ? 0xFFFFFFFF : c, machine, 0);
}

/**
 * The array identified by the B register is duplicated
 * and the duplicate shall replace the '0' array,
 * regardless of size. The execution finger is placed
 * to indicate the platter of this array that is
 * described by the offset given in C, where the value
 * 0 denotes the first platter, 1 the second, et
 * cetera.
 *
 * The '0' array shall be the most sublime choice for
 * loading, and shall be handled with the utmost
 * velocity.
 */

void load_program(Operation* op, Machine* machine)
{
	uint32_t index = get_register(op->standard.b, machine, 0);
	TRACE("loading program at array[%d] setting execution at offset %d\n", index, op->standard.c);

	if (index)
	{
		TRACE("loading program from non 0 array, copying from %d into 0\n", index);

		Array* src = get_array(index, machine);

		#ifndef UNSAFE
		if (src->content == NULL)
		{
			fprintf(stderr, "FATAL: loading program from an unallocated array %d.\n", index);
			fatal(ERR_MEMORY_ACCESS_INVALID, machine);
		}
		#endif

		allocate_memory(PROGRAM_ARRAY, src->size, machine);

		Array* program = get_array(PROGRAM_ARRAY, machine);
		memcpy(program->content, src->content, src->size * sizeof(uint32_t));
	}

	set_register(PC_REGISTER, get_register(op->standard.c, machine, 0), machine, 1);
}

/**
 * The value indicated is loaded into the register A
 * forthwith.
 */

void ortography(Operation* op, Machine* machine)
{
	TRACE("Setting register r%d = %d\n", op->put.a, op->put.value);
	set_register(op->put.a, op->put.value, machine, 0);
}
