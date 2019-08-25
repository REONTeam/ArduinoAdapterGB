name := mobile

dir_source := source
dir_build := build

CFLAGS := -O0 -g -Wall -Wextra -std=gnu17 $(CFLAGS)

CFLAGS += #$(shell pkg-config --cflags ...)
LDLIBS += -lpthread #$(shell pkg-config --libs ...)

SANIT := -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=leak -fsanitize=undefined
OPTIM := -Os -fdata-sections -ffunction-sections -flto -fuse-linker-plugin -fipa-pta -Wl,--gc-sections -Wl,--print-gc-sections #-fgraphite-identity -floop-nest-optimize

rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))
objects := $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, $(call rwildcard, $(dir_source)/, *.c))

.SECONDEXPANSION:

.PHONY: all
all: $(name)

.PHONY: clean
clean:
	rm -rf $(dir_build) $(name)

.PHONY: sanit
sanit: CFLAGS += $(SANIT)
sanit: LDFLAGS += $(SANIT)
sanit: $(name)

.PHONY: optim
optim: CFLAGS += $(OPTIM)
optim: LDFLAGS += $(OPTIM)
optim: $(name)
	strip --strip-all --strip-unneeded $(name)

$(name): $(objects)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(dir_build)/%.o: $(dir_source)/%.c | $$(dir $$@)
	$(COMPILE.c) -MMD -MF $(@:.o=.d) $(OUTPUT_OPTION) $<

.PRECIOUS: %/
%/:
	mkdir -p $@

-include $(patsubst %.o, %.d, $(objects))
