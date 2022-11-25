# Change this to change where `make install` places the `riff` executable
LOC           = /usr/local/bin

RIFF_VERSION  = $(shell git describe)

CFLAGS        = -O3
CFLAGS       += $(shell pcre2-config --cflags)
CFLAGS       += -Wunused

LDFLAGS       = -lm
LDFLAGS      += $(shell pcre2-config --libs8)

WASM_CFLAGS   = -O3
WASM_CFLAGS  += -Ilib/pcre2/src -DPCRE2_STATIC

WASM_LDFLAGS  = -lm
WASM_LDFLAGS += -Llib/pcre2/.libs -lpcre2-8

WASM_SFLAGS   = -sEXPORTED_FUNCTIONS=_riff_version,_riff_main
WASM_SFLAGS  += -sEXPORTED_RUNTIME_METHODS=ccall
WASM_SFLAGS  += -sALLOW_MEMORY_GROWTH=1
WASM_SFLAGS  += -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=$$ccall
WASM_SFLAGS  += -sINCOMING_MODULE_JS_API=onRuntimeInitialized,print,printErr

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
SRC          += src/fmt.c
SRC          += src/fn.c
SRC          += src/lex.c
SRC          += src/lib_base.c
SRC          += src/lib_io.c
SRC          += src/lib_math.c
SRC          += src/lib_os.c
SRC          += src/lib_prng.c
SRC          += src/lib_str.c
SRC          += src/parse.c
SRC          += src/prng.c
SRC          += src/regex.c
SRC          += src/state.c
SRC          += src/string.c
SRC          += src/table.c
SRC          += src/util.c
SRC          += src/value.c
SRC          += src/vm.c

BATSDIR       = test

BATSFLAGS     = --pretty
BATSFLAGS    += --print-output-on-failure
BATSFLAGS    += --recursive

# Version string (riff -v)
CFLAGS       += -DRIFF_VERSION=\"$(RIFF_VERSION)\"
WASM_CFLAGS  += -DRIFF_VERSION=\"$(RIFF_VERSION)\"

# Homebrew-installed Clang
BREW_CLANG    = /opt/homebrew/bin/clang

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

test: bin/riff bats

bats:
	bats $(BATSFLAGS) $(BATSDIR)

warn: bin/warn

wasm: $(SRC)
# Build PCRE2 if necessary
ifeq ($(wildcard lib/pcre2/pcre2test.wasm),)
	@echo Building PCRE2
	(cd lib/pcre2; ./autogen.sh; emconfigure ./configure --disable-shared; emmake make)
endif
	mkdir -p bin
	emcc $(WASM_CFLAGS) $(SRC) $(WASM_LDFLAGS) $(WASM_SFLAGS) -o bin/riff.js

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
