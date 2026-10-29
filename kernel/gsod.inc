; ============================================================
;  kernel/gsod.inc - Green Screen of Death
;
;  AX = error code.  Shows the sad face, the title, the code
;  and its catalogue name, then halts the CPU.
;
;  Uses the linear framebuffer when the video mode is ready,
;  otherwise falls back to the 80x25 text buffer.
; ============================================================

; ------------------------------------------------------------
; install_exceptions - point safe CPU exception vectors at stubs
; In real mode vectors 0x10..0x1A belong to the BIOS, so they
; are never hooked.  Vectors 8..15 are installed by the kernel
; after irq_remap has moved the PIC to 0x20..0x2F.
; ------------------------------------------------------------
install_exceptions:
    pusha
    push es

    xor ax, ax
    mov es, ax
    mov si, ex_stub_table
    xor di, di
    mov bx, cs
    mov cx, 8
.loop:
    mov ax, [si]
    mov [es:di], ax
    mov [es:di+2], bx
    add si, 2
    add di, 4
    loop .loop

    pop es
    popa
    ret

; SI = stub-offset table, DI = first vector * 4, CX = count
; Called after gfx_enter_unreal: ES is already flat (base 0), so it
; must NOT be reloaded or pushed/popped here.  In real mode pop es
; would replace the flat descriptor with base 8<<4 = 0x80.
install_exception_vectors:
    pusha
    mov bx, cs
.loop:
    mov ax, [si]
    mov [fs:di], ax
    mov [fs:di+2], bx
    add si, 2
    add di, 4
    loop .loop
    popa
    ret

; ------------------------------------------------------------
; exception stubs: push the vector number and fall into gsod
; Exceptions 8/10..14/17 already have a CPU error code on the
; stack; it is simply left below our own vector number.
; ------------------------------------------------------------
%macro EX_STUB 2
ex_stub_%1:
    push word %1
    jmp ex_common
%endmacro

EX_STUB 0, ERR_DIVIDE_BY_ZERO
EX_STUB 1, ERR_UNHANDLED
EX_STUB 2, ERR_UNHANDLED
EX_STUB 3, ERR_UNHANDLED
EX_STUB 4, ERR_OVERFLOW
EX_STUB 5, ERR_BOUNDS
EX_STUB 6, ERR_INVALID_OPCODE
EX_STUB 7, ERR_UNHANDLED
EX_STUB 8, ERR_DOUBLE_FAULT
EX_STUB 9, ERR_UNHANDLED
EX_STUB 10, ERR_UNHANDLED
EX_STUB 11, ERR_UNHANDLED

; #SS has an error code.  A plain INT 0Ch (no error code) can land
; here on some chipsets: its frame is IP,CS,FLAGS, so [bp+4] holds
; FLAGS instead of the kernel CS.  Acknowledge it and return.
ex_stub_12:
    mov bp, sp
    cmp word [ss:bp+4], 0x2000
    jne .spurious
    push word 12
    jmp ex_common
.spurious:
    mov al, 0x20
    out 0xA0, al
    out 0x20, al
    iret

EX_STUB 13, ERR_GENERAL_PROTECTION
EX_STUB 14, ERR_UNHANDLED
EX_STUB 15, ERR_UNHANDLED
EX_STUB 16, ERR_UNHANDLED
EX_STUB 17, ERR_UNHANDLED
EX_STUB 18, ERR_UNHANDLED
EX_STUB 19, ERR_UNHANDLED

ex_common:
    pop bx
    shl bx, 1
    mov ax, [cs:ex_vector_codes + bx]
    jmp gsod

; vector -> catalogue code
ex_vector_codes:
    dw ERR_DIVIDE_BY_ZERO      ; 0
    dw ERR_UNHANDLED           ; 1 debug
    dw ERR_UNHANDLED           ; 2 NMI
    dw ERR_UNHANDLED           ; 3 breakpoint
    dw ERR_OVERFLOW            ; 4
    dw ERR_BOUNDS              ; 5
    dw ERR_INVALID_OPCODE      ; 6
    dw ERR_UNHANDLED           ; 7 device not available
    dw ERR_DOUBLE_FAULT        ; 8
    dw ERR_UNHANDLED           ; 9
    dw ERR_UNHANDLED           ; 10
    dw ERR_UNHANDLED           ; 11
    dw ERR_STACK_FAULT         ; 12
    dw ERR_GENERAL_PROTECTION  ; 13
    dw ERR_UNHANDLED           ; 14 page fault
    dw ERR_UNHANDLED           ; 15

ex_stub_table:
    dw ex_stub_0,  ex_stub_1,  ex_stub_2,  ex_stub_3
    dw ex_stub_4,  ex_stub_5,  ex_stub_6,  ex_stub_7
    dw ex_stub_8,  ex_stub_9,  ex_stub_10, ex_stub_11
    dw ex_stub_12, ex_stub_13, ex_stub_14, ex_stub_15
    dw ex_stub_16, ex_stub_17, ex_stub_18, ex_stub_19

; ------------------------------------------------------------
; gsod - AX = error code, never returns
; ------------------------------------------------------------
gsod:
    cli
    mov [cs:gsod_code], ax

    mov ax, cs
    mov ds, ax
    cld

    call gsod_build_line

    cmp byte [gfx_ready], 0
    je gsod_text

    ; ---- green desktop, white foreground ----
    xor ax, ax
    xor bx, bx
    mov cx, [UI_W]
    mov dx, [UI_H]
    mov si, 9
    call fill_rect

    ; ---- big ASCII sad face :( ----
    mov ax, [UI_W]
    sub ax, 128
    shr ax, 1
    mov bx, ax
    mov cx, 48
    mov al, ':'
    mov dl, 1
    mov si, 8
    call gsod_big_char

    mov ax, [UI_W]
    sub ax, 128
    shr ax, 1
    add ax, 64
    mov bx, ax
    mov cx, 48
    mov al, '('
    mov dl, 1
    mov si, 8
    call gsod_big_char

    ; ---- SOMETING WENT WRONG ----
    mov si, gsod_msg_title
    call text_width
    mov bx, [UI_W]
    sub bx, ax
    shr bx, 1
    mov cx, 140
    mov dl, 1
    mov si, gsod_msg_title
    call draw_text

    ; ---- SYSTEM HALTED ----
    mov si, gsod_msg_halted
    call text_width
    mov bx, [UI_W]
    sub bx, ax
    shr bx, 1
    mov cx, 180
    mov dl, 1
    mov si, gsod_msg_halted
    call draw_text

    ; ---- CODE OF ERROR: <code> <name> ----
    mov si, gsod_line
    call text_width
    mov bx, [UI_W]
    sub bx, ax
    shr bx, 1
    mov cx, 220
    mov dl, 1
    mov si, gsod_line
    call draw_text

    call blit
    jmp gsod_halt

; ------------------------------------------------------------
; gsod_text - 80x25 fallback (green background, white text)
; ------------------------------------------------------------
gsod_text:
    mov ax, 0xB800
    mov es, ax
    xor di, di
    mov ax, 0x2F20
    mov cx, 2000
    rep stosw

    mov di, 2 * 160 + 4 * 2
    mov si, gsod_face
    mov ah, 0x2F
    call gsod_tputs
    mov si, gsod_msg_title
    call gsod_tputs

    mov di, 4 * 160 + 4 * 2
    mov si, gsod_line
    mov ah, 0x2F
    call gsod_tputs

    mov di, 6 * 160 + 4 * 2
    mov si, gsod_msg_halted
    mov ah, 0x2F
    call gsod_tputs

gsod_halt:
    cli
.hang:
    hlt
    jmp .hang

; ------------------------------------------------------------
; gsod_tputs - SI = ASCIIZ, DI = video offset, AH = attribute
; ES must be 0xB800
; ------------------------------------------------------------
gsod_tputs:
    lodsb
    test al, al
    jz .done
    stosw
    jmp gsod_tputs
.done:
    ret

; ------------------------------------------------------------
; gsod_build_line - build "CODE OF ERROR: nnn NAME" in gsod_line
; ------------------------------------------------------------
gsod_build_line:
    pusha

    mov di, gsod_line
    mov si, gsod_msg_code
.copy_prefix:
    lodsb
    test al, al
    jz .digits
    mov [di], al
    inc di
    jmp .copy_prefix

.digits:
    mov ax, [gsod_code]
    call gsod_append_dec

    mov al, ' '
    mov [di], al
    inc di

    mov ax, [gsod_code]
    call gsod_find_name
.copy_name:
    lodsb
    test al, al
    jz .done
    mov [di], al
    inc di
    jmp .copy_name

.done:
    mov byte [di], 0
    popa
    ret

; ------------------------------------------------------------
; gsod_append_dec - AX = value, DI = destination, advances DI
; ------------------------------------------------------------
gsod_append_dec:
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
; gsod_big_char - AL=char BX=x CX=y DL=color SI=scale (logical)
; Draws the 8x8 BIOS glyph with each font dot as scale x scale.
; ------------------------------------------------------------
gsod_big_char:
    pusha

    mov [cs:gb_char], al
    mov [cs:gb_x], bx
    mov [cs:gb_y], cx
    mov [cs:gb_color], dl
    mov [cs:gb_scale], si

    mov word [cs:gb_row], 0
.row:
    mov word [cs:gb_col], 0
.col:
    ; AL = font row byte, read through the flat ES selector
    xor eax, eax
    mov al, [cs:gb_char]
    shl eax, 3
    add eax, FONT_SEG * 16
    movzx ebx, word [cs:gb_row]
    add eax, ebx
    mov edi, eax
    mov al, [es:edi]

    mov cl, [cs:gb_col]
    mov ah, 0x80
    shr ah, cl
    test al, ah
    jz .skip

    ; x = gb_x + col * scale
    mov ax, [cs:gb_col]
    xor dx, dx
    mul word [cs:gb_scale]
    add ax, [cs:gb_x]
    mov bp, ax

    ; y = gb_y + row * scale
    mov ax, [cs:gb_row]
    xor dx, dx
    mul word [cs:gb_scale]
    add ax, [cs:gb_y]

    mov bx, ax
    mov ax, bp
    mov cx, [cs:gb_scale]
    mov dx, cx
    movzx si, byte [cs:gb_color]
    call fill_rect

.skip:
    inc word [cs:gb_col]
    cmp word [cs:gb_col], 8
    jb .col

    inc word [cs:gb_row]
    cmp word [cs:gb_row], 8
    jb .row

    popa
    ret

; ------------------------------------------------------------
; GSOD data
; ------------------------------------------------------------
gsod_code       dw 0
gsod_line       times 80 db 0
gsod_face       db ":(  ", 0

gb_char         db 0
gb_x            dw 0
gb_y            dw 0
gb_color        db 0
gb_scale        dw 0
gb_row          dw 0
gb_col          dw 0
