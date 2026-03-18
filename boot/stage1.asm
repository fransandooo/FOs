; =============================================================================
; FOs Stage 1 Bootloader (MBR)
; =============================================================================
; Loaded by BIOS at 0x7C00 in 16-bit real mode.
; Job: load Stage 2 from disk and jump to it. That's it.
;
; Disk layout:
;   Sector 0     : Stage 1 (this file, 512 bytes)
;   Sectors 1-16 : Stage 2 (8KB)
;   Sectors 17+  : Kernel binary
; =============================================================================

[BITS 16]
[ORG 0x7C00]

start:
    ; BIOS puts boot drive number in DL — preserve it.
    ; Zero segment registers (BIOS doesn't guarantee their state).
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00              ; Stack grows downward from our load address

    ; --- Serial debug: write '1' to COM1 (0x3F8) ---
    ; Save DL (boot drive) before clobbering DX with serial port address
    push dx
    mov dx, 0x3F8
    mov al, '1'
    out dx, al
    pop dx                      ; Restore DL = boot drive number

    ; --- Load Stage 2 from disk using BIOS Extended Read (int 0x13, AH=0x42) ---
    mov ah, 0x42
    mov si, dap
    int 0x13
    jc disk_error

    ; --- Jump to Stage 2 ---
    ; Pass boot drive in DL so Stage 2 can load the kernel.
    jmp 0x0000:0x7E00

disk_error:
    ; Print 'E' via BIOS teletype and halt
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    ; Also write to serial for headless debugging
    mov dx, 0x3F8
    mov al, '!'
    out dx, al

halt:
    cli
    hlt
    jmp halt

; =============================================================================
; Disk Address Packet (DAP) for int 0x13 AH=0x42
; =============================================================================
dap:
    db 0x10                     ; DAP size (16 bytes)
    db 0x00                     ; Reserved
    dw 16                       ; Number of sectors to read (16 * 512 = 8KB)
    dw 0x7E00                   ; Target offset
    dw 0x0000                   ; Target segment (0x0000:0x7E00 = phys 0x7E00)
    dq 1                        ; Start LBA (sector 1, right after MBR)

; =============================================================================
; Pad to 510 bytes and add boot signature
; =============================================================================
times 510 - ($ - $$) db 0
dw 0xAA55
