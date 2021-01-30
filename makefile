# Change this to change where `make install` places the `riff` executable
LOC     = /usr/local/bin

CFLAGS  = -O3

LDFLAGS = -lm

MFLAGS  = -O0
MFLAGS += -fsanitize=address
MFLAGS += -fsanitize-address-use-after-scope

PFLAGS  = -g
PFLAGS += -fprofile-instr-generate
PFLAGS += -fcoverage-mapping

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
SRC    += src/util.c
SRC    += src/val.c
SRC    += src/vm.c

TESTS   = test/expressions.bats
TESTS  += test/literals.bats

.PHONY: all clean install mem prof test warn

all: bin/riff

clean:
	rm -rf bin
	rm -rf *.dSYM

install: bin/riff
	install bin/riff $(LOC)/riff

mem: bin/mem

prof: bin/prof

test: bin/riff
	bats -p $(TESTS)

warn: bin/warn

# Targets

bin/riff: $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) $(SRC) -o bin/riff $(LDFLAGS)

bin/mem: $(SRC)
	mkdir -p bin
	$(CC) $(MFLAGS) $(SRC) -o bin/mem $(LDFLAGS)

bin/prof: $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) $(PFLAGS) $(SRC) -o bin/prof $(LDFLAGS)

bin/warn: $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) $(WFLAGS) $(SRC) -o bin/warn $(LDFLAGS)
