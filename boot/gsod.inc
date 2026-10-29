; ============================================================
;  boot/gsod.inc - text-mode Green Screen of Death for LMAXLDR
;  AX = error code.  Never returns.
; ============================================================

gsod:
    cli
    mov [gsod_code], ax

    ; green background, white text
    push es
    mov ax, 0xB800
    mov es, ax
    xor di, di
    mov ax, 0x2F20
    mov cx, 2000
    cld
    rep stosw
    pop es

    ; sad face + title
    mov al, 2
    mov ah, 4
    call goto_xy
    mov al, 0x2F
    call set_attr
    mov si, gsod_face
    call puts
    mov si, gsod_msg_title
    call puts

    ; SYSTEM HALTED
    mov al, 3
    mov ah, 4
    call goto_xy
    mov si, gsod_msg_halted
    call puts

    ; CODE OF ERROR: <code> <name>
    mov al, 5
    mov ah, 4
    call goto_xy
    mov si, gsod_msg_code
    call puts
    mov ax, [gsod_code]
    call print_dec16
    mov al, ' '
    call putchar
    mov ax, [gsod_code]
    call gsod_find_name
    call puts

    ; reset hint
    mov al, 7
    mov ah, 4
    call goto_xy
    mov si, gsod_msg_reset
    call puts

.hang:
    cli
    hlt
    jmp .hang

gsod_face       db ":(  ", 0
gsod_code       dw 0
