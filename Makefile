# Declaration of variables
CC= gcc
CC_FLAGS=
LD_FLAGS=

# File names
UM_SOURCES = main.c operation.c
UM_OBJECTS = $(UM_SOURCES:.c=.o)

COMPILER_SOURCES = compiler.c operation.c
COMPILER_OBJECTS = $(COMPILER_SOURCES:.c=.o)

DISASM_SOURCES = disasm.c operation.c
DISASM_OBJECTS = $(DISASM_SOURCES:.c=.o)

all: um compiler disasm

clean:
	rm -f um
	rm -f compiler
	rm -f *.o

um: $(UM_OBJECTS)
	$(CC) $(LD_FLAGS) $(UM_OBJECTS) -o um

disasm: $(DISASM_OBJECTS)
	$(CC) $(LD_FLAGS) $(DISASM_OBJECTS) -o disasm

compiler: $(COMPILER_OBJECTS)
	$(CC) $(LD_FLAGS) $(COMPILER_OBJECTS) -o compiler

%.o: %.c
	$(CC) -c $(CC_FLAGS) $< -o $@
