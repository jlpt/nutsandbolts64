# Nuts & Bolts 64 - top-level build
#
#   ./tools/setup.sh            (once)
#   make BASEROM=your_rom.z64   -> build/nutsandbolts64.z64
#
# Options:
#   NB_UNLOCK_ALL=1   start with every part unlocked (sandbox mode)
#   NB_DEBUG=1        on-screen physics debug readout
#   TEST_MAP=0x27     boot straight into a level (testing only)

BASEROM ?= baserom.us.v10.z64
DECOMP  := decomp
BUILD   := build
OUT     := $(BUILD)/nutsandbolts64.z64
PYTHON  ?= python3

DECOMP_ROM := $(DECOMP)/build/us.v10/banjo.us.v10.z64
DECOMP_ELF := $(DECOMP)/build/us.v10/banjo.us.v10.elf

MOD_CFLAGS :=
ifeq ($(NB_UNLOCK_ALL),1)
MOD_CFLAGS += -DNB_UNLOCK_ALL
endif
ifeq ($(NB_DEBUG),1)
MOD_CFLAGS += -DNB_DEBUG
endif
DECOMP_CFLAGS :=
ifneq ($(TEST_MAP),)
DECOMP_CFLAGS += -DNB_TEST_BOOTMAP=$(TEST_MAP)
endif
ifneq ($(TEST_EXIT),)
DECOMP_CFLAGS += -DNB_TEST_EXIT=$(TEST_EXIT)
endif

# rebuild what depends on these flags when they change
$(shell mkdir -p $(DECOMP)/build $(BUILD)/mod; \
	echo '$(DECOMP_CFLAGS)' | cmp -s - $(DECOMP)/build/nb_cflags.stamp || \
	(echo '$(DECOMP_CFLAGS)' > $(DECOMP)/build/nb_cflags.stamp; touch $(DECOMP)/src/core1/code_0.c $(DECOMP)/src/core2/code_5C870.c); \
	echo '$(MOD_CFLAGS)' | cmp -s - $(BUILD)/mod/cflags.stamp || \
	(echo '$(MOD_CFLAGS)' > $(BUILD)/mod/cflags.stamp; touch mod/src/*.c))

all: $(OUT)

$(DECOMP)/baserom.us.v10.z64: | $(BASEROM)
	$(PYTHON) tools/prepare_rom.py $(BASEROM) $@

$(BASEROM):
	@echo "Missing ROM: pass your Banjo-Kazooie USA v1.0 ROM with make BASEROM=path/to/rom" && false

decomp: $(DECOMP)/baserom.us.v10.z64
	@git -C $(DECOMP) apply --reverse --check ../patches/0001-nutsandbolts64-hooks.patch 2>/dev/null || \
		(echo "hook patch not applied - run ./tools/setup.sh" && false)
	$(MAKE) -C $(DECOMP) build/us.v10/banjo.us.v10.z64 ANTI_TAMPER=0 ANTI_PIRACY=0 NB_CFLAGS="$(DECOMP_CFLAGS)"

mod: decomp
	$(MAKE) -C mod NB_CFLAGS="$(MOD_CFLAGS)"

$(OUT): mod
	$(PYTHON) tools/mkrom.py $(DECOMP_ROM) $(BUILD)/mod/nb.bin $@

textures:
	$(PYTHON) tools/gen_textures.py

test:
	mkdir -p $(BUILD)/test
	$(PYTHON) tools/png2n64.py $(BUILD)/test/nb_textures.c $(BUILD)/test/nb_textures.h 32 assets/textures/wood.png > /dev/null
	cc -O1 -w -Imod/include -I$(BUILD)/test -I$(DECOMP)/lib/ultralib/include -I$(DECOMP)/lib/ultralib/include/compiler/modern_gcc \
		-DF3DEX_GBI -D_LANGUAGE_C -o $(BUILD)/test/physics_sim tests/physics_sim.c mod/src/nb_vehicle.c mod/src/nb_parts.c mod/src/nb_math.c -lm
	./$(BUILD)/test/physics_sim

clean:
	rm -rf $(BUILD)
	$(MAKE) -C $(DECOMP) clean

.PHONY: all decomp mod textures test clean
