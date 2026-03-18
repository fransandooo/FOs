# Phase 3: Interrupts, Keyboard, Timer

## Overview

Phase 3 adds hardware interrupt handling, making the OS genuinely interactive.
The CPU can now respond to asynchronous hardware events (timer ticks, key
presses) rather than only running sequentially. The milestone is a working
shell prompt where you can type commands and get responses.

## Architecture

```
Hardware device (keyboard, timer)
        |  Sends signal on IRQ line
        v
8259A PIC (Programmable Interrupt Controller)
        |  Translates IRQ -> interrupt vector number
        v
CPU -- looks up vector in IDT
        |
        v
Assembly stub (isr_stubs.asm)
        |  Saves all registers, builds InterruptFrame
        v
C dispatcher (isr.c: interrupt_handler)
        |  Routes to registered handler
        v
Device handler (pit.c / keyboard.c)
        |  Does work, returns
        v
Assembly stub sends EOI to PIC, restores registers, iretq
```

## Components

### PIC Remapping (`kernel/pic.c`)

The two cascaded 8259A PICs (master + slave) handle IRQ0-15. Default BIOS
mapping puts IRQ0-7 at vectors 8-15, which collides with CPU exception
vectors (8 = Double Fault, 13 = GPF). We remap:

- Master (IRQ0-7) -> vectors 32-39
- Slave (IRQ8-15) -> vectors 40-47

After remapping, all IRQs are masked. Drivers unmask individual lines as they
initialize, preventing interrupts from firing before handlers are registered.

The slave PIC's cascade line (IRQ2 on master) is automatically unmasked when
any slave IRQ (8-15) is unmasked.

### IDT (`kernel/idt.c`)

256-entry Interrupt Descriptor Table. Each entry is a 16-byte 64-bit interrupt
gate descriptor pointing to a handler function. All entries use:
- Selector: 0x08 (kernel code segment from GDT)
- Type: 0x8E (present, DPL=0, 64-bit interrupt gate — clears IF on entry)

Vectors registered:
- 0-19: CPU exceptions (with proper error code handling)
- 32-47: Hardware IRQs (after PIC remap)

### Interrupt Stubs (`kernel/isr_stubs.asm`)

Assembly glue between CPU interrupt mechanism and C handlers. Each stub:
1. Pushes a dummy error code (for exceptions that don't push one)
2. Pushes the vector number
3. Jumps to `isr_common`

`isr_common` saves all 15 GPRs, aligns the stack to 16 bytes (x86-64 ABI
requirement), calls `interrupt_handler(InterruptFrame*)`, then restores
everything and executes `iretq`.

Stack alignment: 15 GPRs (120 bytes) + vector + error (16 bytes) = 136 bytes.
136 % 16 = 8, so we `sub rsp, 8` before the call.

### Interrupt Dispatcher (`kernel/isr.c`)

Central C handler that routes interrupts by vector number:
- Vectors 0-19: CPU exceptions -> prints diagnostic (registers, error code,
  faulting address for page faults) and halts. No more silent triple faults.
- Vectors 32-47: Hardware IRQs -> dispatches to registered handler function,
  then sends EOI to PIC.

**Spurious IRQ detection:** IRQ7 (master) and IRQ15 (slave) can fire
spuriously. Before handling these, we read the PIC's In-Service Register
(ISR) to verify the IRQ is real. Spurious IRQs are silently dropped (with
cascade EOI for slave spurious).

**Driver registration:** Drivers call `isr_register_irq(irq, handler)` to
register their handler. This keeps isr.c generic — it doesn't need to know
about specific hardware.

### PIT Timer (`kernel/pit.c`)

Intel 8253/8254 Programmable Interval Timer, configured at 100 Hz (10ms per
tick). Provides:
- `pit_get_ticks()` — monotonic tick counter
- `pit_sleep_ms(ms)` — busy-wait sleep using HLT (CPU sleeps between ticks)

The timer handler simply increments the tick counter. Future phases can use
this for preemptive scheduling.

### PS/2 Keyboard (`kernel/keyboard.c`)

Translates Scancode Set 1 (make/break codes) to ASCII characters. Features:
- Full alphanumeric + symbol mapping (unshifted and shifted)
- Shift key tracking (left/right)
- Caps Lock toggle (interacts correctly with shift for letters)
- 64-byte ring buffer for input
- Non-blocking `keyboard_getchar()` API

### TTY Updates (`kernel/tty.c`)

- **Backspace support:** Moves cursor back, clears character
- **Hardware cursor:** Updates VGA CRT controller cursor position on every
  character write (ports 0x3D4/0x3D5)
- **`tty_clear()`:** Clears screen and resets cursor

### String Updates (`kernel/string.c`)

- Added `strcmp()` for shell command matching

## Shell

The shell is a simple read-eval-print loop:
- Reads characters from keyboard ring buffer
- Echoes typed characters to VGA + serial
- On Enter, parses the line and executes commands

Commands:
- `help` — list available commands
- `ticks` — show tick count and uptime in seconds
- `mem` — show free physical memory
- `clear` — clear the screen

## Initialization Order

This is critical — wrong order = immediate triple fault:

```
1. serial_init()      -- debug output available
2. tty_init()         -- VGA output available
3. pmm_init()         -- physical memory
4. vmm_init()         -- virtual memory
5. heap_init()        -- kernel heap
6. pic_remap()        -- remap PIC (IRQs masked)
7. idt_init()         -- load IDT (handlers registered)
8. pit_init(100)      -- configure timer hardware
9. keyboard_init()    -- register keyboard handler
10. pic_unmask_irq(0) -- enable timer IRQ
11. pic_unmask_irq(1) -- enable keyboard IRQ
12. sti               -- enable CPU interrupts
```

If `sti` happens before the IDT is loaded, the first hardware interrupt
vectors into garbage memory -> triple fault. If `sti` happens before PIC
remap, IRQ0 fires as vector 8 (Double Fault handler) instead of vector 32.

## Improvements Over Plan

1. **Driver registration pattern:** Instead of hardcoding timer/keyboard
   handlers in `interrupt_handler()`, drivers register via
   `isr_register_irq()`. This keeps the dispatcher generic and makes adding
   new IRQ handlers trivial.

2. **Spurious IRQ detection:** IRQ7 and IRQ15 spurious interrupts are
   properly detected by reading the PIC ISR register, preventing phantom
   interrupt handling.

3. **Hardware VGA cursor:** The blinking cursor tracks the current write
   position, making the shell feel more like a real terminal.

4. **Caps Lock interaction with Shift:** Caps Lock only affects letter keys
   and correctly inverts when Shift is held (Shift+CapsLock = lowercase).

5. **All 16 IRQ stubs:** Even unused IRQs (COM, LPT, floppy, ATA) have
   stubs registered, so stray hardware interrupts don't triple fault.

6. **Mask-all-then-unmask PIC init:** After remapping, all IRQs start masked.
   Each driver explicitly unmasks its IRQ, preventing interrupts from firing
   before handlers exist.

7. **Automatic cascade unmask:** Unmasking any slave IRQ (8-15) automatically
   unmasks the cascade line (IRQ2 on master).

## Test Results

```
Timer test: 10 ticks in ~100ms PASS

FOs> help
Commands: help, ticks, mem, clear
FOs> ticks
Ticks: 574 (uptime: 5s)
FOs> mem
Free: 126 MB (32474 pages)
```
