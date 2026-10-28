; ============================================================
;  mouse.inc - PS/2 mouse on IRQ12 (vector 0x2C)
;  Coordinates are LOGICAL UI pixels.  The pointer bitmap is
;  scaled x2 when drawn into the linear framebuffer.
; ============================================================

; ------------------------------------------------------------
; mouse_init - enable the auxiliary PS/2 port and IRQ12
; ------------------------------------------------------------
mouse_init:
    pusha
    call kbd_flush

    ; Enable the auxiliary PS/2 port
    call mouse_wait_write
    mov al, 0xA8
    out 0x64, al

    ; Read the controller command byte
    call mouse_wait_write
    mov al, 0x20
    out 0x64, al
    call mouse_wait_read
    in al, 0x60
    or al, 0x03             ; keyboard IRQ1 + mouse IRQ12
    and al, 0xDF            ; auxiliary clock enabled
    and al, 0xEF            ; keyboard clock enabled
    mov bl, al

    ; Write the command byte back
    call mouse_wait_write
    mov al, 0x60
    out 0x64, al
    call mouse_wait_write
    mov al, bl
    out 0x60, al

    ; Mouse defaults, then enable data reporting (retry until ACK)
    call kbd_flush
    mov al, 0xF6
    call mouse_command
    call kbd_flush

    mov cx, 5
.enable_retry:
    mov al, 0xF4
    call mouse_command
    cmp al, 0xFA
    je .enabled
    call kbd_flush
    loop .enable_retry
.enabled:
    call kbd_flush

    mov ax, [UI_W]
    shr ax, 1
    mov [mouse_x], ax
    mov ax, [UI_H]
    shr ax, 1
    mov [mouse_y], ax
    mov byte [mouse_cycle], 0
    mov byte [mouse_left], 0
    mov byte [mouse_moved], 0
    mov byte [mouse_pressed], 0
    mov byte [mouse_released], 0

    popa
    ret

; ------------------------------------------------------------
; mouse_wait_write / mouse_wait_read - bounded controller waits
; ------------------------------------------------------------
mouse_wait_write:
    push cx
    mov cx, 0xFFFF
.wait:
    in al, 0x64
    test al, 0x02
    jz .ready
    loop .wait
.ready:
    pop cx
    ret

mouse_wait_read:
    push cx
    mov cx, 0xFFFF
.wait:
    in al, 0x64
    test al, 0x01
    jnz .ready
    loop .wait
.ready:
    pop cx
    ret

; ------------------------------------------------------------
; mouse_command - send AL to the auxiliary device and read ACK
; ------------------------------------------------------------
mouse_command:
    push bx
    push cx
    mov bl, al
    call mouse_wait_write
    mov al, 0xD4
    out 0x64, al
    call mouse_wait_write
    mov al, bl
    out 0x60, al

    mov cx, 0x8000
.wait_ack:
    in al, 0x64
    test al, 0x01
    jnz .read_ack
    loop .wait_ack
    xor al, al
    jmp .done
.read_ack:
    in al, 0x60
.done:
    pop cx
    pop bx
    ret

; ------------------------------------------------------------
; mouse_irq - IRQ12 handler, builds 3-byte packets
; ------------------------------------------------------------
mouse_irq:
    push ax
    push bx
    push cx
    push dx
    push ds

    mov ax, cs
    mov ds, ax

    in al, 0x60
    mov bl, [mouse_cycle]

    test bl, bl
    jnz .not_first
    test al, 0x08           ; bit 3 must always be set
    jz .finish
    test al, 0xC0           ; reject overflow / stray ACK bytes
    jnz .finish
    mov [mouse_packet], al
    mov byte [mouse_cycle], 1
    jmp .finish

.not_first:
    cmp bl, 1
    jne .third
    mov [mouse_packet + 1], al
    mov byte [mouse_cycle], 2
    jmp .finish

.third:
    mov [mouse_packet + 2], al
    mov byte [mouse_cycle], 0
    call mouse_process

.finish:
    mov al, 0x20
    out 0xA0, al
    out 0x20, al

    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
    iret

; ------------------------------------------------------------
; mouse_process - apply packet deltas and buttons
; ------------------------------------------------------------
mouse_process:
    mov al, [mouse_packet]
    mov bl, al              ; flags byte

    ; Ignore overflow packets (both X and Y)
    test al, 0xC0
    jnz .buttons

    ; X delta (signed, scaled by mouse_speed)
    movsx ax, byte [mouse_packet + 1]
    imul ax, [mouse_speed]
    add [mouse_x], ax

    ; Y delta is positive upwards, screen Y grows downwards
    movsx ax, byte [mouse_packet + 2]
    imul ax, [mouse_speed]
    neg ax
    add [mouse_y], ax

    ; Clamp the logical pointer so the x2 bitmap stays on screen
    mov ax, [UI_W]
    sub ax, 9
    cmp [mouse_x], ax
    jle .x_ok
    mov [mouse_x], ax
.x_ok:
    cmp word [mouse_x], 0
    jge .x_low_ok
    mov word [mouse_x], 0
.x_low_ok:
    mov ax, [UI_H]
    sub ax, 13
    cmp [mouse_y], ax
    jle .y_ok
    mov [mouse_y], ax
.y_ok:
    cmp word [mouse_y], 0
    jge .y_low_ok
    mov word [mouse_y], 0
.y_low_ok:
    mov byte [mouse_moved], 1

.buttons:
    mov al, [mouse_packet]
    and al, 0x01
    mov ah, [mouse_left]
    mov [mouse_left], al
    cmp al, ah
    je .done
    test al, al
    jz .released
    mov byte [mouse_pressed], 1
    jmp .done
.released:
    mov byte [mouse_released], 1
.done:
    ret

; ------------------------------------------------------------
; mouse data
; ------------------------------------------------------------
mouse_x         dw 160
mouse_y         dw 90
mouse_left      db 0
mouse_cycle     db 0
mouse_packet    times 3 db 0
mouse_moved     db 0
mouse_pressed   db 0
mouse_released  db 0
