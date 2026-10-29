; ============================================================
;  serial.inc - COM1 (0x3F8) auto-resolution protocol
;
;  host -> guest : WxH\n
;  guest -> host : HELLO\n, MODE WxH\n, LOCK\n, AUTO\n
; ============================================================

; ------------------------------------------------------------
; serial_init - 115200 8N1, FIFO, no UART interrupts
; ------------------------------------------------------------
serial_init:
    pusha
    mov dx, 0x3F9
    xor al, al
    out dx, al              ; IER = 0

    mov dx, 0x3FB
    mov al, 0x80
    out dx, al              ; DLAB
    mov dx, 0x3F8
    mov al, 1
    out dx, al              ; divisor low
    mov dx, 0x3F9
    xor al, al
    out dx, al              ; divisor high
    mov dx, 0x3FB
    mov al, 0x03
    out dx, al              ; 8N1, DLAB off
    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al              ; FIFO enable + clear
    mov dx, 0x3FC
    mov al, 0x03
    out dx, al              ; DTR + RTS

    mov dx, 0x3F9
    xor al, al
    out dx, al              ; IER = 0 again (no UART IRQs, ever)

    mov word [serial_len], 0
    mov byte [helper_seen], 0
    popa
    ret

; ------------------------------------------------------------
; serial_putc - AL, bounded wait for THR empty
; ------------------------------------------------------------
serial_putc:
    push ax
    push bx
    push cx
    push dx
    mov bl, al
    mov cx, 0x8000
    mov dx, 0x3FD
.wait:
    in al, dx
    test al, 0x20
    jnz .ready
    loop .wait
    jmp .done
.ready:
    mov dx, 0x3F8
    mov al, bl
    out dx, al
.done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ------------------------------------------------------------
; serial_send - SI = ASCIIZ
; ------------------------------------------------------------
serial_send:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    call serial_putc
    jmp .loop
.done:
    popa
    ret

serial_send_hello:
    mov si, serial_msg_hello
    jmp serial_send

serial_send_lock:
    mov si, serial_msg_lock
    jmp serial_send

serial_send_auto:
    mov si, serial_msg_auto
    jmp serial_send

serial_send_mode:
    mov si, serial_msg_mode
    call serial_send
    mov si, mode_text
    call serial_send
    mov si, serial_msg_nl
    jmp serial_send

; ------------------------------------------------------------
; serial_poll - read pending bytes, execute complete lines
; ------------------------------------------------------------
serial_poll:
    pusha
.loop:
    mov dx, 0x3FD
    in al, dx
    test al, 0x01
    jz .done
    mov dx, 0x3F8
    in al, dx

    cmp al, 13
    je .loop
    cmp al, 10
    je .line

    cmp al, 32
    jb .loop
    mov bx, [serial_len]
    cmp bx, 38
    jae .loop
    mov [serial_line + bx], al
    inc word [serial_len]
    jmp .loop

.line:
    mov bx, [serial_len]
    mov byte [serial_line + bx], 0
    mov word [serial_len], 0
    call serial_parse
    jmp .loop
.done:
    popa
    ret

; ------------------------------------------------------------
; serial_parse - parse "WxH" or "MODE WxH"
; ------------------------------------------------------------
serial_parse:
    pusha
    mov si, serial_line

    ; optional leading spaces
.skip:
    cmp byte [si], ' '
    jne .prefix
    inc si
    jmp .skip
.prefix:
    cmp word [si], 'MO'
    jne .width
    cmp word [si+2], 'DE'
    jne .width
    cmp byte [si+4], ' '
    jne .width
    add si, 5
    jmp .width

.width:
    xor dx, dx
    xor cx, cx
.wdigits:
    mov al, [si]
    cmp al, '0'
    jb .wdone
    cmp al, '9'
    ja .wdone
    imul dx, 10
    sub al, '0'
    xor ah, ah
    add dx, ax
    inc si
    inc cx
    jmp .wdigits
.wdone:
    test cx, cx
    jz .done
    cmp byte [si], 'x'
    je .times
    cmp byte [si], 'X'
    jne .done
.times:
    inc si
    xor bx, bx
    xor cx, cx
.hdigits:
    mov al, [si]
    cmp al, '0'
    jb .hdone
    cmp al, '9'
    ja .hdone
    imul bx, 10
    sub al, '0'
    xor ah, ah
    add bx, ax
    inc si
    inc cx
    jmp .hdigits
.hdone:
    test cx, cx
    jz .done

    mov byte [helper_seen], 1
    cmp byte [display_locked], 0
    jne .done

    mov ax, dx
    call display_apply_auto
.done:
    popa
    ret

; ------------------------------------------------------------
; serial data
; ------------------------------------------------------------
serial_len      dw 0
serial_line     times 42 db 0
helper_seen     db 0

serial_msg_hello    db "HELLO", 10, 0
serial_msg_lock     db "LOCK", 10, 0
serial_msg_auto     db "AUTO", 10, 0
serial_msg_mode     db "MODE ", 0
serial_msg_nl       db 10, 0
