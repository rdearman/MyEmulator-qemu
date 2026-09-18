SHELL := /bin/bash

PROJECT_ROOT := $(CURDIR)
QEMU_SOURCE ?= $(PROJECT_ROOT)/.qemu-upstream
QEMU_BUILD ?= $(PROJECT_ROOT)/.qemu-build
QEMU_BINARY ?= $(QEMU_BUILD)/qemu-system-myemulator
QEMU32_BINARY ?= $(QEMU_BUILD)/qemu-system-myemulator32
JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')

.PHONY: all build build32 qemu test check cpu-test cpu32-test debug-test device-test \
	assembler-test emacs-test firmware-test spec-test myfs-demo myfs-test \
	toolchain-build toolchain-test clean help

all: build

build qemu:
	QEMU_SOURCE="$(QEMU_SOURCE)" QEMU_BUILD="$(QEMU_BUILD)" JOBS="$(JOBS)" \
		$(PROJECT_ROOT)/qemu/tools/build-myemulator.sh

build32: build

cpu32-test: build
	QEMU_MYEMULATOR32="$(QEMU32_BINARY)" $(PROJECT_ROOT)/tests/run-myemulator32-cpu-tests.sh
	QEMU_MYEMULATOR32="$(QEMU32_BINARY)" python3 $(PROJECT_ROOT)/tests/run-myemulator32-integer-tests.py
	QEMU_MYEMULATOR32="$(QEMU32_BINARY)" python3 $(PROJECT_ROOT)/tests/run-myemulator32-system-tests.py

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
	QEMU_MYEMULATOR="$(QEMU_BINARY)" $(PROJECT_ROOT)/tests/run-myemulator-timer-test.sh

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

spec-test:
	python3 $(PROJECT_ROOT)/tools/test_myemulator2_spec.py

toolchain-build:
	$(PROJECT_ROOT)/toolchain/scripts/build-binutils.sh

toolchain-test:
	$(PROJECT_ROOT)/toolchain/scripts/test-binutils.sh

MYFS_BUILD ?= $(PROJECT_ROOT)/build/myfs
MYFS_IMAGE ?= $(MYFS_BUILD)/myemulator.img

myfs-demo: build
	mkdir -p "$(MYFS_BUILD)"
	$(PROJECT_ROOT)/tools/myasm $(PROJECT_ROOT)/myfs/boot.s -o "$(MYFS_BUILD)/boot.full.bin" --flat-64k
	dd if="$(MYFS_BUILD)/boot.full.bin" of="$(MYFS_BUILD)/boot.bin" bs=1 skip=512 count=256 status=none
	$(PROJECT_ROOT)/tools/mkmyfs-kernel "$(MYFS_BUILD)/boot.bin" "$(MYFS_BUILD)/kernel.bin"
	$(PROJECT_ROOT)/tools/myasm $(PROJECT_ROOT)/myfs/command.s -o "$(MYFS_BUILD)/command.full.bin" --flat-64k
	$(PROJECT_ROOT)/tools/extract-binary "$(MYFS_BUILD)/command.full.bin" "$(MYFS_BUILD)/COMMAND.COM" --start 0x400
	$(PROJECT_ROOT)/tools/myasm $(PROJECT_ROOT)/myfs/hello.s -o "$(MYFS_BUILD)/hello.full.bin" --flat-64k
	$(PROJECT_ROOT)/tools/extract-binary "$(MYFS_BUILD)/hello.full.bin" "$(MYFS_BUILD)/HELLO.COM" --start 0x2000
	$(PROJECT_ROOT)/tools/mkmyfs --boot "$(MYFS_BUILD)/boot.bin" --output "$(MYFS_IMAGE)" \
		--file COMMAND.COM="$(MYFS_BUILD)/COMMAND.COM" \
		--file HELLO.COM="$(MYFS_BUILD)/HELLO.COM" \
		--file README.TXT="$(PROJECT_ROOT)/myfs/README.TXT"
	@echo "Built $(MYFS_IMAGE)"

myfs-test: myfs-demo
	python3 $(PROJECT_ROOT)/tools/test_mkmyfs.py
	QEMU_MYEMULATOR="$(QEMU_BINARY)" $(PROJECT_ROOT)/tests/run-myfs-e2e-test.sh

test check: assembler-test cpu-test cpu32-test debug-test device-test firmware-test emacs-test spec-test myfs-test

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
	@echo '  make spec-test      validate the MyEmulator 2.0 design manifest'
	@echo '  make toolchain-build build GNU binutils 2.46.0 for MyEmulator2'
	@echo '  make toolchain-test  run GNU binutils and QEMU ELF integration tests'
	@echo '  make cpu32-test     run the MyEmulator 2.0 executable CPU tests'
	@echo '  make myfs-demo      build boot sector, COMMAND.COM, HELLO.COM, and image'
	@echo '  make myfs-test      run MyFS host-builder tests'
	@echo '  make clean          remove only .qemu-build/'
	@echo ''
	@echo 'Override QEMU_SOURCE, QEMU_BUILD, QEMU_BINARY, or JOBS as needed.'
