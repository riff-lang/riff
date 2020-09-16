# Change this to change where `make install` places the `riff` executable
LOC     = /usr/local/bin

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
SRC    += src/fn.c
SRC    += src/hash.c
SRC    += src/lex.c
SRC    += src/lib.c
SRC    += src/parse.c
SRC    += src/str.c
SRC    += src/val.c
SRC    += src/vm.c

TESTS   = test/expressions.bats
TESTS  += test/literals.bats

.PHONY: all clean compile install mem test warn

all: compile

compile:
	mkdir -p dist
	$(CC) $(CFLAGS) $(SRC) -o dist/riff

clean:
	rm -rf dist

install: compile
	install dist/riff $(LOC)/riff

mem:
	$(CC) $(MFLAGS) $(SRC)

test: compile
	bats -p $(TESTS)

warn:
	$(CC) $(CFLAGS) $(WFLAGS) $(SRC)
