CFLAGS  = -O3

MFLAGS  = -fsanitize=address
MFLAGS += -fsanitize-address-use-after-scope

WFLAGS  = -Wall
WFLAGS += -Wextra
WFLAGS += -Wpedantic
WFLAGS += -Weverything

SRC     = riff.c
SRC    += code.c
SRC    += disas.c
SRC    += hash.c
SRC    += lex.c
SRC    += parse.c
SRC    += str.c
SRC    += val.c
SRC    += vm.c

all: compile

compile:
	$(CC) $(CFLAGS) $(SRC)

mem:
	$(CC) $(MFLAGS) $(SRC)

warn:
	$(CC) $(CFLAGS) $(WFLAGS) $(SRC)
