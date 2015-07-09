#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define ERR_MISSING_ARGUMENTS 2
#define ERR_INVALID_INPUT_FILE 3
#define ERR_INVALID_OUTPUT_FILE 4
#define ERR_COMPILATION_FAILED 5

#define INVALID_OPERATION_CODE 255

#include "operation.h"

int is_empty_line(const char* line, size_t length);
void parse_line(const char* line, size_t line_count, Operation* op);
uint8_t get_operation_code(const char* code);

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s program [outfile]\n", argv[0]);
		exit(ERR_MISSING_ARGUMENTS);
	}

	char* input_filename = argv[1];
	char* output_filename = "output.umz";

	if (argc > 2)
		output_filename =  argv[2];

	FILE* input_file = fopen(input_filename, "r");

	if (input_file == NULL)
	{
		fprintf(stderr, "FATAL: Can't open input file: %s\n", input_filename);
		exit(ERR_INVALID_INPUT_FILE);
	}

	FILE* output_file = fopen(output_filename, "wb");

	if (output_file == NULL)
	{
		fprintf(stderr, "FATAL: Can't open output file: %s\n", output_filename);
		exit(ERR_INVALID_OUTPUT_FILE);
	}

	char* current_line = NULL;
	size_t length = 0;
	size_t line_count = 1;
	Operation op;
	uint32_t value;

	while(getline(&current_line, &length, input_file) != -1)
	{
		parse_line(current_line, line_count++, &op);
		value = operation_to_int(&op);
		fwrite(&value, sizeof(uint32_t), 1, output_file);

		free(current_line);
		current_line = NULL;
	}

	if (!feof(input_file))
	{
		fprintf(stderr, "FATAL: An error occurred before reaching the end of the input file: %d\n", errno);
		exit(ERR_INVALID_INPUT_FILE);
	}

	fclose(input_file);
	fclose(output_file);
}

int is_empty_line(const char* line, size_t length)
{
	size_t i;
	for(i = 0; i < length; i++)
	{
		if (line[i] != ' ' && line[i] != '\n' && line[i] != '\r' && line[i] != '\t')
			return 0;
	}

	return 1;
}

void parse_line(const char* line, size_t line_count, Operation* op)
{
	if (op == NULL)
		return;

	size_t length = strlen(line);

	if (length <= 0)
		return;

	if (line[0] == '#')
		return;

	if (is_empty_line(line, length))
		return;

	char* ac = NULL;
	char* bc = NULL;
	char* cc = NULL;

	char* token = strtok((char*)line, " \n");
	char* number_end = NULL;

	uint8_t code_number = INVALID_OPERATION_CODE;

	while (token != NULL)
	{
		if (strlen(token))
		{
			if (code_number == INVALID_OPERATION_CODE)
			{
				code_number = get_operation_code(token);

				if (code_number == INVALID_OPERATION_CODE)
				{
					fprintf(stderr, "COMPILATION ERROR: Invalid operation '%s' found at line %zu\n", token, line_count);
					exit(ERR_COMPILATION_FAILED);
				}
			}
			else if (ac == NULL)
			{
				ac = token;
			}
			else if (bc == NULL)
			{
				bc = token;
			}
			else
			{
				cc = token;
			}
		}

		token = strtok(NULL, " \n");
	}

	if (code_number < 13)
	{
		if (ac == NULL || bc == NULL || cc == NULL)
		{
			fprintf(stderr, "COMPILATION ERROR: Wrong number of arguments at line %zu\n", line_count);
			exit(ERR_COMPILATION_FAILED);
		}

		uint8_t a = strtol(ac, &number_end, 10);
		uint8_t b = strtol(bc, &number_end, 10);
		uint8_t c = strtol(cc, &number_end, 10);

		if (errno == ERANGE)
		{
			fprintf(stderr, "COMPILATION ERROR: One of the registers as a wrong value at line %zu\n", line_count);
			exit(ERR_COMPILATION_FAILED);
		}

		if (a >= 8)
		{
			fprintf(stderr, "COMPILATION ERROR: Wrong register number for a '%d' at line %zu\n", a, line_count);
			exit(ERR_COMPILATION_FAILED);
		}

		if (b >= 8)
		{
			fprintf(stderr, "COMPILATION ERROR: Wrong register number for b '%d' at line %zu\n", a, line_count);
			exit(ERR_COMPILATION_FAILED);
		}

		if (c >= 8)
		{
			fprintf(stderr, "COMPILATION ERROR: Wrong register number for c '%d' at line %zu\n", a, line_count);
			exit(ERR_COMPILATION_FAILED);
		}

		op->standard.number = code_number;
		op->standard.a = a;
		op->standard.b = b;
		op->standard.c = c;
	}
	else
	{
		if (ac == NULL || bc == NULL)
		{
			fprintf(stderr, "COMPILATION ERROR: Wrong number of arguments at line %zu\n", line_count);
			exit(ERR_COMPILATION_FAILED);
		}

		uint8_t a = strtol(ac, &number_end, 10);
		uint32_t value = strtol(bc, &number_end, 10);

		if (errno == ERANGE)
		{
			fprintf(stderr, "COMPILATION ERROR: The register or value has a wrong value at line %zu\n", line_count);
			exit(ERR_COMPILATION_FAILED);
		}

		if (a >= 8)
		{
			fprintf(stderr, "COMPILATION ERROR: Wrong register number for a '%d' at line %zu\n", a, line_count);
			exit(ERR_COMPILATION_FAILED);
		}

		if (value >= 33554432) // 25 bits
		{
			fprintf(stderr, "COMPILATION ERROR: Out of range value '%d' at line %zu\n", value, line_count);
			exit(ERR_COMPILATION_FAILED);
		}

		op->put.number = code_number;
		op->put.a = a;
		op->put.value = value;
	}
}

uint8_t get_operation_code(const char* code)
{
	if (strcmp(code, "cmove") == 0)
		return 0;
	else if (strcmp(code, "get") == 0)
		return 1;
	else if (strcmp(code, "set") == 0)
		return 2;
	else if (strcmp(code, "add") == 0)
		return 3;
	else if (strcmp(code, "mult") == 0)
		return 4;
	else if (strcmp(code, "div") == 0)
		return 5;
	else if (strcmp(code, "nand") == 0)
		return 6;
	else if (strcmp(code, "halt") == 0)
		return 7;
	else if (strcmp(code, "allocate") == 0)
		return 8;
	else if (strcmp(code, "free") == 0)
		return 9;
	else if (strcmp(code, "out") == 0)
		return 10;
	else if (strcmp(code, "in") == 0)
		return 11;
	else if (strcmp(code, "load") == 0)
		return 12;
	else if (strcmp(code, "put") == 0)
		return 13;

	return INVALID_OPERATION_CODE;
}
