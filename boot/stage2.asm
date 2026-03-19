; =============================================================================
; FOs Stage 2 Bootloader
; =============================================================================
; Loaded at 0x7E00 in 16-bit real mode by Stage 1.
;
; Responsibilities (in order):
;   1. Load kernel from disk to temporary address (0x10000)
;   2. Detect memory map via E820 BIOS call
;   3. Check that CPU supports long mode (CPUID)
;   4. Enable the A20 line (access memory above 1MB)
;   5. Set VBE graphics mode (linear framebuffer)
;   6. Switch to 32-bit protected mode
;   7. Copy kernel to final address (0x100000 = 1MB mark)
;   8. Set up identity-mapped page tables
;   9. Switch to 64-bit long mode
;  10. Jump to kernel
;
; Memory map during boot:
;   0x00000 - 0x004FF : BIOS data area (IVT, BDA)
;   0x00500 - 0x00FFF : Free
;   0x01000 - 0x01FFF : PML4 (page table level 4)
;   0x02000 - 0x02FFF : PDPT (page directory pointer table)
;   0x03000 - 0x03FFF : PD   (page directory)
;   0x04000 - 0x04FFF : Free
;   0x04FF0 - 0x04FF7 : E820 entry count (uint16_t at 0x4FF8)
;   0x05000 - 0x05FFF : E820 memory map entries (24 bytes each)
;   0x05FF0 - 0x05FF1 : VBE magic word (0x1EAF = success)
;   0x06000 - 0x060FF : VBE mode info block (256 bytes)
;   0x07000 - 0x071FF : VBE controller info (temp, 512 bytes)
;   0x07C00 - 0x07DFF : Stage 1 (MBR)
;   0x07E00 - 0x09DFF : Stage 2 (this code, 8KB)
;   0x10000 - 0x1FFFF : Kernel (temporary load, up to 64KB)
;   0xB8000 - 0xB8FFF : VGA text buffer
;   0x100000+         : Kernel (final address after copy)
; =============================================================================

[BITS 16]
[ORG 0x7E00]

; =============================================================================
; Constants
; =============================================================================
KERNEL_LOAD_SEG     equ 0x1000      ; Segment for temp kernel load (phys 0x10000)
KERNEL_LOAD_SECTORS equ 128         ; Max sectors to load (128 * 512 = 64KB)
KERNEL_DISK_LBA     equ 17          ; Kernel starts at sector 17
KERNEL_FINAL_ADDR   equ 0x100000    ; Final kernel address (1MB)
KERNEL_COPY_DWORDS  equ (KERNEL_LOAD_SECTORS * 512) / 4

E820_MAP_ADDR       equ 0x5000      ; Where E820 entries are stored
E820_COUNT_ADDR     equ 0x4FF8      ; Where E820 entry count is stored
E820_MAGIC          equ 0x534D4150  ; 'SMAP'

VBE_INFO_ADDR       equ 0x6000      ; Where VBE mode info is stored (256 bytes)
VBE_MAGIC_ADDR      equ 0x5FF0      ; Magic word: 0x1EAF = VBE success
VBE_CTRL_ADDR       equ 0x7000      ; Temp buffer for VBE controller info

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
    jmp detect_e820

load_failed:
    mov si, msg_disk_err
    call print16
    jmp halt16

    ; =====================================================================
    ; Step 2: Detect memory map via E820
    ; =====================================================================
    ; Must happen in real mode (uses BIOS int 0x15).
    ; Stores entries at E820_MAP_ADDR, count at E820_COUNT_ADDR.
    ; Each entry: base(8) + length(8) + type(4) + acpi_attrs(4) = 24 bytes
    ; =====================================================================
detect_e820:
    mov si, msg_e820
    call print16

    xor ebx, ebx                ; Continuation value (0 = first call)
    mov di, E820_MAP_ADDR       ; ES:DI → buffer for entries
    xor bp, bp                  ; Entry count

.e820_loop:
    mov eax, 0xE820
    mov ecx, 24                 ; Request 24-byte entries (ACPI 3.0)
    mov edx, E820_MAGIC         ; 'SMAP' signature
    int 0x15

    jc .e820_done               ; Carry = error or end of list
    cmp eax, E820_MAGIC         ; EAX must echo 'SMAP' on success
    jne .e820_fail

    cmp ecx, 20                 ; Entry must be at least 20 bytes
    jl .e820_skip

    inc bp                      ; Count this valid entry
    add di, 24                  ; Advance to next slot

.e820_skip:
    test ebx, ebx              ; EBX = 0 means this was the last entry
    jz .e820_done
    jmp .e820_loop

.e820_fail:
    mov si, msg_e820_fail
    call print16
    jmp halt16

.e820_done:
    test bp, bp                 ; Did we get any entries?
    jz .e820_fail               ; No entries = failure

    mov [E820_COUNT_ADDR], bp   ; Store entry count
    mov si, msg_ok
    call print16
    jmp check_long_mode

    ; =====================================================================
    ; Step 3: Verify long mode support via CPUID
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
    ; Step 4: Enable A20 line
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
    ; Step 5: Set VBE graphics mode
    ; =====================================================================
    ; Must happen in real mode (uses BIOS int 0x10).
    ; We query mode info first (to get framebuffer address), then set mode.
    ; Done LAST before protected mode so all BIOS text output is finished.
    ;
    ; Try modes in preference order: 1024x768, 800x600, 640x480.
    ; We check each mode's info for 32bpp + linear framebuffer support.
    ; =====================================================================
vbe_setup:
    mov si, msg_vbe
    call print16

    ; Default: VBE failed
    mov word [VBE_MAGIC_ADDR], 0x0000

    ; Ensure ES=0 for VBE calls (ES:DI buffer addressing)
    xor ax, ax
    mov es, ax

    ; Get VBE controller info (to find the supported mode list)
    mov dword [VBE_CTRL_ADDR], 'VBE2'   ; Request VBE 2.0+ info
    mov ax, 0x4F00
    mov di, VBE_CTRL_ADDR
    int 0x10
    cmp ax, 0x004F
    jne .vbe_failed

    ; Read mode list pointer (far pointer at offset 14 in controller info)
    ; Format: offset (16-bit) at +14, segment (16-bit) at +16
    mov si, [VBE_CTRL_ADDR + 14]    ; Mode list offset
    mov ax, [VBE_CTRL_ADDR + 16]    ; Mode list segment
    mov fs, ax                       ; FS:SI = mode list

    ; Best mode tracking: prefer highest resolution at 32bpp
    mov word [.vbe_best], 0xFFFF     ; No best mode yet
    mov dword [.vbe_best_pixels], 0  ; Best resolution = 0 pixels

.vbe_scan:
    mov cx, [fs:si]                  ; Read mode number
    cmp cx, 0xFFFF
    je .vbe_scan_done                ; End of list
    add si, 2                        ; Advance to next mode

    mov [.vbe_current], cx

    ; Get mode info
    push si
    push fs
    xor ax, ax
    mov es, ax
    mov di, VBE_INFO_ADDR
    mov ax, 0x4F01
    int 0x10
    pop fs
    pop si

    cmp ax, 0x004F
    jne .vbe_scan                    ; Skip invalid modes

    ; Check: must have linear framebuffer (attribute bit 7)
    mov ax, [VBE_INFO_ADDR]
    test ax, (1 << 7)
    jz .vbe_scan

    ; Check: must be 32bpp
    mov al, [VBE_INFO_ADDR + 25]
    cmp al, 32
    jne .vbe_scan

    ; Check: width must be >= 640 and <= 1920
    mov ax, [VBE_INFO_ADDR + 18]     ; width
    cmp ax, 640
    jb .vbe_scan
    cmp ax, 1920
    ja .vbe_scan

    ; Check: height must be >= 480 and <= 1200
    mov bx, [VBE_INFO_ADDR + 20]     ; height
    cmp bx, 480
    jb .vbe_scan
    cmp bx, 1200
    ja .vbe_scan

    ; Calculate pixels (width * height) — compare with best
    ; Use 32-bit multiply: AX * BX -> DX:AX
    mul bx                           ; DX:AX = width * height
    ; Compare with best (simple: just compare AX if DX==0 for < 64K*64K)
    cmp eax, [.vbe_best_pixels]
    jbe .vbe_scan                    ; Not better than current best

    ; This is our new best mode
    mov [.vbe_best_pixels], eax
    mov cx, [.vbe_current]
    mov [.vbe_best], cx
    jmp .vbe_scan

.vbe_scan_done:
    ; Do we have a valid mode?
    cmp word [.vbe_best], 0xFFFF
    je .vbe_failed

    ; Re-query mode info for the best mode (need it in buffer for kernel)
    xor ax, ax
    mov es, ax
    mov cx, [.vbe_best]
    mov di, VBE_INFO_ADDR
    mov ax, 0x4F01
    int 0x10

    ; Set the mode with LFB bit
    mov bx, [.vbe_best]
    or bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .vbe_failed

    ; VBE success!
    mov word [VBE_MAGIC_ADDR], 0x1EAF
    jmp .vbe_done

.vbe_current:     dw 0
.vbe_best:        dw 0xFFFF
.vbe_best_pixels: dd 0

.vbe_failed:
    ; Stay in text mode — kernel will fall back to VGA text

.vbe_done:

    ; =====================================================================
    ; Step 6: Enter 32-bit protected mode
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

; DAP for loading kernel (128 sectors = 64KB at LBA 17)
kernel_dap:
    db 0x10                     ; DAP size
    db 0x00                     ; Reserved
    dw KERNEL_LOAD_SECTORS      ; Sectors to read
    dw 0x0000                   ; Offset  (KERNEL_LOAD_SEG:0x0000 = physical 0x10000)
    dw KERNEL_LOAD_SEG          ; Segment
    dq KERNEL_DISK_LBA          ; Start LBA

; Messages
msg_stage2:     db "[FOs] Stage 2 bootloader", 13, 10, 0
msg_loading:    db "  Loading kernel... ", 0
msg_ok:         db "OK", 13, 10, 0
msg_disk_err:   db "DISK ERROR", 13, 10, 0
msg_e820:       db "  E820 memory map... ", 0
msg_e820_fail:  db "E820 FAILED", 13, 10, 0
msg_no_cpuid:   db "ERROR: No CPUID support", 13, 10, 0
msg_no_lm:      db "ERROR: CPU lacks long mode", 13, 10, 0
msg_lm_ok:      db "  Long mode: supported", 13, 10, 0
msg_a20_ok:     db "  A20 line: enabled", 13, 10, 0
msg_vbe:        db "  VBE graphics... ", 0

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
    ; Step 7: Copy kernel from 0x10000 to 0x100000 (1MB)
    ; =====================================================================
    cld
    mov esi, 0x10000            ; Source: temporary load address
    mov edi, KERNEL_FINAL_ADDR  ; Destination: 1MB mark
    mov ecx, KERNEL_COPY_DWORDS
    rep movsd

    ; =====================================================================
    ; Step 8: Set up identity-mapped page tables
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
    ; Step 9: Enable long mode and paging
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
    ; Step 10: Jump to kernel at 0x100000
    ; =====================================================================
    mov rax, KERNEL_FINAL_ADDR
    jmp rax

; Pad stage2 to exactly 8KB (16 sectors) so disk layout is predictable
times 8192 - ($ - $$) db 0
