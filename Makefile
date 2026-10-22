# Bare-metal RV32 firmware for qemu-system-riscv32 -machine virt.
#   make            build build/firmware.elf
#   make run        interactive console (Ctrl-A x to quit)
#   make test       host unit tests (ASan/UBSan) + QEMU integration tests
CC      := clang
LLD     ?= $(shell command -v ld.lld || command -v ld.lld-19 || command -v ld.lld-18)
OBJDUMP ?= $(shell command -v llvm-objdump || command -v llvm-objdump-19 || command -v llvm-objdump-18)
QEMU    ?= qemu-system-riscv32
HOSTCC  ?= cc
ARCH    := --target=riscv32-unknown-elf -march=rv32imac_zicsr -mabi=ilp32
CFLAGS  := $(ARCH) -std=c11 -O2 -g -Wall -Wextra -Werror -ffreestanding -fno-builtin \
           -nostdlib -mcmodel=medany -fno-pic -Iinclude
LDFLAGS := $(ARCH) -nostdlib -static -fuse-ld=lld --ld-path=$(LLD) -Wl,-T,link.ld -Wl,--gc-sections
SRCS    := src/start.S src/main.c src/uart.c src/fmt.c src/libc_min.c

.PHONY: all run test host-test qemu-test size clean
all: build/firmware.elf

build/firmware.elf: $(SRCS) link.ld $(wildcard include/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

build/firmware-auto.elf: $(SRCS) link.ld $(wildcard include/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -DAUTO_EXIT $(LDFLAGS) $(SRCS) -o $@

build/test_host: tests/test_host.c src/fmt.c include/fmt.h include/ring.h
	@mkdir -p build
	$(HOSTCC) -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-format -Iinclude \
	  -fsanitize=address,undefined -fno-sanitize-recover=all tests/test_host.c src/fmt.c -o $@

run: build/firmware.elf
	$(QEMU) -machine virt -bios none -nographic -kernel $<

host-test: build/test_host
	./build/test_host

qemu-test: build/firmware.elf build/firmware-auto.elf
	$(QEMU) -machine virt -bios none -display none -serial stdio -monitor none -kernel build/firmware-auto.elf
	python3 tests/run_qemu.py --qemu $(QEMU) --elf build/firmware.elf

test: host-test qemu-test

size: build/firmware.elf
	$(OBJDUMP) -h $< | grep -E 'text|rodata|data|bss'

clean:
	rm -rf build
