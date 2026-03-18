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
           -mno-red-zone -fno-stack-protector -fno-pie -no-pie \
           -fno-exceptions -m64

LDFLAGS  = -nostdlib -T linker.ld
NASMFLAGS_BIN = -f bin
NASMFLAGS_ELF = -f elf64

# Source files
KERNEL_C   = kernel/kmain.c kernel/tty.c kernel/serial.c kernel/string.c
KERNEL_ASM = kernel/entry.asm
KERNEL_OBJ = kernel/entry.o $(KERNEL_C:.c=.o)

# =============================================================================
# Targets
# =============================================================================

.PHONY: all run debug clean

all: fos.img

# --- Bootloader (assembled to raw flat binaries) ---

boot/stage1.bin: boot/stage1.asm
	$(NASM) $(NASMFLAGS_BIN) -o $@ $<

boot/stage2.bin: boot/stage2.asm
	$(NASM) $(NASMFLAGS_BIN) -o $@ $<

# --- Kernel (compiled to ELF, then converted to flat binary) ---

kernel/entry.o: kernel/entry.asm
	$(NASM) $(NASMFLAGS_ELF) -o $@ $<

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel.elf: $(KERNEL_OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJ)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

# --- Disk image ---
# Layout:
#   Sector  0      : Stage 1 (MBR, 512 bytes)
#   Sectors 1-16   : Stage 2 (8KB)
#   Sectors 17+    : Kernel (flat binary)

fos.img: boot/stage1.bin boot/stage2.bin kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=boot/stage1.bin of=$@ conv=notrunc 2>/dev/null
	dd if=boot/stage2.bin of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=kernel.bin of=$@ bs=512 seek=17 conv=notrunc 2>/dev/null
	@echo "==> fos.img created ($(shell wc -c < kernel.bin 2>/dev/null || echo '?') byte kernel)"

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
