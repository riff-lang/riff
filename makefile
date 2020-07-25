CFLAGS  = -O3

MFLAGS  = -O0
MFLAGS += -fsanitize=address
MFLAGS += -fsanitize-address-use-after-scope

WFLAGS  = -Wall
WFLAGS += -Wextra
WFLAGS += -Wpedantic
WFLAGS += -Weverything

SRC     = src/riff.c
SRC    += src/array.c
SRC    += src/code.c
SRC    += src/disas.c
SRC    += src/hash.c
SRC    += src/lex.c
SRC    += src/parse.c
SRC    += src/str.c
SRC    += src/val.c
SRC    += src/vm.c

all: compile

compile:
	$(CC) $(CFLAGS) $(SRC)

mem:
	$(CC) $(MFLAGS) $(SRC)

warn:
	$(CC) $(CFLAGS) $(WFLAGS) $(SRC)
