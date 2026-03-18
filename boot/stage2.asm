; =============================================================================
; FOs Stage 2 Bootloader
; =============================================================================
; Loaded at 0x7E00 in 16-bit real mode by Stage 1.
;
; Responsibilities (in order):
;   1. Load kernel from disk to temporary address (0x10000)
;   2. Enable the A20 line (access memory above 1MB)
;   3. Check that CPU supports long mode (CPUID)
;   4. Switch to 32-bit protected mode
;   5. Copy kernel to final address (0x100000 = 1MB mark)
;   6. Set up identity-mapped page tables
;   7. Switch to 64-bit long mode
;   8. Jump to kernel
;
; Memory map during boot:
;   0x00000 - 0x004FF : BIOS data area (IVT, BDA)
;   0x00500 - 0x00FFF : Free
;   0x01000 - 0x01FFF : PML4 (page table level 4)
;   0x02000 - 0x02FFF : PDPT (page directory pointer table)
;   0x03000 - 0x03FFF : PD   (page directory)
;   0x04000 - 0x07BFF : Free (stack grows down from 0x7C00)
;   0x07C00 - 0x07DFF : Stage 1 (MBR)
;   0x07E00 - 0x09FFF : Stage 2 (this code)
;   0x10000 - 0x1FFFF : Kernel (temporary load address)
;   0xB8000 - 0xB8FFF : VGA text buffer
;   0x100000+         : Kernel (final address after copy)
; =============================================================================

[BITS 16]
[ORG 0x7E00]

stage2_entry:
    ; Save boot drive number (passed in DL from Stage 1)
    mov [boot_drive], dl

    ; Serial debug: Stage 2 reached
    mov dx, 0x3F8
    mov al, '2'
    out dx, al

    ; Print banner via BIOS
    mov si, msg_stage2
    call print16

    ; =====================================================================
    ; Step 1: Load kernel from disk
    ; =====================================================================
    mov si, msg_loading
    call print16

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, kernel_dap
    int 0x13
    jc load_failed

    mov si, msg_ok
    call print16
    jmp check_long_mode

load_failed:
    mov si, msg_disk_err
    call print16
    jmp halt16

    ; =====================================================================
    ; Step 2: Verify long mode support via CPUID
    ; =====================================================================
check_long_mode:
    ; First check if CPUID instruction is available
    ; (toggle bit 21 of EFLAGS — if it changes, CPUID is supported)
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd                       ; Restore original EFLAGS
    cmp eax, ecx
    je no_cpuid

    ; Check if extended CPUID functions exist
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb no_long_mode

    ; Check long mode bit (bit 29 of EDX from CPUID 0x80000001)
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz no_long_mode

    mov si, msg_lm_ok
    call print16
    jmp enable_a20

no_cpuid:
    mov si, msg_no_cpuid
    call print16
    jmp halt16

no_long_mode:
    mov si, msg_no_lm
    call print16
    jmp halt16

    ; =====================================================================
    ; Step 3: Enable A20 line
    ; =====================================================================
enable_a20:
    ; Try BIOS method first (int 0x15, AX=0x2401)
    mov ax, 0x2401
    int 0x15
    jc a20_fast_method
    jmp a20_done

a20_fast_method:
    ; Fallback: Fast A20 gate via port 0x92
    in al, 0x92
    or al, 2                    ; Set A20 gate bit
    and al, 0xFE                ; Don't accidentally reset the CPU (bit 0)
    out 0x92, al

a20_done:
    mov si, msg_a20_ok
    call print16

    ; =====================================================================
    ; Step 4: Enter 32-bit protected mode
    ; =====================================================================
    cli                         ; Disable interrupts for mode switch
    lgdt [gdt32_ptr]            ; Load 32-bit GDT

    mov eax, cr0
    or eax, 1                  ; Set PE (Protection Enable) bit
    mov cr0, eax

    ; Far jump to flush CPU pipeline and load CS with code segment selector
    jmp 0x08:protected_mode_entry

; =============================================================================
; 16-bit helper functions
; =============================================================================

; Print null-terminated string at DS:SI via BIOS teletype
print16:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp .loop
.done:
    popa
    ret

halt16:
    cli
    hlt
    jmp halt16

; =============================================================================
; 16-bit data
; =============================================================================
boot_drive:     db 0

; DAP for loading kernel (64 sectors = 32KB at LBA 17)
kernel_dap:
    db 0x10                     ; DAP size
    db 0x00                     ; Reserved
    dw 64                       ; Sectors to read (32KB)
    dw 0x0000                   ; Offset  (0x1000:0x0000 = physical 0x10000)
    dw 0x1000                   ; Segment
    dq 17                       ; Start LBA (sectors 17+)

; Messages
msg_stage2:     db "[FOs] Stage 2 bootloader", 13, 10, 0
msg_loading:    db "  Loading kernel... ", 0
msg_ok:         db "OK", 13, 10, 0
msg_disk_err:   db "DISK ERROR", 13, 10, 0
msg_no_cpuid:   db "ERROR: No CPUID support", 13, 10, 0
msg_no_lm:      db "ERROR: CPU lacks long mode", 13, 10, 0
msg_lm_ok:      db "  Long mode: supported", 13, 10, 0
msg_a20_ok:     db "  A20 line: enabled", 13, 10, 0

; =============================================================================
; 32-bit GDT (temporary, used only for the real→protected transition)
; =============================================================================
align 16
gdt32:
    ; Null descriptor (required, index 0x00)
    dq 0
    ; Code segment descriptor (index 0x08)
    ;   Base=0, Limit=4GB, 32-bit, ring 0, executable, readable
    dw 0xFFFF                   ; Limit 0:15
    dw 0x0000                   ; Base 0:15
    db 0x00                     ; Base 16:23
    db 0x9A                     ; Access: P=1 DPL=00 S=1 E=1 DC=0 RW=1 A=0
    db 0xCF                     ; Flags: G=1 D=1 L=0 | Limit 16:19 = 0xF
    db 0x00                     ; Base 24:31
    ; Data segment descriptor (index 0x10)
    ;   Base=0, Limit=4GB, 32-bit, ring 0, writable
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92                     ; Access: P=1 DPL=00 S=1 E=0 DC=0 RW=1 A=0
    db 0xCF
    db 0x00
gdt32_end:

gdt32_ptr:
    dw gdt32_end - gdt32 - 1   ; GDT limit (size - 1)
    dd gdt32                    ; GDT base address

; =============================================================================
; 32-bit Protected Mode
; =============================================================================
[BITS 32]
protected_mode_entry:
    ; Reload all data segment registers with the data segment selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7C00             ; Stack below our bootloader

    ; Serial debug: protected mode reached
    mov dx, 0x3F8
    mov al, 'P'
    out dx, al

    ; =====================================================================
    ; Step 5: Copy kernel from 0x10000 to 0x100000 (1MB)
    ; =====================================================================
    cld
    mov esi, 0x10000            ; Source: temporary load address
    mov edi, 0x100000           ; Destination: 1MB mark
    mov ecx, 32768 / 4         ; 32KB / 4 = 8192 dwords
    rep movsd

    ; =====================================================================
    ; Step 6: Set up identity-mapped page tables
    ; =====================================================================
    ; We need 4-level paging for long mode:
    ;   PML4 (at 0x1000) → PDPT (at 0x2000) → PD (at 0x3000)
    ; Using 2MB huge pages in PD to avoid needing a PT level.
    ; Maps first 4MB: enough for kernel (at 1MB) and VGA (at 0xB8000).

    ; Clear all 3 page table pages (12KB total)
    mov edi, 0x1000
    xor eax, eax
    mov ecx, (3 * 4096) / 4
    rep stosd

    ; PML4[0] → PDPT at 0x2000 (present + writable)
    mov dword [0x1000], 0x2003
    mov dword [0x1004], 0x00000000

    ; PDPT[0] → PD at 0x3000 (present + writable)
    mov dword [0x2000], 0x3003
    mov dword [0x2004], 0x00000000

    ; PD[0] → 2MB huge page at physical 0x000000
    ;   Flags: Present(0) + Writable(1) + PS/HugePages(7) = 0x83
    mov dword [0x3000], 0x000083
    mov dword [0x3004], 0x00000000

    ; PD[1] → 2MB huge page at physical 0x200000
    mov dword [0x3008], 0x200083
    mov dword [0x300C], 0x00000000

    ; =====================================================================
    ; Step 7: Enable long mode and paging
    ; =====================================================================

    ; Load PML4 base address into CR3
    mov eax, 0x1000
    mov cr3, eax

    ; Enable PAE (Physical Address Extension) — required for long mode
    mov eax, cr4
    or eax, (1 << 5)           ; CR4.PAE
    mov cr4, eax

    ; Enable Long Mode in the EFER MSR
    mov ecx, 0xC0000080         ; EFER MSR address
    rdmsr
    or eax, (1 << 8)           ; EFER.LME (Long Mode Enable)
    wrmsr

    ; Enable paging (CR0.PG) — this activates long mode (compatibility sub-mode)
    mov eax, cr0
    or eax, (1 << 31)          ; CR0.PG
    mov cr0, eax

    ; Load 64-bit GDT
    lgdt [gdt64_ptr]

    ; Far jump to 64-bit code — this transitions from compatibility to full 64-bit
    jmp 0x08:long_mode_entry

; =============================================================================
; 64-bit GDT (the real GDT, used from here on)
; =============================================================================
align 16
gdt64:
    ; Null descriptor (index 0x00)
    dq 0
    ; 64-bit code segment (index 0x08)
    ;   L=1 (long mode), D=0 (required when L=1)
    dw 0xFFFF                   ; Limit 0:15
    dw 0x0000                   ; Base 0:15
    db 0x00                     ; Base 16:23
    db 0x9A                     ; Access: P=1 DPL=00 S=1 E=1 DC=0 RW=1 A=0
    db 0xAF                     ; Flags: G=1 D=0 L=1 | Limit 16:19 = 0xF
    db 0x00                     ; Base 24:31
    ; 64-bit data segment (index 0x10)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92                     ; Access: P=1 DPL=00 S=1 E=0 DC=0 RW=1 A=0
    db 0xCF                     ; Flags: G=1 D=1 L=0 | Limit 16:19 = 0xF
    db 0x00
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1   ; Limit
    dd gdt64                    ; Base (32-bit, fine since we're in low memory)
    dd 0                        ; Upper 32 bits of base (for completeness)

; =============================================================================
; 64-bit Long Mode
; =============================================================================
[BITS 64]
long_mode_entry:
    ; Reload segment registers with 64-bit data segment selector
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up stack
    mov rsp, 0x200000           ; 2MB mark, stack grows down

    ; Serial debug: long mode reached
    mov dx, 0x3F8
    mov al, 'L'
    out dx, al
    mov al, 10                  ; newline
    out dx, al

    ; =====================================================================
    ; Step 8: Jump to kernel at 0x100000
    ; =====================================================================
    mov rax, 0x100000
    jmp rax

; Pad stage2 to exactly 8KB (16 sectors) so disk layout is predictable
times 8192 - ($ - $$) db 0
