SHELL := /bin/bash

PROJECT_ROOT := $(CURDIR)
QEMU_SOURCE ?= $(PROJECT_ROOT)/.qemu-upstream
QEMU_BUILD ?= $(PROJECT_ROOT)/.qemu-build
QEMU_BINARY ?= $(QEMU_BUILD)/qemu-system-myemulator
JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')

.PHONY: all build qemu test check cpu-test debug-test device-test \
	assembler-test emacs-test firmware-test clean help

all: build

build qemu:
	QEMU_SOURCE="$(QEMU_SOURCE)" QEMU_BUILD="$(QEMU_BUILD)" JOBS="$(JOBS)" \
		$(PROJECT_ROOT)/qemu/tools/build-myemulator.sh

cpu-test: build
	QEMU_MYEMULATOR="$(QEMU_BINARY)" $(PROJECT_ROOT)/tests/run-qemu-cpu-tests.sh

debug-test: build
	python3 $(PROJECT_ROOT)/tools/test_mydebug_paths.py
	QEMU_MYEMULATOR="$(QEMU_BINARY)" $(PROJECT_ROOT)/tests/run-qemu-debug-tests.sh
	$(PROJECT_ROOT)/tests/run-mydebug-tests.sh
	$(PROJECT_ROOT)/tests/run-mydebug-halt-test.sh

device-test: build
	QEMU_MYEMULATOR="$(QEMU_BINARY)" $(PROJECT_ROOT)/tests/run-myemulator-console-test.sh
	$(PROJECT_ROOT)/tests/run-myemulator-floppy-test.sh

firmware-test: build
	QEMU_MYEMULATOR="$(QEMU_BINARY)" $(PROJECT_ROOT)/tests/run-myemulator-firmware-test.sh
	QEMU_MYEMULATOR="$(QEMU_BINARY)" $(PROJECT_ROOT)/tests/run-myemulator-alignment-test.sh

assembler-test: build
	python3 $(PROJECT_ROOT)/tools/assembler/test_myasm.py
	python3 $(PROJECT_ROOT)/tools/test_format_asm.py
	QEMU_MYEMULATOR="$(QEMU_BINARY)" $(PROJECT_ROOT)/tests/run-assembler-qemu-test.sh

emacs-test:
	emacs --batch -Q -L $(PROJECT_ROOT)/tools/emacs \
		-l $(PROJECT_ROOT)/tools/emacs/mydebug-tests.el \
		-f ert-run-tests-batch-and-exit

test check: assembler-test cpu-test debug-test device-test firmware-test emacs-test

# Remove only the generated QEMU build tree. The upstream checkout is retained
# so the next build does not need to clone it again.
clean:
	rm -rf "$(QEMU_BUILD)"

help:
	@echo 'MyEmulator build targets:'
	@echo '  make build          clone/configure/build qemu-system-myemulator'
	@echo '  make test           run assembler, CPU, device, debugger, and Emacs tests'
	@echo '  make cpu-test       run the CPU and interrupt tests'
	@echo '  make debug-test     run disassembler and native debugger tests'
	@echo '  make device-test    run floppy and console tests'
	@echo '  make firmware-test  run firmware ROM and memory-map tests'
	@echo '  make assembler-test run assembler tests and assembler/QEMU integration'
	@echo '  make emacs-test     run Emacs ERT tests'
	@echo '  make clean          remove only .qemu-build/'
	@echo ''
	@echo 'Override QEMU_SOURCE, QEMU_BUILD, QEMU_BINARY, or JOBS as needed.'
