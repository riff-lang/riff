# Change this to change where `make install` places the `riff` executable
LOC     = /usr/local/bin

CFLAGS  = -O3

LDFLAGS  = `pkg-config --cflags --libs libpcre2-8`
LDFLAGS += -lm

# AddressSanitizer
AFLAGS  = -g
AFLAGS += -fsanitize=address
AFLAGS += -fsanitize-address-use-after-scope

# LeakSanitizer (standalone)
# LSan can be run on top of ASan with the bin/asan executable:
#   $ ASAN_OPTIONS=detect_leaks=1 bin/asan ...
LFLAGS  = -g
LFLAGS += -fsanitize=leak

PFLAGS  = -g
PFLAGS += -fprofile-instr-generate
PFLAGS += -fcoverage-mapping

WFLAGS  = -Wall
WFLAGS += -Wextra
WFLAGS += -Wpedantic
WFLAGS += -Weverything

SRC     = src/riff.c
SRC    += src/code.c
SRC    += src/disas.c
SRC    += src/fn.c
SRC    += src/hash.c
SRC    += src/lex.c
SRC    += src/lib.c
SRC    += src/parse.c
SRC    += src/re.c
SRC    += src/str.c
SRC    += src/table.c
SRC    += src/util.c
SRC    += src/val.c
SRC    += src/vm.c

TESTS   = test/expressions.bats
TESTS  += test/literals.bats
TESTS  += test/etc.bats

.PHONY: all clean install mem prof test warn

all: bin/riff

clean:
	rm -rf bin
	rm -rf *.dSYM

install: bin/riff
	install bin/riff $(LOC)/riff

asan: bin/asan
lsan: bin/lsan
prof: bin/prof

test: bin/riff
	bats -p $(TESTS)

warn: bin/warn
wasm: bin/wasm

# Targets

bin/riff: $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) $(SRC) -o bin/riff $(LDFLAGS)

bin/asan: $(SRC)
	mkdir -p bin
	$(CC) $(AFLAGS) $(SRC) -o bin/asan $(LDFLAGS)

bin/lsan: $(SRC)
# LeakSanitizer doesn't seem to be supported by the default compiler
# on macOS - use Homebrew-installed clang instead
ifneq ($(wildcard /usr/local/opt/llvm/bin/clang),)
	mkdir -p bin
	/usr/local/opt/llvm/bin/clang $(LFLAGS) $(SRC) -o bin/lsan $(LDFLAGS)
else
	@echo ERROR: LeakSanitizer requires Brew-installed clang
endif

bin/prof: $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) $(PFLAGS) $(SRC) -o bin/prof $(LDFLAGS)

bin/warn: $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) $(WFLAGS) $(SRC) -o bin/warn $(LDFLAGS)

bin/wasm: $(SRC)
	mkdir -p bin
	emcc $(CFLAGS) $(SRC) -s EXPORTED_FUNCTIONS='["_wasm_main"]' -s EXPORTED_RUNTIME_METHODS='["ccall"]' -s ALLOW_MEMORY_GROWTH=1 -o bin/riff.js
