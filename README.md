# FOs

A from-scratch x86-64 operating system built to eventually run DOOM.

## Goal

Build a minimal OS from the ground up, phase by phase, culminating in running [doomgeneric](https://github.com/ozkl/doomgeneric) natively.

## Phases

| Phase | Goal | Status |
|-------|------|--------|
| 1 | Boots in QEMU, prints text, no crash | [ ] |
| 2 | Dynamic memory allocation in kernel space | [ ] |
| 3 | Keyboard input + timer working | [ ] |
| 4 | Framebuffer graphics, pixel drawing | [ ] |
| 5 | Userspace: ELF loader, syscalls, scheduler | [ ] |
| 6 | FAT32 filesystem, read files from disk | [ ] |
| 7 | DOOM runs | [ ] |

## Tools

- **Emulator**: QEMU (x86_64)
- **Toolchain**: x86_64-elf-gcc (cross-compiler)
- **Bootloader**: GRUB with Multiboot2
- **Target arch**: x86-64 (64-bit long mode)

## Resources

- [OSDev Wiki](https://wiki.osdev.org) — primary reference
- [doomgeneric](https://github.com/ozkl/doomgeneric) — portable DOOM port to target
- [Writing an OS in Rust](https://os.phil-opp.com) — concepts transfer to C
- [xv6-riscv](https://github.com/mit-pdos/xv6-riscv) — MIT teaching OS for reference

## Running

```sh
# Build
make

# Run in QEMU
make run
```
