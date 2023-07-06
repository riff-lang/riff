prefix        ?= $(HOME)/.local
exec_prefix   ?= $(prefix)
datarootdir   ?= $(prefix)/share
bindir        ?= $(exec_prefix)/bin
mandir        ?= $(datarootdir)/man
man1dir       ?= $(mandir)/man1
extdir        ?= ext
srcdir        ?= src

pandoc        ?= pandoc
pcre2-config  ?= pcre2-config
pandocdatadir ?= ref/pandoc-data
docrootdir    ?= ref/doc
mandatadir    ?= man

binbuilddir   ?= bin
man1builddir  ?= man/man1

clang         ?= $(shell brew --prefix)/bin/clang
version       ?= $(shell git describe)

CFLAGS         = -O3
CFLAGS        += -Wunused
CFLAGS        += -DRIFF_VERSION=\"$(version)\"
CFLAGS        += $(shell $(pcre2-config) --cflags)

LDFLAGS        = -lm
LDFLAGS       += $(shell $(pcre2-config) --libs8)

WASM_SFLAGS   := -sEXPORTED_FUNCTIONS=_riff_version,_riff_main
WASM_SFLAGS   += -sEXPORTED_RUNTIME_METHODS=ccall
WASM_SFLAGS   += -sALLOW_MEMORY_GROWTH=1
WASM_SFLAGS   += -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=$$ccall
WASM_SFLAGS   += -sINCOMING_MODULE_JS_API=onRuntimeInitialized,print,printErr

# AddressSanitizer
AFLAGS        := -fsanitize=address
AFLAGS        += -fsanitize-address-use-after-scope

IFLAGS        := -fprofile-instr-generate
IFLAGS        += -fcoverage-mapping

# LeakSanitizer (standalone)
# LSan can be run on top of ASan with the bin/asan executable:
#   $ ASAN_OPTIONS=detect_leaks=1 bin/asan ...
LFLAGS        := -fsanitize=leak

WFLAGS        := -Wall
WFLAGS        += -Wextra
WFLAGS        += -Wpedantic
WFLAGS        += -Weverything

SRC           := $(srcdir)/*.c

BATSDIR       := test

BATSFLAGS     := --pretty
BATSFLAGS     += --print-output-on-failure
BATSFLAGS     += --recursive

TARGET_STEMS  := riff asan instr lsan riff.js

TARGETS       := $(addprefix $(binbuilddir)/, $(TARGET_STEMS))
BIN_TARGETS   := $(addprefix $(binbuilddir)/, $(filter-out riff.js, $(TARGET_STEMS)))
CLANG_TARGETS := $(addprefix $(binbuilddir)/, instr lsan)
DEBUG_TARGETS := $(addprefix $(binbuilddir)/, asan lsan)
WASM_TARGETS  := $(binbuilddir)/riff.js

MANFLAGS      := --data-dir=$(mandatadir)
MANFLAGS      += --defaults=$(pandocdatadir)/defaults/markdown
MANFLAGS      += --template=riff.man
MANFLAGS      += --metadata date="$(shell date +"%B %e, %Y")"
MANFLAGS      += --metadata riff-version="$(version)"
MANFLAGS      += --metadata-file=common.yaml

PANDOCPP_BASE := $(pandoc) --defaults=$(pandocdatadir)/defaults/markdown -t markdown
PANDOC_MAN1    = $(pandoc) $(MANFLAGS) --metadata title=$(basename $(@F)) --metadata-file=$(@F).yaml -o $@

# --shift-level-heading-by=-1 strips H1 header(s) from the input
PANDOCPP_H0   := $(PANDOCPP_BASE) --shift-heading-level-by=-1
PANDOCPP_H2   := $(PANDOCPP_BASE) --shift-heading-level-by=1
MAN_STEMS     := $(basename $(notdir $(wildcard man/metadata/riff*.1.yaml)))
MAN_TARGETS   := $(addprefix $(man1builddir)/, $(MAN_STEMS))


# Phony targets

.PHONY: $(TARGET_STEMS) $(MAN_TARGET_STEMS) bats clean install man test warn wasm

all: riff

clean:
	rm -rf $(binbuilddir)
	rm -rf $(man1builddir)
	rm -rf *.dSYM

install: riff man | $(bindir) $(man1dir)
	install $(binbuilddir)/riff $(bindir)/riff
	cp $(man1builddir)/* $(man1dir)

test: riff bats

bats:
	bats $(BATSFLAGS) $(BATSDIR)

man: $(MAN_TARGETS)

warn: CFLAGS += $(WFLAGS)
warn: riff

wasm: $(WASM_TARGETS)

.SECONDEXPANSION:
$(TARGET_STEMS): $(binbuilddir)/$$@
$(MAN_STEMS): $(man1builddir)/$$@


# Compilation targets

# CC overrides
$(CLANG_TARGETS): CC = $(clang)
$(WASM_TARGETS):  CC = emcc

# Extra CFLAGS
$(DEBUG_TARGETS):     CFLAGS += -g
$(binbuilddir)/asan:  CFLAGS += $(AFLAGS)
$(binbuilddir)/instr: CFLAGS += $(IFLAGS)
$(binbuilddir)/lsan:  CFLAGS += $(LFLAGS)
$(WASM_TARGETS):      CFLAGS += $(WASM_SFLAGS)

$(WASM_TARGETS): pcre2-config = $(extdir)/pcre2/pcre2-config
$(WASM_TARGETS): | $(extdir)/pcre2/pcre2test.wasm

$(TARGETS): $(SRC) | $(binbuilddir)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)


# Man page targets

$(MAN_TARGETS): | $(man1builddir)

SYNDIR := $(docrootdir)/syntax
FNDIR  := $(docrootdir)/builtin/fn

$(man1builddir)/rifflib.1: $(FNDIR)/*.md
$(man1builddir)/riffprng.1: $(docrootdir)/prng.md $(FNDIR)/*rand.md
$(man1builddir)/riffop.1: $(docrootdir)/op/*.md
$(man1builddir)/riffregex.1: $(SYNDIR)/literal/regex.md $(addprefix $(docrootdir)/op/,match.md fieldtable.md) $(addprefix $(FNDIR)/, split.md gsub.md sub.md)
$(man1builddir)/riffsyn.1: $(addprefix $(SYNDIR)/,_intro.md comments.md literal/*.md variables.md stmt/*.md functions.md)
$(man1builddir)/riffvar.1: $(docrootdir)/builtin/var/*.md

comma := ,
space :=
space +=
SECONDARY_PAGES := $(addprefix $(man1builddir)/,$(filter-out riff.1, $(MAN_STEMS)))
$(man1builddir)/riff.1: $(docrootdir)/intro.md $(SECONDARY_PAGES)
	$(PANDOC_MAN1) $< --metadata see-also="$(subst $(space),$(comma)$(space),$(addsuffix (1),$(basename $(notdir $(filter-out $<,$^)))))"

$(addprefix $(man1builddir)/,rifflib.1 riffop.1 riffvar.1):
	$(PANDOCPP_H2) $^ | $(PANDOC_MAN1) -

$(addprefix $(man1builddir)/,riffprng.1 riffregex.1 riffsyn.1):
	$(PANDOCPP_H0) $< | $(PANDOCPP_H2) - $(filter-out $<,$^) | $(PANDOC_MAN1) -


# Order-only prereqs

$(bindir) $(man1dir) $(binbuilddir) $(man1builddir):
	mkdir -p $@

$(extdir)/pcre2/pcre2test.wasm:
	(cd $(@D); ./autogen.sh; emconfigure ./configure --prefix=$$PWD/wasm-install --disable-shared; emmake make install)
