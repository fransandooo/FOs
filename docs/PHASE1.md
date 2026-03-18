# Phase 1 — "It boots"

Custom bootloader → 64-bit long mode → C kernel printing to VGA + serial.

## Boot Chain

```
BIOS (16-bit real mode)
  │  Loads first 512 bytes (MBR) to 0x7C00
  ▼
Stage 1 — boot/stage1.asm
  │  Loads Stage 2 from disk (int 0x13 AH=0x42, LBA mode)
  │  Jumps to 0x7E00
  ▼
Stage 2 — boot/stage2.asm (16-bit real mode)
  │  1. Loads kernel from disk to 0x10000 (temporary)
  │  2. Checks CPUID for long mode support
  │  3. Enables A20 line (BIOS method, fast gate fallback)
  │  4. Loads 32-bit GDT, sets CR0.PE → protected mode
  ▼
Stage 2 — (32-bit protected mode)
  │  5. Copies kernel from 0x10000 → 0x100000 (1MB mark)
  │  6. Sets up identity-mapped page tables (2MB huge pages)
  │  7. Enables PAE, sets EFER.LME, enables paging → long mode
  │  8. Loads 64-bit GDT, far jumps to 64-bit code
  ▼
Stage 2 — (64-bit long mode)
  │  Reloads segment registers, sets RSP, jumps to 0x100000
  ▼
Kernel entry — kernel/entry.asm
  │  Clears BSS, sets up stack at 0x200000, calls kmain()
  ▼
kmain() — kernel/kmain.c
  │  Initializes serial (COM1) and VGA text mode
  │  Prints "Hello from FOs!"
  ▼
  Halts (hlt loop)
```

## Memory Map During Boot

| Address Range         | Contents                        |
|-----------------------|---------------------------------|
| `0x00000 - 0x004FF`   | BIOS data (IVT, BDA)           |
| `0x01000 - 0x01FFF`   | PML4 (page table level 4)      |
| `0x02000 - 0x02FFF`   | PDPT (page directory ptr table) |
| `0x03000 - 0x03FFF`   | PD (page directory, 2MB pages) |
| `0x04000 - 0x07BFF`   | Free (stack grows ↓ from 0x7C00) |
| `0x07C00 - 0x07DFF`   | Stage 1 (MBR, 512 bytes)       |
| `0x07E00 - 0x09DFF`   | Stage 2 (8KB)                  |
| `0x10000 - 0x17FFF`   | Kernel temp load (32KB max)    |
| `0xB8000 - 0xB8F9F`   | VGA text buffer (80×25×2)      |
| `0x100000+`           | Kernel (final location)        |
| `↓ from 0x200000`     | Kernel stack (grows down)      |

## Disk Image Layout

| Sectors  | Offset   | Contents    |
|----------|----------|-------------|
| 0        | 0x000    | Stage 1 MBR |
| 1–16     | 0x200    | Stage 2     |
| 17+      | 0x2200   | Kernel .bin |

## CPU Mode Transitions

### Real Mode → Protected Mode
1. Disable interrupts (`cli`)
2. Load the 32-bit GDT (`lgdt`)
3. Set `CR0.PE` (bit 0)
4. **Far jump** to flush the pipeline and load CS with new code segment selector

### Protected Mode → Long Mode
1. Copy kernel to high memory (can now address above 1MB)
2. Set up 4-level page tables (PML4 → PDPT → PD, using 2MB huge pages)
3. Load PML4 address into CR3
4. Enable PAE in CR4 (bit 5)
5. Set LME in EFER MSR (bit 8 of MSR 0xC0000080)
6. Enable paging in CR0 (bit 31) — enters compatibility sub-mode
7. Load 64-bit GDT
8. **Far jump** to 64-bit code segment — enters full 64-bit long mode

## Key Design Decisions

- **No cross-compiler needed**: Host is x86_64 Linux targeting x86_64 freestanding. System GCC works with `-ffreestanding -mno-red-zone -fno-stack-protector`.
- **`-mno-red-zone`**: Critical for kernel code. The red zone is a 128-byte area below RSP that interrupts would clobber. Must be disabled.
- **Two-stage bootloader**: 512-byte MBR is too small for mode transitions. Stage 1 just loads Stage 2.
- **Kernel loaded twice**: BIOS int 0x13 only works in real mode (can't address above 1MB easily). Kernel is loaded to 0x10000 first, then copied to 0x100000 in protected mode.
- **2MB huge pages**: Simplifies page table setup (no PT level needed). Two entries cover first 4MB — enough for kernel + VGA buffer.
- **Serial output from day one**: `qemu-system-x86_64 -nographic` shows serial on stdio. Works even when VGA is broken.

## Files

| File | Purpose |
|------|---------|
| `boot/stage1.asm` | MBR bootloader, loads stage2, 512 bytes |
| `boot/stage2.asm` | Mode transitions: real → protected → long, loads kernel |
| `kernel/entry.asm` | Entry stub: clears BSS, calls kmain() |
| `kernel/kmain.c` | Kernel main function |
| `kernel/tty.c/h` | VGA text mode driver (80×25, scrolling) |
| `kernel/serial.c/h` | COM1 serial port driver (38400 8N1) |
| `kernel/string.c/h` | memset, memcpy, strlen (GCC may emit calls to these) |
| `kernel/io.h` | inb/outb inline assembly wrappers |
| `linker.ld` | Places kernel at 0x100000, entry point first |
| `Makefile` | Build system + QEMU run targets |

## Building and Running

```sh
make            # Build fos.img
make run        # Run with GUI + serial on stdio
make run-serial # Run headless (serial only)
make run-debug  # Run with QEMU debug logging (int, cpu_reset)
make debug      # Run paused, waiting for GDB on :1234
make clean      # Remove all build artifacts
```

## Debugging Tips

- **Triple fault (instant reboot)**: Use `make run-debug` to see `cpu_reset` events. Usually a bad far jump or missing GDT reload after mode switch.
- **Serial breadcrumbs**: Stage 1 writes `1`, Stage 2 writes `2`, protected mode writes `P`, long mode writes `L` to COM1. Check how far it gets.
- **GDB**: `make debug`, then in another terminal: `gdb -ex 'target remote :1234' -ex 'set arch i386:x86-64:intel'`. Set breakpoints at `*0x7C00` (stage1), `*0x7E00` (stage2), `*0x100000` (kernel).
