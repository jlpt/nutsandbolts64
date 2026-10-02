# Builds the Nuts & Bolts 64 mod image (nb.bin) with a modern MIPS GCC.
CROSS   ?= mips-linux-gnu-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
NM      := $(CROSS)nm
PYTHON  ?= python3

ROOT    := ..
DECOMP  ?= $(ROOT)/decomp
BUILD   ?= $(ROOT)/build/mod
BK_ELF  ?= $(DECOMP)/build/us.v10/banjo.us.v10.elf
TEXDIR  := $(ROOT)/assets/textures
NB_CFLAGS ?=

TEXTURES := wood metal tire hub engine fuel seat canvas glass honeycomb jiggy checker \
            floater jet hazard lamp balloon plain skull note grille
TEX_PNGS := $(addprefix $(TEXDIR)/,$(addsuffix .png,$(TEXTURES)))

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS)) $(BUILD)/nb_textures.o

CFLAGS := -G 0 -O2 -march=vr4300 -mtune=vr4300 -mabi=32 -mfp32 -mhard-float \
          -mno-abicalls -fno-pic -mno-check-zero-division -mfix4300 \
          -ffreestanding -fno-builtin -fno-common -fno-math-errno -fsingle-precision-constant \
          -Wall -Wno-missing-braces -Wno-unused-variable -Wno-unused-but-set-variable \
          -Iinclude -I$(BUILD) -I$(DECOMP)/lib/ultralib/include -I$(DECOMP)/lib/ultralib/include/compiler/modern_gcc \
          -DF3DEX_GBI -D_LANGUAGE_C -D_FINALROM -DNDEBUG -DBUILD_VERSION=VERSION_I \
          -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 $(NB_CFLAGS)

all: $(BUILD)/nb.bin

$(BUILD):
	mkdir -p $@

$(BUILD)/nb_textures.c $(BUILD)/nb_textures.h: $(TEX_PNGS) $(ROOT)/tools/png2n64.py | $(BUILD)
	$(PYTHON) $(ROOT)/tools/png2n64.py $(BUILD)/nb_textures.c $(BUILD)/nb_textures.h 32 $(TEX_PNGS)

$(BUILD)/bk_syms.ld: $(BK_ELF) $(ROOT)/tools/gen_syms.py | $(BUILD)
	$(PYTHON) $(ROOT)/tools/gen_syms.py $(NM) $(BK_ELF) $@

$(BUILD)/%.o: src/%.c include/*.h $(BUILD)/nb_textures.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/nb_textures.o: $(BUILD)/nb_textures.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/nb.elf: $(OBJS) nb.ld $(BUILD)/bk_syms.ld
	$(LD) -T nb.ld -T $(BUILD)/bk_syms.ld -Map $(BUILD)/nb.map --no-check-sections -o $@ $(OBJS)

$(BUILD)/nb.bin: $(BUILD)/nb.elf
	$(OBJCOPY) -O binary -j .text -j .rodata -j .data $< $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
