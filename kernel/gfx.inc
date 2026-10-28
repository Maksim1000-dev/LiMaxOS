; ============================================================
;  gfx.inc - VBE 8bpp linear framebuffer, UI scale x2
;
;  Back buffer: linear 0x00400000 (up to 3840x2160 bytes)
;  Font buffer: 0x5000:0000 (256 chars x 8 bytes)
;
;  After gfx_enter_unreal ES/FS/GS are flat 4 GB selectors.
;  They must never be reloaded while the CPU is in real mode.
;  DS stays the kernel data segment (0x2000).
; ============================================================

FONT_SEG        equ 0x5000
BUF_BASE        equ 0x00400000
UI_SCALE        equ 2
VBE_INDEX_PORT  equ 0x01CE
VBE_DATA_PORT   equ 0x01CF

; ------------------------------------------------------------
; gfx_enter_unreal - load flat ES/FS/GS and return to real mode
; ------------------------------------------------------------
gfx_enter_unreal:
    cli
    lgdt [gdt_desc]

    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 0x10:pm_entry

    ; 16-bit protected mode code segment (D=0), so the far jump
    ; back to real mode is decoded with 16-bit offsets.
pm_entry:
    mov ax, 0x08
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov eax, cr0
    and al, 0xFE
    mov cr0, eax
    jmp 0x2000:unreal_ret

unreal_ret:
    mov ax, 0x2000
    mov ds, ax
    ; ES/FS/GS keep their flat 4 GB hidden descriptors.
    ret

; ------------------------------------------------------------
; vbe_write - AX = index, BX = value
; ------------------------------------------------------------
vbe_write:
    push ax
    push dx
    mov dx, VBE_INDEX_PORT
    out dx, ax
    inc dx
    mov ax, bx
    out dx, ax
    pop dx
    pop ax
    ret

; ------------------------------------------------------------
; vbe_read - AX = index -> AX = value
; ------------------------------------------------------------
vbe_read:
    push dx
    mov dx, VBE_INDEX_PORT
    out dx, ax
    inc dx
    in ax, dx
    pop dx
    ret

; ------------------------------------------------------------
; pci_find_vga - find QEMU std VGA 1234:1111 and read BAR0
; ------------------------------------------------------------
pci_find_vga:
    push eax
    push ebx
    push ecx
    push edx

    mov dword [lfb_base], 0xE0000000
    xor ebx, ebx
    mov cx, 32
.device:
    mov eax, ebx
    shl eax, 11
    or eax, 0x80000000
    mov dx, 0x0CF8
    out dx, eax
    mov dx, 0x0CFC
    in eax, dx
    cmp ax, 0x1234
    jne .next
    shr eax, 16
    cmp ax, 0x1111
    jne .next

    ; BAR0
    mov eax, ebx
    shl eax, 11
    or eax, 0x80000010
    mov dx, 0x0CF8
    out dx, eax
    mov dx, 0x0CFC
    in eax, dx
    and eax, 0xFFFFFFF0
    test eax, eax
    jz .next
    mov [lfb_base], eax

    ; enable memory space (and keep I/O space enabled)
    mov eax, ebx
    shl eax, 11
    or eax, 0x80000004
    mov dx, 0x0CF8
    out dx, eax
    mov dx, 0x0CFC
    in ax, dx
    or ax, 0x0003
    out dx, ax
    jmp .done

.next:
    inc bx
    loop .device

.done:
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; ------------------------------------------------------------
; gfx_set_mode - AX = width, BX = height
; Rounds to VBE-safe values and switches the Bochs VBE mode.
; ------------------------------------------------------------
gfx_set_mode:
    pusha

    and ax, 0xFFF8          ; xres must be a multiple of 8
    and bx, 0xFFFE
    cmp ax, 640
    jae .w_min
    mov ax, 640
.w_min:
    cmp ax, 3840
    jbe .w_max
    mov ax, 3840
.w_max:
    cmp bx, 360
    jae .h_min
    mov bx, 360
.h_min:
    cmp bx, 2160
    jbe .h_max
    mov bx, 2160
.h_max:
    mov [new_w], ax
    mov [new_h], bx

    cmp ax, [SCREEN_W]
    jne .set
    cmp bx, [SCREEN_H]
    jne .set
    popa
    ret

.set:
    ; disable VBE before changing the mode
    mov ax, 4
    xor bx, bx
    call vbe_write

    mov ax, 1
    mov bx, [new_w]
    call vbe_write
    mov ax, 2
    mov bx, [new_h]
    call vbe_write
    mov ax, 3
    mov bx, 8
    call vbe_write
    mov ax, 6
    mov bx, [new_w]
    call vbe_write
    mov ax, 7
    mov bx, [new_h]
    call vbe_write
    mov ax, 8
    xor bx, bx
    call vbe_write
    mov ax, 9
    xor bx, bx
    call vbe_write

    ; enable + linear framebuffer
    mov ax, 4
    mov bx, 0x41
    call vbe_write

    mov ax, [new_w]
    mov [SCREEN_W], ax
    mov bx, ax
    shr bx, 1
    mov [UI_W], bx

    mov ax, [new_h]
    mov [SCREEN_H], ax
    mov bx, ax
    shr bx, 1
    mov [UI_H], bx

    ; keep the pointer speed proportional to the logical desktop
    mov ax, [UI_W]
    xor dx, dx
    mov bx, 320
    div bx
    test ax, ax
    jnz .speed_ok
    mov ax, 1
.speed_ok:
    mov [mouse_speed], ax

    call gfx_set_palette
    call gfx_update_mode_text

    ; clear the back buffer
    xor ax, ax
    xor bx, bx
    mov cx, [UI_W]
    mov dx, [UI_H]
    xor si, si
    call fill_rect

    ; keep the logical pointer inside the new logical surface
    mov ax, [UI_W]
    sub ax, 9
    cmp [mouse_x], ax
    jbe .mx
    mov [mouse_x], ax
.mx:
    mov ax, [UI_H]
    sub ax, 13
    cmp [mouse_y], ax
    jbe .my
    mov [mouse_y], ax
.my:
    mov byte [cursor_drawn], 0
    mov byte [need_cursor], 1
    mov byte [scene_dirty], 1
    popa
    ret

; ------------------------------------------------------------
; gfx_set_palette - upload 256 6-bit RGB entries
; ------------------------------------------------------------
gfx_set_palette:
    pusha
    push ds
    mov dx, 0x3C8
    xor al, al
    out dx, al
    inc dx
    mov si, palette_data
    mov cx, palette_entries * 3
    cld
    rep outsb
    mov si, palette_tail
    mov cx, (256 - palette_entries) * 3
    rep outsb
    pop ds
    popa
    ret

; ------------------------------------------------------------
; gfx_load_font - copy the BIOS 8x8 font to FONT_SEG:0000
; Must be called before gfx_enter_unreal.
; ------------------------------------------------------------
gfx_load_font:
    pusha
    push ds
    push es

    mov ax, 0x1130
    mov bh, 0x03
    int 0x10
    mov ax, es
    or ax, bp
    jz .fallback

    mov si, bp
    mov ax, es
    mov ds, ax
    jmp .copy

.fallback:
    mov ax, 0xF000
    mov ds, ax
    mov si, 0xFA6E

.copy:
    mov ax, FONT_SEG
    mov es, ax
    xor di, di
    mov cx, 2048
    cld
    rep movsb

    pop es
    pop ds
    popa
    ret

; ------------------------------------------------------------
; fill_rect - AX=x BX=y CX=w DX=h SI=color (logical UI coords)
; ------------------------------------------------------------
fill_rect:
    pusha

    test cx, cx
    jz .done
    test dx, dx
    jz .done
    cmp ax, [UI_W]
    jae .done
    cmp bx, [UI_H]
    jae .done

    push ax
    mov ax, si
    mov [rect_color], al
    pop ax

    mov si, [UI_W]
    sub si, ax
    cmp cx, si
    jbe .width_ok
    mov cx, si
.width_ok:
    mov si, [UI_H]
    sub si, bx
    cmp dx, si
    jbe .height_ok
    mov dx, si
.height_ok:
    mov [rect_w], cx
    shl dx, 1                ; each logical row is two physical rows
    mov [rect_h], dx

    ; physical destination
    movzx eax, ax
    shl eax, 1
    movzx ebx, bx
    shl ebx, 1
    movzx ecx, word [SCREEN_W]
    imul ebx, ecx
    add eax, ebx
    add eax, BUF_BASE
    mov [rect_dest], eax

    movzx eax, word [rect_w]
    shl eax, 1
    mov [rect_w2], eax
    movzx eax, word [SCREEN_W]
    mov [rect_stride], eax

.row:
    mov edi, [rect_dest]
    mov ecx, [rect_w2]
    mov al, [rect_color]
    cld
    a32 rep stosb

    mov eax, [rect_stride]
    add [rect_dest], eax
    dec word [rect_h]
    jnz .row

.done:
    popa
    ret

; ------------------------------------------------------------
; put_pixel - AX=x BX=y SI=color (logical UI coords)
; ------------------------------------------------------------
put_pixel:
    pusha
    cmp ax, [UI_W]
    jae .done
    cmp bx, [UI_H]
    jae .done

    push ax
    mov ax, si
    mov [rect_color], al
    pop ax

    movzx eax, ax
    shl eax, 1
    movzx ebx, bx
    shl ebx, 1
    movzx ecx, word [SCREEN_W]
    imul ebx, ecx
    add eax, ebx
    add eax, BUF_BASE
    mov edi, eax

    movzx ebp, word [SCREEN_W]
    mov al, [rect_color]
    mov [es:edi], al
    mov [es:edi+1], al
    mov [es:edi+ebp], al
    mov [es:edi+ebp+1], al

.done:
    popa
    ret

; ------------------------------------------------------------
; frame_rect - AX=x BX=y CX=w DX=h SI=color (logical)
; ------------------------------------------------------------
frame_rect:
    pusha
    mov [frm_x], ax
    mov [frm_y], bx
    mov [frm_w], cx
    mov [frm_h], dx
    mov [frm_c], si

    mov ax, [frm_x]
    mov bx, [frm_y]
    mov cx, [frm_w]
    mov dx, 1
    mov si, [frm_c]
    call fill_rect

    mov ax, [frm_x]
    mov bx, [frm_y]
    add bx, [frm_h]
    dec bx
    mov cx, [frm_w]
    mov dx, 1
    mov si, [frm_c]
    call fill_rect

    mov ax, [frm_x]
    mov bx, [frm_y]
    inc bx
    mov cx, 1
    mov dx, [frm_h]
    sub dx, 2
    jbe .right
    mov si, [frm_c]
    call fill_rect

.right:
    mov ax, [frm_x]
    add ax, [frm_w]
    dec ax
    mov bx, [frm_y]
    inc bx
    mov cx, 1
    mov dx, [frm_h]
    sub dx, 2
    jbe .done
    mov si, [frm_c]
    call fill_rect

.done:
    popa
    ret

; ------------------------------------------------------------
; draw_char - AL=char BX=x CX=y DL=color (logical, scaled x2)
; ------------------------------------------------------------
draw_char:
    pusha
    push ds

    mov [cs:dc_char], al
    mov [cs:dc_x], bx
    mov [cs:dc_y], cx
    mov [cs:dc_color], dl

    cmp bx, [UI_W]
    jae .done
    cmp cx, [UI_H]
    jae .done

    mov ax, [UI_W]
    sub ax, [cs:dc_x]
    cmp ax, 8
    jbe .cols_ok
    mov ax, 8
.cols_ok:
    mov [cs:dc_cols], ax

    mov ax, [UI_H]
    sub ax, [cs:dc_y]
    cmp ax, 8
    jbe .rows_ok
    mov ax, 8
.rows_ok:
    mov [cs:dc_rows], ax

    movzx eax, word [cs:dc_x]
    shl eax, 1
    movzx ebx, word [cs:dc_y]
    shl ebx, 1
    movzx ecx, word [SCREEN_W]
    imul ebx, ecx
    add eax, ebx
    add eax, BUF_BASE
    mov [cs:dc_dest], eax

    xor ax, ax
    mov al, [cs:dc_char]
    mov si, ax
    shl si, 3
    mov ax, FONT_SEG
    mov ds, ax

    movzx ebp, word [cs:SCREEN_W]
    mov dx, [cs:dc_rows]

.row:
    lodsb
    mov bl, al
    mov edi, [cs:dc_dest]
    mov cx, [cs:dc_cols]
.bit:
    shl bl, 1
    jnc .skip
    mov al, [cs:dc_color]
    mov [es:edi], al
    mov [es:edi+1], al
    mov [es:edi+ebp], al
    mov [es:edi+ebp+1], al
.skip:
    add edi, 2
    dec cx
    jnz .bit

    mov eax, ebp
    shl eax, 1
    add [cs:dc_dest], eax
    dec dx
    jnz .row

.done:
    pop ds
    popa
    ret

; ------------------------------------------------------------
; draw_text - SI=ASCIIZ string BX=x CX=y DL=color (logical)
; ------------------------------------------------------------
draw_text:
    pusha
    mov [dt_x], bx
    mov [dt_y], cx
    mov [dt_color], dl

.next:
    lodsb
    test al, al
    jz .done
    mov bx, [dt_x]
    mov cx, [dt_y]
    mov dl, [dt_color]
    call draw_char
    add word [dt_x], 8
    jmp .next

.done:
    popa
    ret

; ------------------------------------------------------------
; text_width - SI=ASCIIZ string -> AX = logical pixel width
; ------------------------------------------------------------
text_width:
    push si
    push bx
    xor bx, bx
.loop:
    lodsb
    test al, al
    jz .done
    inc bx
    jmp .loop
.done:
    mov ax, bx
    shl ax, 3
    pop bx
    pop si
    ret

; ------------------------------------------------------------
; blit - copy the whole back buffer to the LFB
; ------------------------------------------------------------
blit:
    pusha
    mov esi, BUF_BASE
    mov edi, [lfb_base]
    movzx eax, word [SCREEN_W]
    movzx ebx, word [SCREEN_H]
    imul eax, ebx
    shr eax, 2
    mov ecx, eax
    cld
    fs a32 rep movsd
    popa
    ret

; ------------------------------------------------------------
; copy_buffer_rect - AX=x BX=y CX=w DX=h (physical)
; Copy from the back buffer to the LFB (cursor erase).
; ------------------------------------------------------------
copy_buffer_rect:
    pusha

    test cx, cx
    jz .done
    test dx, dx
    jz .done
    cmp ax, [SCREEN_W]
    jae .done
    cmp bx, [SCREEN_H]
    jae .done

    mov si, [SCREEN_W]
    sub si, ax
    cmp cx, si
    jbe .width_ok
    mov cx, si
.width_ok:
    mov si, [SCREEN_H]
    sub si, bx
    cmp dx, si
    jbe .height_ok
    mov dx, si
.height_ok:
    mov [cpy_w], cx
    mov [cpy_h], dx

    movzx eax, ax
    movzx ebx, bx
    movzx ecx, word [SCREEN_W]
    imul ebx, ecx
    add eax, ebx
    mov [cpy_src], eax

    add eax, BUF_BASE
    mov [cpy_src_lin], eax

    mov eax, [cpy_src]
    add eax, [lfb_base]
    mov [cpy_dst_lin], eax

.row:
    mov esi, [cpy_src_lin]
    mov edi, [cpy_dst_lin]
    mov ecx, [cpy_w]
    cld
    fs a32 rep movsb

    movzx eax, word [SCREEN_W]
    add [cpy_src_lin], eax
    add [cpy_dst_lin], eax
    dec word [cpy_h]
    jnz .row

.done:
    popa
    ret

; ------------------------------------------------------------
; draw_cursor - draw the pointer on the LFB (physical)
; ------------------------------------------------------------
draw_cursor:
    pusha
    mov ax, [mouse_x]
    shl ax, 1
    mov bx, [mouse_y]
    shl bx, 1
    add ax, 2
    add bx, 2
    mov byte [cursor_draw_color], 0
    call cursor_blit

    mov ax, [mouse_x]
    shl ax, 1
    mov bx, [mouse_y]
    shl bx, 1
    mov byte [cursor_draw_color], 1
    call cursor_blit
    popa
    ret

cursor_blit:
    pusha
    movzx eax, ax
    movzx ebx, bx
    movzx ecx, word [SCREEN_W]
    imul ebx, ecx
    add eax, ebx
    add eax, [lfb_base]
    mov edi, eax

    movzx ebp, word [SCREEN_W]
    mov si, cursor_bitmap
    mov dx, 12
.row:
    lodsb
    mov bl, al
    mov cx, 8
.bit:
    shl bl, 1
    jnc .skip
    mov al, [cursor_draw_color]
    mov [es:edi], al
    mov [es:edi+1], al
    mov [es:edi+ebp], al
    mov [es:edi+ebp+1], al
.skip:
    add edi, 2
    dec cx
    jnz .bit
    add edi, ebp
    add edi, ebp
    sub edi, 16
    dec dx
    jnz .row

    popa
    ret

; ------------------------------------------------------------
; gfx_update_mode_text - build "WxH" in mode_text
; ------------------------------------------------------------
gfx_update_mode_text:
    pusha
    mov di, mode_text
    mov ax, [SCREEN_W]
    call gfx_u16_dec
    mov al, 'x'
    mov [di], al
    inc di
    mov ax, [SCREEN_H]
    call gfx_u16_dec
    mov byte [di], 0
    popa
    ret

; AX=value, DI=destination, returns DI advanced
gfx_u16_dec:
    push ax
    push bx
    push cx
    push dx
    mov bx, 10
    xor cx, cx
.divide:
    xor dx, dx
    div bx
    push dx
    inc cx
    test ax, ax
    jnz .divide
.output:
    pop ax
    add al, '0'
    mov [di], al
    inc di
    loop .output
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ------------------------------------------------------------
; GDT
; ------------------------------------------------------------
gdt_start:
    dq 0
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92
    db 0xCF
    db 0x00
gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x02            ; base 0x00020000
    db 0x9A
    db 0x00            ; 16-bit code, limit 64K
    db 0x00
gdt_end:
gdt_desc:
    dw gdt_end - gdt_start - 1
    dd 0x00020000 + gdt_start

; ------------------------------------------------------------
; graphics data
; ------------------------------------------------------------
SCREEN_W        dw 0
SCREEN_H        dw 0
UI_W            dw 0
UI_H            dw 0
new_w           dw 0
new_h           dw 0
lfb_base        dd 0xE0000000
mouse_speed     dw 1
mode_text       times 12 db 0

rect_w          dw 0
rect_h          dw 0
rect_color      db 0
rect_dest       dd 0
rect_w2         dd 0
rect_stride     dd 0
frm_x           dw 0
frm_y           dw 0
frm_w           dw 0
frm_h           dw 0
frm_c           dw 0
dc_char         db 0
dc_x            dw 0
dc_y            dw 0
dc_color        db 0
dc_cols         dw 0
dc_rows         dw 0
dc_dest         dd 0
dt_x            dw 0
dt_y            dw 0
dt_color        db 0
cpy_w           dw 0
cpy_h           dw 0
cpy_src         dd 0
cpy_src_lin     dd 0
cpy_dst_lin     dd 0
cursor_draw_color db 0

cursor_bitmap:
    db 10000000b
    db 11000000b
    db 11100000b
    db 11110000b
    db 11111000b
    db 11111100b
    db 11111110b
    db 11111111b
    db 11111000b
    db 11011100b
    db 10001110b
    db 00000111b

; 64 base colours + 36 desktop gradient shades (64..99)
palette_entries equ 100
palette_data:
    db 0,0,0            ; 0 black
    db 63,63,63         ; 1 white
    db 42,42,42         ; 2 light grey
    db 21,21,21         ; 3 dark grey
    db 6,6,10           ; 4 near black
    db 0,8,26           ; 5 deep navy
    db 0,20,50          ; 6 blue
    db 0,63,63          ; 7 cyan
    db 0,34,38          ; 8 dark cyan
    db 0,63,0           ; 9 green
    db 0,32,0           ; 10 dark green
    db 63,63,0          ; 11 yellow
    db 63,34,0          ; 12 orange
    db 63,0,0           ; 13 red
    db 63,0,40          ; 14 magenta
    db 30,0,50          ; 15 purple
    db 12,12,14         ; 16 panel dark
    db 28,28,32         ; 17 panel light
    db 48,48,52         ; 18 panel highlight
    db 63,24,24         ; 19 soft red
    db 20,48,63         ; 20 sky
    db 32,0,48          ; 21 violet
    db 0,48,32          ; 22 mint
    db 48,32,0          ; 23 amber

    ; 24..63 reserved / useful UI colours
    db 8,12,20, 10,16,28, 12,20,34, 14,24,40
    db 16,28,48, 18,32,54, 20,36,58, 22,40,60
    db 24,44,63, 6,10,16, 10,14,22, 14,18,28
    db 18,22,32, 22,26,38, 26,30,44, 30,34,50
    db 34,38,56, 38,42,60, 42,46,63, 46,50,63
    db 50,54,63, 54,58,63, 58,60,63, 60,62,63
    db 63,20,20, 63,32,0, 63,44,0, 63,56,0
    db 32,63,0, 16,63,0, 0,63,16, 0,63,32
    db 0,63,48, 0,63,63, 0,48,63, 0,32,63
    db 0,40,63, 0,24,48, 16,0,32, 32,0,16

    ; 64..99 desktop gradient (36 shades, dark blue -> teal)
    db 1,3,9
    db 1,4,10
    db 1,4,11
    db 1,5,12
    db 1,5,13
    db 1,6,14
    db 1,6,15
    db 2,7,16
    db 2,7,17
    db 2,8,18
    db 2,8,19
    db 2,9,20
    db 2,9,21
    db 3,10,22
    db 3,10,23
    db 3,11,24
    db 3,11,25
    db 3,12,26
    db 4,12,27
    db 4,13,28
    db 4,13,29
    db 4,14,30
    db 4,14,31
    db 5,15,32
    db 5,15,33
    db 5,16,34
    db 5,16,35
    db 5,17,36
    db 6,17,37
    db 6,18,38
    db 6,18,39
    db 6,19,40
    db 7,19,41
    db 7,20,42
    db 7,20,43
    db 8,22,46

palette_tail times (256 - palette_entries) * 3 db 0
