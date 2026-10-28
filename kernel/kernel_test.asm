; temporary VBE smoke test
[BITS 16]
[ORG 0x0000]
[CPU 386]

hdr_magic:      db "LiMaxOSK"
hdr_sectors:    dw 0
hdr_load_seg:   dw 0x2000
hdr_entry:      dw 0x0020
hdr_size:       dw 0
hdr_checksum:   dw 0
hdr_version:    dw 0x0100
                times 32 - ($ - $$) db 0

start:
    cli
    mov ax, 0x3000
    mov ss, ax
    mov sp, 0xFFFE
    cld

    mov ax, cs
    mov ds, ax

    mov al, 'A'
    call dbg
    call gfx_load_font
    mov al, 'B'
    call dbg

    ; dump the 8 glyph bytes for 'L' from FONT_SEG
    mov ax, 0x5000
    mov ds, ax
    mov si, 0x260
    mov cx, 8
.dumpfont:
    mov al, [si]
    call dbg_hex8
    inc si
    loop .dumpfont
    mov ax, 0x2000
    mov ds, ax

    call pci_find_vga
    mov al, 'C'
    call dbg
    call gfx_enter_unreal
    mov al, 'D'
    call dbg

    mov ax, 640
    mov bx, 360
    call gfx_set_mode
    mov al, 'E'
    call dbg

    mov word [band], 0
.bands:
    mov ax, [band]
    mov bx, 10
    mul bx
    mov bx, ax
    xor ax, ax
    mov cx, [UI_W]
    mov dx, 10
    mov si, 64
    add si, [band]
    call fill_rect
    inc word [band]
    cmp word [band], 36
    jb .bands

    mov ax, 4
    mov bx, 4
    mov cx, 120
    mov dx, 28
    mov si, 7
    call fill_rect
    mov ax, 5
    mov bx, 5
    mov cx, 118
    mov dx, 26
    mov si, 0
    call fill_rect

    mov bx, 10
    mov cx, 10
    mov dl, 1
    mov si, msg
    call draw_text

    mov al, 'R'
    call dbg
    mov ax, [dc_rows]
    call dbg_hex8
    mov ax, [dc_cols]
    call dbg_hex8
    mov ax, [dc_dest]
    call dbg_hex8
    mov ax, [dc_dest+2]
    call dbg_hex8
    mov al, 'S'
    call dbg

    ; dump 32 bytes of the back buffer at the 'L' glyph
    mov al, 'M'
    call dbg
    mov edi, 0x403214
    mov cx, 32
.dumpmem:
    mov al, [es:edi]
    call dbg_hex8
    inc edi
    loop .dumpmem
    mov al, 'N'
    call dbg

    ; dump 8 physical rows x 16 bytes starting at the 'L' glyph
    mov al, 'D'
    call dbg
    xor bx, bx
.rowdump:
    mov al, '|'
    call dbg
    mov eax, 0x403214
    movzx ecx, bx
    imul ecx, ecx, 640
    add eax, ecx
    mov edi, eax
    mov cx, 16
.rd:
    mov al, [es:edi]
    call dbg_hex8
    inc edi
    loop .rd
    inc bx
    cmp bx, 8
    jb .rowdump
    mov al, 'E'
    call dbg

    mov bx, 10
    mov cx, 24
    mov dl, 11
    mov si, msg2
    call draw_text

    mov al, 'F'
    call dbg
    call blit
    mov al, 'G'
    call dbg

.hang:
    hlt
    jmp .hang

; dbg - write AL to QEMU debug console (port 0xE9)
dbg:
    push ax
    push dx
    mov dx, 0xE9
    out dx, al
    pop dx
    pop ax
    ret

; dbg_hex8 - write AL as two hex digits to port 0xE9
dbg_hex8:
    push ax
    push bx
    push dx
    mov bl, al
    shr al, 4
    call .nibble
    mov al, bl
    call .nibble
    pop dx
    pop bx
    pop ax
    ret
.nibble:
    and al, 0x0F
    cmp al, 10
    jb .digit
    add al, 'A' - 10
    jmp .out
.digit:
    add al, '0'
.out:
    mov dx, 0xE9
    out dx, al
    ret

band    dw 0
msg     db "LiMaxOS VBE 640x360", 0
msg2    db "LFB + unreal mode OK", 0

mouse_x     dw 0
mouse_y     dw 0
scene_dirty db 0

%include "kernel/gfx.inc"
