#if !defined(__OPERATION_H)
#define __OPERATION_H

typedef struct StandardOperation {
	uint8_t number: 4;
	uint32_t      : 17;
	uint8_t a     : 3;
	uint8_t b     : 3;
	uint8_t c     : 3;
} StandardOperation;

typedef struct PutOperation {
	uint8_t number: 4;
	uint8_t a     : 3;
	uint32_t value: 25;
} PutOperation;

typedef union Operation {
	StandardOperation standard;
	PutOperation put;
} Operation;

uint32_t operation_to_int(Operation* operation);
void int_to_operation(uint32_t value, Operation* operation);

#endif //__OPERATION_H
