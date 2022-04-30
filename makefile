# Change this to change where `make install` places the `riff` executable
LOC           = /usr/local/bin

CFLAGS        = -O3
CFLAGS       += $(shell pcre2-config --cflags)
CFLAGS       += -Wunused

LDFLAGS       = -lm
LDFLAGS      += $(shell pcre2-config --libs8)

WASM_CFLAGS   = -O3
WASM_CFLAGS  += -Ilib/pcre2/src

WASM_LDFLAGS  = -lm
WASM_LDFLAGS += -Llib/pcre2/.libs -lpcre2-8

# AddressSanitizer
AFLAGS        = -g
AFLAGS       += -fsanitize=address
AFLAGS       += -fsanitize-address-use-after-scope

IFLAGS        = -fprofile-instr-generate
IFLAGS       += -fcoverage-mapping

# LeakSanitizer (standalone)
# LSan can be run on top of ASan with the bin/asan executable:
#   $ ASAN_OPTIONS=detect_leaks=1 bin/asan ...
LFLAGS        = -g
LFLAGS       += -fsanitize=leak

WFLAGS        = -Wall
WFLAGS       += -Wextra
WFLAGS       += -Wpedantic
WFLAGS       += -Weverything

SRC           = src/riff.c
SRC          += src/code.c
SRC          += src/disas.c
SRC          += src/env.c
SRC          += src/fmt.c
SRC          += src/fn.c
SRC          += src/lex.c
SRC          += src/lib.c
SRC          += src/parse.c
SRC          += src/regex.c
SRC          += src/string.c
SRC          += src/table.c
SRC          += src/util.c
SRC          += src/value.c
SRC          += src/vm.c

TESTS         = test/expressions.bats
TESTS        += test/fmt.bats
TESTS        += test/literals.bats
TESTS        += test/etc.bats

# Version string (riff -v)
CFLAGS       += -DRIFF_VERSION=\"$(shell git describe)\"

# Homebrew-installed Clang
BREW_CLANG    = /usr/local/opt/llvm/bin/clang

.PHONY: clean

all: bin/riff

clean:
	rm -rf bin
	rm -rf *.dSYM

install: bin/riff
	install bin/riff $(LOC)/riff

asan: bin/asan
instr: bin/instr
lsan: bin/lsan

test: bin/riff
	bats -p $(TESTS)

warn: bin/warn

wasm: $(SRC)
	mkdir -p bin
	emcc $(WASM_CFLAGS) -Ilib/pcre2/src $(SRC) $(WASM_LDFLAGS) -s EXPORTED_FUNCTIONS='["_wasm_main"]' -s EXPORTED_RUNTIME_METHODS='["ccall"]' -s ALLOW_MEMORY_GROWTH=1 -o bin/riff.js

# Run this target if PCRE2 not built locally
pcre2-wasm:
	(cd lib/pcre2; ./autogen.sh; emconfigure ./configure --disable-shared; emmake make)

# Targets

bin/riff: $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

bin/asan: $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) $(AFLAGS) $(SRC) -o $@ $(LDFLAGS)

bin/lsan: $(SRC)
# LeakSanitizer doesn't seem to be supported by the default compiler
# on macOS - use Homebrew-installed clang instead
ifneq ($(wildcard $(BREW_CLANG)),)
	mkdir -p bin
	$(BREW_CLANG) $(CFLAGS) $(LFLAGS) $(SRC) -o $@ $(LDFLAGS)
else
	@echo ERROR: LeakSanitizer requires Brew-installed clang
endif

bin/instr: $(SRC)
	mkdir -p bin
	$(BREW_CLANG) $(CFLAGS) $(IFLAGS) $(SRC) -o $@ $(LDFLAGS)

bin/warn: $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) $(WFLAGS) $(SRC) -o $@ $(LDFLAGS)
