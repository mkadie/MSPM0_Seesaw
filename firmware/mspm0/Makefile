# MSPM0G3507 LED-blink firmware Makefile
#
# Output: build/firmware.elf, build/firmware.bin, build/firmware.hex.
# Toolchain: GNU Arm Embedded (gcc-arm-none-eabi from Debian/Ubuntu apt).

PREFIX  ?= arm-none-eabi-
CC       = $(PREFIX)gcc
OBJCOPY  = $(PREFIX)objcopy
SIZE     = $(PREFIX)size
OBJDUMP  = $(PREFIX)objdump

BUILDDIR = build
SRCDIR   = src
TARGET   = $(BUILDDIR)/firmware

CPU      = -mcpu=cortex-m0plus -mthumb
CFLAGS   = $(CPU) -Os -g \
           -ffreestanding -nostdlib -fno-builtin \
           -fno-jump-tables \
           -ffunction-sections -fdata-sections \
           -Wall -Wextra -Wno-unused-parameter
LDFLAGS  = $(CPU) -nostdlib \
           -T linker.ld \
           -Wl,--gc-sections -Wl,-Map=$(TARGET).map

SRCS     = $(SRCDIR)/main.c
OBJS     = $(BUILDDIR)/main.o

.PHONY: all clean size dis

all: $(TARGET).bin $(TARGET).hex size

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/main.o: $(SRCDIR)/main.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET).elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

size: $(TARGET).elf
	$(SIZE) $<

dis: $(TARGET).elf
	$(OBJDUMP) -d $(TARGET).elf | head -200

clean:
	rm -rf $(BUILDDIR)
