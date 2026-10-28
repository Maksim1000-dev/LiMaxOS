; ============================================================
;  irq.inc - PIC remap, PIT timer, PS/2 keyboard IRQ
;  All handlers run after gfx_enter_unreal.  ES/FS/GS are flat
;  and must never be reloaded here.
; ============================================================

KBD_BUF_SIZE    equ 64

; ------------------------------------------------------------
; irq_remap - move IRQs to 0x20..0x2F and install vectors
; ------------------------------------------------------------
irq_remap:
    pusha
    cli

    ; ICW1: edge triggered, cascade, ICW4 needed
    mov al, 0x11
    out 0x20, al
    out 0xA0, al
    call irq_io_wait

    ; ICW2: vector bases
    mov al, 0x20
    out 0x21, al
    mov al, 0x28
    out 0xA1, al
    call irq_io_wait

    ; ICW3: slave on IRQ2
    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xA1, al
    call irq_io_wait

    ; ICW4: 8086 mode
    mov al, 0x01
    out 0x21, al
    out 0xA1, al
    call irq_io_wait

    ; mask everything for now
    mov al, 0xFF
    out 0x21, al
    out 0xA1, al

    ; install vectors through the flat FS selector
    mov ax, cs

    mov word [fs:0x20*4], timer_irq
    mov word [fs:0x20*4+2], ax
    mov word [fs:0x21*4], kbd_irq
    mov word [fs:0x21*4+2], ax
    mov word [fs:0x2C*4], mouse_irq
    mov word [fs:0x2C*4+2], ax

    ; PIT channel 0: mode 3, divisor 11932 -> 100 Hz
    mov al, 0x36
    out 0x43, al
    mov ax, 11932
    out 0x40, al
    mov al, ah
    out 0x40, al

    mov dword [tick_count], 0
    popa
    ret

; ------------------------------------------------------------
; irq_enable - unmask timer, keyboard, cascade, mouse
; ------------------------------------------------------------
irq_enable:
    push ax
    in al, 0x21
    and al, 0xF8            ; IRQ0, IRQ1, IRQ2
    out 0x21, al
    in al, 0xA1
    and al, 0xEF            ; IRQ12
    out 0xA1, al
    pop ax
    ret

; ------------------------------------------------------------
; irq_io_wait - short delay between PIC commands
; ------------------------------------------------------------
irq_io_wait:
    push ax
    in al, 0x80
    pop ax
    ret

; ------------------------------------------------------------
; timer_irq - IRQ0 at 100 Hz
; ------------------------------------------------------------
timer_irq:
    push ax
    push ds
    mov ax, cs
    mov ds, ax
    inc dword [tick_count]
    mov al, 0x20
    out 0x20, al
    pop ds
    pop ax
    iret

; ------------------------------------------------------------
; kbd_init - controller config, IRQ1 enabled
; ------------------------------------------------------------
kbd_init:
    pusha
    call kbd_flush

    call mouse_wait_write
    mov al, 0x20
    out 0x64, al
    call mouse_wait_read
    in al, 0x60

    or al, 0x41             ; keyboard IRQ + translation
    and al, 0xEF            ; keyboard clock enabled
    mov bl, al

    call mouse_wait_write
    mov al, 0x60
    out 0x64, al
    call mouse_wait_write
    mov al, bl
    out 0x60, al

    mov byte [kbd_shift], 0
    mov byte [kbd_caps], 0
    mov byte [kbd_ext], 0
    mov word [kbd_head], 0
    mov word [kbd_tail], 0
    popa
    ret

; ------------------------------------------------------------
; kbd_flush - drain the controller output buffer
; ------------------------------------------------------------
kbd_flush:
    pusha
    mov cx, 128
.loop:
    in al, 0x64
    test al, 0x01
    jz .done
    in al, 0x60
    loop .loop
.done:
    popa
    ret

; ------------------------------------------------------------
; kbd_irq - IRQ1 scancode decoder
; ------------------------------------------------------------
kbd_irq:
    pusha
    push ds
    mov ax, cs
    mov ds, ax
    cld

    in al, 0x60
    cmp al, 0xE0
    je .extended

    mov bl, [kbd_ext]
    mov byte [kbd_ext], 0
    test bl, bl
    jnz .ext_key

    test al, 0x80
    jnz .release

    cmp al, 0x2A
    je .shift_on
    cmp al, 0x36
    je .shift_on
    cmp al, 0x3A
    je .caps
    cmp al, 0x1D
    je .eoi
    cmp al, 0x38
    je .eoi

    mov bl, [kbd_shift]
    test bl, bl
    jnz .shifted

    movzx bx, al
    mov al, [cs:kbd_map_normal + bx]
    jmp .apply_caps
.shifted:
    movzx bx, al
    mov al, [cs:kbd_map_shift + bx]

.apply_caps:
    test al, al
    jz .eoi
    cmp byte [kbd_caps], 0
    je .push
    cmp al, 'a'
    jb .push
    cmp al, 'z'
    ja .caps_upper
    sub al, 32
    jmp .push
.caps_upper:
    cmp al, 'A'
    jb .push
    cmp al, 'Z'
    ja .push
    add al, 32
.push:
    call kbd_push
    jmp .eoi

.shift_on:
    mov byte [kbd_shift], 1
    jmp .eoi
.caps:
    xor byte [kbd_caps], 1
    jmp .eoi

.release:
    mov byte [kbd_ext], 0
    cmp al, 0xAA
    je .shift_off
    cmp al, 0xB6
    je .shift_off
    jmp .eoi
.shift_off:
    mov byte [kbd_shift], 0
    jmp .eoi

.extended:
    mov byte [kbd_ext], 1
    jmp .eoi

.ext_key:
    test al, 0x80
    jnz .eoi
    cmp al, 0x48
    je .ext_up
    cmp al, 0x50
    je .ext_down
    cmp al, 0x4B
    je .ext_left
    cmp al, 0x4D
    je .ext_right
    cmp al, 0x47
    je .ext_home
    cmp al, 0x4F
    je .ext_end
    cmp al, 0x53
    je .ext_del
    jmp .eoi
.ext_up:
    mov al, 0x80
    jmp .ext_push
.ext_down:
    mov al, 0x81
    jmp .ext_push
.ext_left:
    mov al, 0x82
    jmp .ext_push
.ext_right:
    mov al, 0x83
    jmp .ext_push
.ext_home:
    mov al, 0x84
    jmp .ext_push
.ext_end:
    mov al, 0x85
    jmp .ext_push
.ext_del:
    mov al, 0x86
.ext_push:
    call kbd_push
    jmp .eoi

.eoi:
    mov al, 0x20
    out 0x20, al
    pop ds
    popa
    iret

; ------------------------------------------------------------
; kbd_push - AL = character, drop on a full buffer
; ------------------------------------------------------------
kbd_push:
    push ax
    push bx
    push di
    mov bx, [kbd_head]
    mov di, bx
    inc bx
    and bx, KBD_BUF_SIZE-1
    cmp bx, [kbd_tail]
    je .full
    mov [kbd_buf + di], al
    mov [kbd_head], bx
.full:
    pop di
    pop bx
    pop ax
    ret

; ------------------------------------------------------------
; kbd_get - CF=1 and AL=character when a key is available
; ------------------------------------------------------------
kbd_get:
    push bx
    mov bx, [kbd_tail]
    cmp bx, [kbd_head]
    je .empty
    mov al, [kbd_buf + bx]
    inc bx
    and bx, KBD_BUF_SIZE-1
    mov [kbd_tail], bx
    pop bx
    stc
    ret
.empty:
    pop bx
    clc
    ret

; ------------------------------------------------------------
; keyboard data
; ------------------------------------------------------------
kbd_head    dw 0
kbd_tail    dw 0
kbd_shift   db 0
kbd_caps    db 0
kbd_ext     db 0
kbd_buf     times KBD_BUF_SIZE db 0
tick_count  dd 0

; ------------------------------------------------------------
; scancode -> ASCII (set 1), 128 entries each
; 0x80..0x86 are internal codes: up/down/left/right/home/end/delete
; ------------------------------------------------------------
kbd_map_normal:
    db 0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', 8, 9
    db 'q','w','e','r','t','y','u','i','o','p','[',']',13,  0,'a','s'
    db 'd','f','g','h','j','k','l',';',39,'`',  0, 92,'z','x','c','v'
    db 'b','n','m',',','.','/',  0,'*',  0,' ',  0,  0,  0,  0,  0,  0
    db   0,  0,  0,  0,  0,  0,  0,0x84,0x80,  0,  0,0x82,  0,0x83,  0,0x85
    db 0x81, 0,0x86,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
    times 32 db 0

kbd_map_shift:
    db 0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', 8, 9
    db 'Q','W','E','R','T','Y','U','I','O','P','{','}',13,  0,'A','S'
    db 'D','F','G','H','J','K','L',':',34,'~',  0,'|','Z','X','C','V'
    db 'B','N','M','<','>','?',  0,'*',  0,' ',  0,  0,  0,  0,  0,  0
    db   0,  0,  0,  0,  0,  0,  0,0x84,0x80,  0,  0,0x82,  0,0x83,  0,0x85
    db 0x81, 0,0x86,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
    times 32 db 0
