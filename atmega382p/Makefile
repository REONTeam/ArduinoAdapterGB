name := $(notdir $(PWD))

dir_source := source
dir_build := build

TARGET_ARCH := -mmcu=atmega328p
TARGET_AVRDUDE := -patmega328p -carduino
SERIAL := /dev/ttyACM0

CC := avr-gcc
OBJCOPY := avr-objcopy
OBJDUMP := avr-objdump
AVRDUDE := avrdude

OPTIM := -Os -g -fdata-sections -ffunction-sections -flto -fuse-linker-plugin -fipa-pta #-fgraphite-identity -floop-nest-optimize
CFLAGS := $(OPTIM) -Wall -Wextra -std=c17 -DF_CPU=16000000L
LDFLAGS := $(OPTIM) -Wl,--gc-sections -Wl,--print-gc-sections

rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))
objects := $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, $(call rwildcard, $(dir_source)/, *.c))

.SECONDEXPANSION:

.PHONY: all
all: $(name).hex $(name).lst

.PHONY: clean
clean:
	rm -rf $(dir_build) $(name).hex $(name).lst

.PHONY: upload
upload: $(name).hex $(name).lst
	$(AVRDUDE) -v $(TARGET_AVRDUDE) -P$(SERIAL) -Uflash:w:$<:i

.PHONY: screen
screen: upload
	minicom -D $(SERIAL) -b 2000000

%.hex: $(dir_build)/%.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

%.lst: $(dir_build)/%.elf
	$(OBJDUMP) -h -S $< > $@

$(dir_build)/$(name).elf: $(objects) | $$(dir $$@)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(dir_build)/%.o: $(dir_source)/%.c | $$(dir $$@)
	$(COMPILE.c) -MMD -MF $(@:.o=.d) $(OUTPUT_OPTION) $<

.PRECIOUS: %/
%/:
	mkdir -p $@

-include $(patsubst %.o, %.d, $(objects))
