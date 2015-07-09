#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <byteswap.h>

#define ERR_MISSING_ARGUMENTS 2
#define ERR_INVALID_INPUT_FILE 3
#define ERR_INVALID_OUTPUT_FILE 4

#define WRITE_CODE(operation, name, output) \
	fprintf(output, name " %d %d %d\n", operation->standard.a, operation->standard.b, operation->standard.c)

#define WRITE_CODE_PUT(operation, name, output) \
	fprintf(output, name " %d %d\n", operation->put.a, operation->put.value)

#include "operation.h"

void write_source_code(Operation* op, FILE* output);

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s program [outfile]\n", argv[0]);
		exit(ERR_MISSING_ARGUMENTS);
	}

	char* input_filename = argv[1];
	char* output_filename = "output.uma";

	if (argc > 2)
		output_filename =  argv[2];

	FILE* input_file = fopen(input_filename, "r");

	if (input_file == NULL)
	{
		fprintf(stderr, "FATAL: Can't open input file: %s\n", input_filename);
		exit(ERR_INVALID_INPUT_FILE);
	}

	fseek(input_file, 0, SEEK_END);
	size_t fsize = ftell(input_file);
	fseek(input_file, 0, SEEK_SET);

	if (fsize % sizeof(uint32_t) != 0)
	{
		fprintf(stderr, "FATAL: Input file's size seems invalid: %zu bytes\n", fsize);
		exit(ERR_INVALID_INPUT_FILE);
	}

	FILE* output_file = fopen(output_filename, "wb");

	if (output_file == NULL)
	{
		fprintf(stderr, "FATAL: Can't open output file: %s\n", output_filename);
		exit(ERR_INVALID_OUTPUT_FILE);
	}

	Operation op;
	uint32_t value;

	size_t i;
	for(i = 0; i < fsize; i += sizeof(uint32_t))
	{
		if (fread(&value, sizeof(uint32_t), 1, input_file) != 1)
		{
			fprintf(stderr, "FATAL: error reading input file %s\n", input_filename);
			exit(ERR_INVALID_INPUT_FILE);
		}

		value = __bswap_32(value);

		int_to_operation(value, &op);
		write_source_code(&op, output_file);
	}

	fclose(output_file);
	fclose(input_file);
}

void write_source_code(Operation* op, FILE* output)
{
	switch(op->standard.number)
	{
		case 0:
			WRITE_CODE(op, "cmove", output);
			break;

		case 1:
			WRITE_CODE(op, "get", output);
			break;

		case 2:
			WRITE_CODE(op, "set", output);
			break;

		case 3:
			WRITE_CODE(op, "add", output);
			break;

		case 4:
			WRITE_CODE(op, "mult", output);
			break;

		case 5:
			WRITE_CODE(op, "div", output);
			break;

		case 6:
			WRITE_CODE(op, "nand", output);
			break;

		case 7:
			WRITE_CODE(op, "halt", output);
			break;

		case 8:
			WRITE_CODE(op, "allocate", output);
			break;

		case 9:
			WRITE_CODE(op, "free", output);
			break;

		case 10:
			WRITE_CODE(op, "out", output);
			break;

		case 11:
			WRITE_CODE(op, "in", output);
			break;

		case 12:
			WRITE_CODE(op, "load", output);
			break;

		case 13:
			WRITE_CODE_PUT(op, "put", output);
			break;

		default:
			fprintf(output, "# Wrong opcode detected: %d\n", op->standard.number);
			break;
	}
}
