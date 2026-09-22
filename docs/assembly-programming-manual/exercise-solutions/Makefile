ROOT := ../../..
AS ?= myemulator2-elf-as
LD ?= myemulator2-elf-ld
OBJCOPY ?= myemulator2-elf-objcopy
BUILD ?= build

# Add a solution source to this list only after it has a documented build
# command and a corresponding host-QEMU or guest verification record.
SOLUTION_SOURCES := $(wildcard exercise-*.S)
SOLUTION_NAMES := $(patsubst %.S,%,$(SOLUTION_SOURCES))
SOLUTION_ELFS := $(addprefix $(BUILD)/,$(addsuffix .elf,$(SOLUTION_NAMES)))

.PHONY: all clean list

all: $(SOLUTION_ELFS)

list:
	@printf '%s\n' $(SOLUTION_SOURCES)

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: %.S | $(BUILD)
	$(AS) -o $@ $<

$(BUILD)/%.elf: $(BUILD)/%.o
	$(LD) -o $@ $<

clean:
	rm -rf $(BUILD)

# This Makefile assembles and links reference sources; it does not claim that
# an output executed under REM Linux or on a Fold. Record that separately.
