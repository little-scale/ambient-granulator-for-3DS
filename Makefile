# Native Nintendo 3DS application built with devkitARM and libctru.

.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM. Use ./scripts/build.sh for the pinned container build")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET          := 3ds_granulator
BUILD           := build/3ds
SOURCES         := source
INCLUDES        := include
ROMFS           := romfs

APP_TITLE       := Ambient Granulator for 3DS
APP_DESCRIPTION := Native polyphonic granular sampler
APP_AUTHOR      := little-scale

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS := -g -Wall -Wextra -O2 -std=gnu11 -mword-relocations \
          -ffunction-sections $(ARCH)
CFLAGS += $(INCLUDE) -D__3DS__

ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS    := -lcitro2d -lcitro3d -lctru -lm
LIBDIRS := $(CTRULIB)

ifneq ($(CURDIR),$(TOPDIR)/$(BUILD))

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export LD := $(CC)
export OFILES := $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := -I$(CURDIR)/$(BUILD) \
                  $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXDEPS := $(OUTPUT).smdh
export _3DSXFLAGS += --smdh=$(OUTPUT).smdh
ifneq ($(ROMFS),)
export _3DSXDEPS += $(CURDIR)/$(ROMFS)/sample_bank.bin
export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: all clean test verify

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -rf build/3ds $(TARGET).3dsx $(TARGET).elf $(TARGET).map $(TARGET).smdh

test:
	@./scripts/test.sh

verify:
	@./scripts/verify-build.sh

else

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)

$(OUTPUT).elf: $(OFILES)

-include $(DEPSDIR)/*.d

endif
