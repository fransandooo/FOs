# =============================================================================
# FOs Makefile
# =============================================================================
# Since the host is x86_64 Linux and the target is x86_64 freestanding,
# we can use the system GCC with freestanding flags (no cross-compiler needed).
#
# If you later need a cross-compiler: https://wiki.osdev.org/GCC_Cross-Compiler
# =============================================================================

CC       = gcc
LD       = ld
OBJCOPY  = objcopy
NASM     = nasm

# -ffreestanding    : No standard library, no hosted assumptions
# -mno-red-zone     : CRITICAL for kernel code — interrupts clobber the red zone
# -fno-stack-protector : No stack canary (requires runtime support we don't have)
# -fno-pie -no-pie  : Absolute addresses, not position-independent
# -fno-exceptions   : No C++ exceptions
CFLAGS   = -ffreestanding -O2 -Wall -Wextra -Werror \
           -mno-red-zone -mno-sse -mno-sse2 -mno-mmx \
           -fno-stack-protector -fno-pie -no-pie \
           -fno-exceptions -m64

LDFLAGS  = -nostdlib -T linker.ld
NASMFLAGS_BIN = -f bin
NASMFLAGS_ELF = -f elf64

# Maximum kernel size the bootloader can handle (must match stage2.asm)
KERNEL_MAX_SECTORS = 128
KERNEL_MAX_BYTES   = $(shell echo $$(($(KERNEL_MAX_SECTORS) * 512)))

# Source files
KERNEL_C   = kernel/kmain.c kernel/tty.c kernel/serial.c kernel/string.c \
             kernel/kprintf.c kernel/pmm.c kernel/vmm.c kernel/heap.c \
             kernel/pic.c kernel/idt.c kernel/isr.c kernel/pit.c \
             kernel/keyboard.c
KERNEL_ASM = kernel/entry.asm kernel/isr_stubs.asm
KERNEL_OBJ = kernel/entry.o kernel/isr_stubs.o $(KERNEL_C:.c=.o)

# =============================================================================
# Targets
# =============================================================================

.PHONY: all run run-serial run-debug debug clean

all: fos.img

# --- Bootloader (assembled to raw flat binaries) ---

boot/stage1.bin: boot/stage1.asm
	$(NASM) $(NASMFLAGS_BIN) -o $@ $<

boot/stage2.bin: boot/stage2.asm
	$(NASM) $(NASMFLAGS_BIN) -o $@ $<

# --- Kernel (compiled to ELF, then converted to flat binary) ---

kernel/entry.o: kernel/entry.asm
	$(NASM) $(NASMFLAGS_ELF) -o $@ $<

kernel/isr_stubs.o: kernel/isr_stubs.asm
	$(NASM) $(NASMFLAGS_ELF) -o $@ $<

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel.elf: $(KERNEL_OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJ)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@
	@KSIZE=$$(wc -c < $@); \
	if [ $$KSIZE -gt $(KERNEL_MAX_BYTES) ]; then \
		echo "ERROR: kernel.bin ($$KSIZE bytes) exceeds bootloader limit ($(KERNEL_MAX_BYTES) bytes / $(KERNEL_MAX_SECTORS) sectors)"; \
		echo "       Increase KERNEL_LOAD_SECTORS in stage2.asm and KERNEL_MAX_SECTORS in Makefile"; \
		rm -f $@; \
		exit 1; \
	fi
	@echo "  kernel.bin: $$(wc -c < $@) / $(KERNEL_MAX_BYTES) bytes"

# --- Disk image ---
# Layout:
#   Sector  0      : Stage 1 (MBR, 512 bytes)
#   Sectors 1-16   : Stage 2 (8KB)
#   Sectors 17+    : Kernel (flat binary, up to KERNEL_MAX_SECTORS sectors)

fos.img: boot/stage1.bin boot/stage2.bin kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=boot/stage1.bin of=$@ conv=notrunc 2>/dev/null
	dd if=boot/stage2.bin of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=kernel.bin of=$@ bs=512 seek=17 conv=notrunc 2>/dev/null
	@echo "==> fos.img ready"

# --- Run in QEMU ---

run: fos.img
	qemu-system-x86_64 -drive format=raw,file=fos.img -serial stdio -display gtk

# Run headless (serial output only, no GUI window)
run-serial: fos.img
	qemu-system-x86_64 -drive format=raw,file=fos.img -nographic

# Run with debug flags (shows triple faults, CPU resets)
run-debug: fos.img
	qemu-system-x86_64 -drive format=raw,file=fos.img -serial stdio -d int,cpu_reset

# GDB debug session
debug: fos.img
	@echo "Starting QEMU (paused, waiting for GDB on :1234)..."
	@echo "In another terminal: gdb -ex 'target remote :1234' -ex 'set arch i386:x86-64:intel'"
	qemu-system-x86_64 -drive format=raw,file=fos.img -serial stdio -s -S

clean:
	rm -f boot/*.bin kernel/*.o kernel.elf kernel.bin fos.img
