; ============================================================
;  rtc.inc - CMOS real-time clock, no BIOS calls
; ============================================================

; ------------------------------------------------------------
; rtc_read_reg - AL = CMOS register -> AL = value
; ------------------------------------------------------------
rtc_read_reg:
    or al, 0x80             ; keep NMI disabled while reading
    out 0x70, al
    in al, 0x71
    ret

; ------------------------------------------------------------
; rtc_wait_uip - wait until the update-in-progress flag clears
; ------------------------------------------------------------
rtc_wait_uip:
    push ax
    push cx
    mov cx, 0xFFFF
.loop:
    mov al, 0x0A
    call rtc_read_reg
    test al, 0x80
    jz .done
    loop .loop
.done:
    pop cx
    pop ax
    ret

; ------------------------------------------------------------
; rtc_to_bin - AL (BCD or binary) -> AX (0..255)
; ------------------------------------------------------------
rtc_to_bin:
    test byte [rtc_statb], 0x04
    jnz .binary
    push bx
    mov bl, al
    shr al, 4
    and al, 0x0F
    mov bh, al
    mov al, bl
    and al, 0x0F
    mov bl, al
    mov al, bh
    mov ah, 10
    mul ah
    add al, bl
    adc ah, 0
    pop bx
    ret
.binary:
    xor ah, ah
    ret

; ------------------------------------------------------------
; rtc_update - refresh clock_time / clock_date
; CF=1 when the displayed strings changed.
; ------------------------------------------------------------
rtc_update:
    pusha

    mov al, 0x0B
    call rtc_read_reg
    mov [rtc_statb], al

    mov cx, 3
.retry:
    call rtc_wait_uip

    mov al, 0x00
    call rtc_read_reg
    mov [rtc_sec], al
    mov al, 0x02
    call rtc_read_reg
    mov [rtc_min], al
    mov al, 0x04
    call rtc_read_reg
    mov [rtc_hour], al
    mov al, 0x07
    call rtc_read_reg
    mov [rtc_day], al
    mov al, 0x08
    call rtc_read_reg
    mov [rtc_mon], al
    mov al, 0x09
    call rtc_read_reg
    mov [rtc_year], al

    call rtc_wait_uip
    mov al, 0x00
    call rtc_read_reg
    cmp al, [rtc_sec]
    je .stable
    loop .retry

.stable:
    ; minutes
    mov al, [rtc_min]
    call rtc_to_bin
    mov [rtc_new_mm], al

    ; hours, with 12/24-hour conversion
    mov al, [rtc_hour]
    test byte [rtc_statb], 0x02
    jnz .hour24
    mov bl, al
    and bl, 0x80            ; PM flag
    and al, 0x7F
    call rtc_to_bin
    cmp ax, 12
    jne .hour_not_12
    test bl, bl
    jz .hour_zero
    mov ax, 12
    jmp .hour_store
.hour_zero:
    xor ax, ax
    jmp .hour_store
.hour_not_12:
    test bl, bl
    jz .hour_store
    add ax, 12
    jmp .hour_store
.hour24:
    call rtc_to_bin
.hour_store:
    mov [rtc_new_hh], al

    ; date
    mov al, [rtc_day]
    call rtc_to_bin
    mov [rtc_new_dd], al
    mov al, [rtc_mon]
    call rtc_to_bin
    mov [rtc_new_mo], al
    mov al, [rtc_year]
    call rtc_to_bin
    mov [rtc_new_yy], al

    ; compare with what the taskbar currently shows
    mov al, [rtc_new_hh]
    cmp al, [cur_hh]
    jne .changed
    mov al, [rtc_new_mm]
    cmp al, [cur_mm]
    jne .changed
    mov al, [rtc_new_dd]
    cmp al, [cur_dd]
    jne .changed
    mov al, [rtc_new_mo]
    cmp al, [cur_mo]
    jne .changed
    mov al, [rtc_new_yy]
    cmp al, [cur_yy]
    jne .changed
    clc
    jmp .done

.changed:
    mov al, [rtc_new_hh]
    mov [cur_hh], al
    mov al, [rtc_new_mm]
    mov [cur_mm], al
    mov al, [rtc_new_dd]
    mov [cur_dd], al
    mov al, [rtc_new_mo]
    mov [cur_mo], al
    mov al, [rtc_new_yy]
    mov [cur_yy], al
    call rtc_build_strings
    stc
.done:
    popa
    ret

; ------------------------------------------------------------
; rtc_build_strings - fill clock_time and clock_date
; ------------------------------------------------------------
rtc_build_strings:
    pusha

    mov di, clock_time
    mov al, [cur_hh]
    call rtc_put2
    mov byte [di], ':'
    inc di
    mov al, [cur_mm]
    call rtc_put2
    mov byte [di], 0

    mov di, clock_date
    mov al, [cur_dd]
    call rtc_put2
    mov byte [di], '.'
    inc di
    mov al, [cur_mo]
    call rtc_put2
    mov byte [di], '.'
    inc di
    mov byte [di], '2'
    inc di
    mov byte [di], '0'
    inc di
    mov al, [cur_yy]
    call rtc_put2
    mov byte [di], 0

    popa
    ret

; ------------------------------------------------------------
; rtc_put2 - AL=0..99 -> two decimal digits at DI, DI advances
; ------------------------------------------------------------
rtc_put2:
    push ax
    push bx
    xor ah, ah
    mov bl, 10
    div bl
    add al, '0'
    mov [di], al
    inc di
    mov al, ah
    add al, '0'
    mov [di], al
    inc di
    pop bx
    pop ax
    ret

; ------------------------------------------------------------
; RTC / clock data
; ------------------------------------------------------------
rtc_statb   db 0
rtc_sec     db 0
rtc_min     db 0
rtc_hour    db 0
rtc_day     db 0
rtc_mon     db 0
rtc_year    db 0
rtc_new_hh  db 0
rtc_new_mm  db 0
rtc_new_dd  db 0
rtc_new_mo  db 0
rtc_new_yy  db 0
cur_hh      db 0xFF
cur_mm      db 0xFF
cur_dd      db 0xFF
cur_mo      db 0xFF
cur_yy      db 0xFF
clock_time  times 8 db 0
clock_date  times 14 db 0
