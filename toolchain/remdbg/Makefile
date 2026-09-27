# Build a runnable REM ELF with the validated REM GCC + musl tuple.
# Example:
#   make REM_CC=/path/to/myemulator2-elf-gcc \
#        MUSL_PREFIX=/path/to/musl-prefix
REM_CC ?= gcc
MUSL_PREFIX ?= /usr/myemulator2-linux-musl
LINKER_SCRIPT ?= ../userspace/myemulator2-user.ld
CFLAGS ?= -O1 -g
OUTPUT ?= remdbg

.PHONY: all clean
all: $(OUTPUT)

$(OUTPUT): remdbg.c
	$(REM_CC) -isystem "$(MUSL_PREFIX)/include" -std=gnu11 $(CFLAGS) -Wall -Wextra -Werror \
		-nostdlib "$(MUSL_PREFIX)/lib/crt1.o" "$(MUSL_PREFIX)/lib/crti.o" $< \
		"$(MUSL_PREFIX)/lib/crtn.o" -L"$(MUSL_PREFIX)/lib" -lc -lgcc \
		-Wl,-T,"$(LINKER_SCRIPT)" -o $@

clean:
	rm -f $(OUTPUT)
