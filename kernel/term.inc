; ============================================================
;  term.inc - 80x25 terminal buffer, dynamic viewport
;  All buffer copies use DS-relative stores (ES is flat and
;  must not be touched by real-mode string instructions).
; ============================================================

TERM_COLS       equ 80
TERM_ROWS       equ 25
TERM_SIZE       equ TERM_COLS * TERM_ROWS

; ------------------------------------------------------------
; term_init - reset the terminal and show the welcome banner
; ------------------------------------------------------------
term_init:
    pusha
    call term_clear
    mov byte [term_color], 7
    mov si, term_msg_banner
    call term_puts
    mov byte [term_color], 2
    mov si, term_msg_hint
    call term_puts
    mov byte [term_color], 8
    mov si, term_msg_rule
    call term_puts
    call term_prompt
    mov word [term_cmd_len], 0
    popa
    ret

; ------------------------------------------------------------
; term_clear - clear the character/attribute buffers
; ------------------------------------------------------------
term_clear:
    pusha
    mov eax, 0x20202020
    mov di, term_chars
    mov cx, TERM_SIZE / 4
.chars:
    mov [di], eax
    add di, 4
    loop .chars

    mov eax, 0x09090909
    mov di, term_attrs
    mov cx, TERM_SIZE / 4
.attrs:
    mov [di], eax
    add di, 4
    loop .attrs

    mov byte [term_row], 0
    mov byte [term_col], 0
    popa
    ret

; ------------------------------------------------------------
; term_scroll - scroll the terminal buffer up one row
; ------------------------------------------------------------
term_scroll:
    pusha

    mov si, term_chars + TERM_COLS
    mov di, term_chars
    mov cx, ((TERM_ROWS - 1) * TERM_COLS) / 4
    cld
.chars:
    mov eax, [si]
    mov [di], eax
    add si, 4
    add di, 4
    loop .chars

    mov eax, 0x20202020
    mov di, term_chars + (TERM_ROWS - 1) * TERM_COLS
    mov cx, TERM_COLS / 4
.fill_chars:
    mov [di], eax
    add di, 4
    loop .fill_chars

    mov si, term_attrs + TERM_COLS
    mov di, term_attrs
    mov cx, ((TERM_ROWS - 1) * TERM_COLS) / 4
.attrs:
    mov eax, [si]
    mov [di], eax
    add si, 4
    add di, 4
    loop .attrs

    mov eax, 0x09090909
    mov di, term_attrs + (TERM_ROWS - 1) * TERM_COLS
    mov cx, TERM_COLS / 4
.fill_attrs:
    mov [di], eax
    add di, 4
    loop .fill_attrs

    mov byte [term_row], TERM_ROWS - 1
    popa
    ret

; ------------------------------------------------------------
; term_putc - AL = character, colour from term_color
; ------------------------------------------------------------
term_putc:
    pusha
    cmp al, 13
    je .cr
    cmp al, 10
    je .lf
    cmp al, 8
    je .bs

    mov bl, al
    mov al, [term_row]
    xor ah, ah
    mov cx, TERM_COLS
    mul cx
    mov di, ax
    mov al, [term_col]
    xor ah, ah
    add di, ax
    mov al, bl
    mov [term_chars + di], al
    mov al, [term_color]
    mov [term_attrs + di], al

    inc byte [term_col]
    cmp byte [term_col], TERM_COLS
    jb .done
    mov byte [term_col], 0
    inc byte [term_row]
    cmp byte [term_row], TERM_ROWS
    jb .done
    call term_scroll
    jmp .done

.cr:
    mov byte [term_col], 0
    jmp .done
.lf:
    mov byte [term_col], 0
    inc byte [term_row]
    cmp byte [term_row], TERM_ROWS
    jb .done
    call term_scroll
    jmp .done

.bs:
    cmp byte [term_col], 0
    je .done
    dec byte [term_col]
    mov al, [term_row]
    xor ah, ah
    mov cx, TERM_COLS
    mul cx
    mov di, ax
    mov al, [term_col]
    xor ah, ah
    add di, ax
    mov byte [term_chars + di], ' '
    mov al, [term_color]
    mov [term_attrs + di], al

.done:
    mov byte [scene_dirty], 1
    popa
    ret

; ------------------------------------------------------------
; term_puts - SI = ASCIIZ string
; ------------------------------------------------------------
term_puts:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    call term_putc
    jmp .loop
.done:
    popa
    ret

; ------------------------------------------------------------
; term_prompt - print the command prompt
; ------------------------------------------------------------
term_prompt:
    pusha
    mov byte [term_color], 7
    mov si, term_msg_prompt
    call term_puts
    popa
    ret

; ------------------------------------------------------------
; term_key - AL = ASCII key from the keyboard
; ------------------------------------------------------------
term_key:
    cmp al, 8
    je .backspace
    cmp al, 13
    je .enter
    cmp al, 32
    jb .done
    cmp al, 126
    ja .done

    mov bx, [term_cmd_len]
    cmp bx, 60
    jae .done
    cmp al, 'a'
    jb .store
    cmp al, 'z'
    ja .store
    sub al, 32
.store:
    mov bx, [term_cmd_len]
    mov [term_cmd + bx], al
    inc word [term_cmd_len]
    mov byte [term_color], 1
    call term_putc
.done:
    ret

.backspace:
    cmp word [term_cmd_len], 0
    je .done
    dec word [term_cmd_len]
    mov al, 8
    call term_putc
    jmp .done

.enter:
    jmp term_enter

; ------------------------------------------------------------
; term_enter - execute the current command line
; ------------------------------------------------------------
term_enter:
    pusha
    mov bx, [term_cmd_len]
    mov byte [term_cmd + bx], 0
    mov al, 10
    call term_putc
    mov si, term_cmd
    call term_execute
    call term_prompt
    mov word [term_cmd_len], 0
    popa
    ret

; ------------------------------------------------------------
; term_execute - run the command in term_cmd
; ------------------------------------------------------------
term_execute:
    pusha
    cmp byte [term_cmd], 0
    je .done

    ; ECHO accepts an argument, so match its 4-byte prefix first
    cmp word [term_cmd], 'EC'
    jne .not_echo_cmd
    cmp word [term_cmd + 2], 'HO'
    jne .not_echo_cmd
    mov al, [term_cmd + 4]
    cmp al, 0
    je .echo
    cmp al, ' '
    je .echo
.not_echo_cmd:

    ; GSOD [code] accepts an optional decimal code
    cmp word [term_cmd], 'GS'
    jne .not_gsod_cmd
    cmp word [term_cmd + 2], 'OD'
    jne .not_gsod_cmd
    mov al, [term_cmd + 4]
    test al, al
    jz .gsod
    cmp al, ' '
    je .gsod
.not_gsod_cmd:

    mov si, term_cmd
    mov di, term_cmd_help
    call term_streq
    jc .help
    mov di, term_cmd_ver
    call term_streq
    jc .ver
    mov di, term_cmd_dir
    call term_streq
    jc .dir
    mov di, term_cmd_cls
    call term_streq
    jc .cls
    mov di, term_cmd_echo
    call term_streq
    jc .echo
    mov di, term_cmd_mem
    call term_streq
    jc .mem
    mov di, term_cmd_time
    call term_streq
    jc .time
    mov di, term_cmd_date
    call term_streq
    jc .date
    mov di, term_cmd_about
    call term_streq
    jc .about
    mov di, term_cmd_mode
    call term_streq
    jc .mode
    mov di, term_cmd_settings
    call term_streq
    jc .settings
    mov di, term_cmd_reboot
    call term_streq
    jc .reboot
    mov di, term_cmd_poweroff
    call term_streq
    jc .poweroff
    mov di, term_cmd_halt
    call term_streq
    jc .halt
    mov di, term_cmd_exit
    call term_streq
    jc .exit

    mov byte [term_color], 13
    mov si, term_msg_bad
    call term_puts
    jmp .done

.help:
    mov byte [term_color], 2
    mov si, term_help_text
    call term_puts
    jmp .done
.ver:
    mov byte [term_color], 2
    mov si, term_ver_text
    call term_puts
    jmp .done
.dir:
    mov byte [term_color], 2
    mov si, term_dir_text
    call term_puts
    jmp .done
.cls:
    call term_clear
    jmp .done
.echo:
    mov si, term_cmd + 4
.skip_space:
    cmp byte [si], ' '
    jne .echo_out
    inc si
    jmp .skip_space
.echo_out:
    mov byte [term_color], 9
    call term_puts
    mov al, 10
    call term_putc
    jmp .done
.mem:
    mov byte [term_color], 2
    mov si, term_msg_conv
    call term_puts
    mov ax, [mem_conv_kb]
    call term_print_dec16
    mov si, term_msg_kb
    call term_puts

    mov si, term_msg_ext
    call term_puts
    mov ax, [mem_ext_kb]
    test ax, ax
    jnz .mem_ext_ok
    mov si, term_msg_none
    call term_puts
    jmp .done
.mem_ext_ok:
    call term_print_dec16
    mov si, term_msg_kb
    call term_puts
    jmp .done
.time:
    mov byte [term_color], 2
    mov si, term_msg_time
    call term_puts
    mov si, clock_time
    call term_puts
    mov al, 10
    call term_putc
    jmp .done
.date:
    mov byte [term_color], 2
    mov si, term_msg_date
    call term_puts
    mov si, clock_date
    call term_puts
    mov al, 10
    call term_putc
    jmp .done
.about:
    mov byte [term_color], 2
    mov si, term_about_text
    call term_puts
    jmp .done
.mode:
    mov byte [term_color], 2
    mov si, term_msg_mode
    call term_puts
    mov si, mode_text
    call term_puts
    mov al, 10
    call term_putc
    jmp .done
.settings:
    call open_settings
    jmp .done
.reboot:
    mov byte [term_color], 11
    mov si, term_msg_reboot
    call term_puts
    call system_reboot
    jmp .done
.poweroff:
    mov byte [term_color], 11
    mov si, term_msg_poweroff
    call term_puts
    call system_poweroff
    jmp .done
.halt:
    mov byte [term_color], 13
    mov si, term_msg_halt
    call term_puts
    cli
.power_halt:
    hlt
    jmp .power_halt
.gsod:
    ; default demo code
    xor bx, bx
    mov si, term_cmd + 4
.gsod_skip:
    cmp byte [si], ' '
    jne .gsod_parse
    inc si
    jmp .gsod_skip
.gsod_parse:
    mov al, [si]
    cmp al, '0'
    jb .gsod_ready
    cmp al, '9'
    ja .gsod_ready
    sub al, '0'
    xor ah, ah
    push ax
    mov ax, bx
    mov cx, 10
    mul cx
    mov bx, ax
    pop ax
    add bx, ax
    inc si
    jmp .gsod_parse
.gsod_ready:
    test bx, bx
    jnz .gsod_launch
    mov bx, ERR_CHECKSUM
.gsod_launch:
    mov ax, bx
    jmp gsod
.exit:
    mov bx, windows + WIN_TERM * W_SIZE
    mov byte [bx + W_STATE], 0
    mov byte [scene_dirty], 1
    jmp .done

.done:
    popa
    ret

; ------------------------------------------------------------
; term_streq - SI and DI ASCIIZ, CF=1 if equal
; ------------------------------------------------------------
term_streq:
    push ax
    push si
    push di
.loop:
    mov al, [si]
    cmp al, [di]
    jne .no
    test al, al
    jz .yes
    inc si
    inc di
    jmp .loop
.yes:
    pop di
    pop si
    pop ax
    stc
    ret
.no:
    pop di
    pop si
    pop ax
    clc
    ret

; ------------------------------------------------------------
; term_print_dec16 - AX = unsigned decimal
; ------------------------------------------------------------
term_print_dec16:
    pusha
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
    call term_putc
    loop .output
    popa
    ret

; ------------------------------------------------------------
; term_draw - AX=client x, BX=client y, CX=client w, DX=client h
; Draws the visible part of the 80x25 buffer.
; ------------------------------------------------------------
term_draw:
    pusha
    mov [td_x], ax
    mov [td_y], bx

    mov ax, cx
    shr ax, 3
    test ax, ax
    jz .done
    cmp ax, TERM_COLS
    jbe .cols_ok
    mov ax, TERM_COLS
.cols_ok:
    mov [td_cols], ax

    mov ax, dx
    shr ax, 3
    test ax, ax
    jz .done
    cmp ax, TERM_ROWS
    jbe .rows_ok
    mov ax, TERM_ROWS
.rows_ok:
    mov [td_rows], ax

    ; vertical window follows the bottom of the buffer:
    ; start_row = max(0, term_row + 1 - rows)
    xor ax, ax
    mov al, [term_row]
    inc ax
    sub ax, [td_rows]
    jnc .row_ok
    xor ax, ax
.row_ok:
    mov [td_start_row], ax

    ; horizontal window keeps the cursor visible:
    ; start_col = max(0, term_col + 1 - cols)
    xor ax, ax
    mov al, [term_col]
    inc ax
    sub ax, [td_cols]
    jnc .col_ok
    xor ax, ax
.col_ok:
    mov [td_start_col], ax

    xor bp, bp
.row:
    mov word [td_c], 0
.col:
    mov ax, bp
    add ax, [td_start_row]
    mov cx, TERM_COLS
    mul cx
    add ax, [td_c]
    add ax, [td_start_col]
    mov si, ax

    mov al, [term_chars + si]
    cmp al, ' '
    je .next
    mov [td_char], al
    mov al, [term_attrs + si]
    mov [td_color], al

    mov ax, [td_c]
    shl ax, 3
    add ax, [td_x]
    mov [td_cx], ax
    mov ax, bp
    shl ax, 3
    add ax, [td_y]
    mov [td_cy], ax

    mov al, [td_char]
    mov bx, [td_cx]
    mov cx, [td_cy]
    mov dl, [td_color]
    call draw_char

.next:
    inc word [td_c]
    mov ax, [td_c]
    cmp ax, [td_cols]
    jb .col
    inc bp
    cmp bp, [td_rows]
    jb .row

    ; cursor underline
    cmp byte [term_blink], 0
    je .done

    xor ax, ax
    mov al, [term_col]
    sub ax, [td_start_col]
    cmp ax, [td_cols]
    jae .done
    shl ax, 3
    add ax, [td_x]
    mov [td_cx], ax

    xor ax, ax
    mov al, [term_row]
    sub ax, [td_start_row]
    cmp ax, [td_rows]
    jae .done
    shl ax, 3
    add ax, [td_y]
    add ax, 6
    mov bx, ax

    mov ax, [td_cx]
    mov cx, 6
    mov dx, 2
    mov si, 1
    call fill_rect

.done:
    popa
    ret

; ------------------------------------------------------------
; terminal data
; ------------------------------------------------------------
term_chars      times TERM_SIZE db ' '
term_attrs      times TERM_SIZE db 9
term_row        db 0
term_col        db 0
term_color      db 9
term_blink      db 1
term_cmd        times 64 db 0
term_cmd_len    dw 0
td_x            dw 0
td_y            dw 0
td_cols         dw 0
td_rows         dw 0
td_start_row    dw 0
td_start_col    dw 0
td_c            dw 0
td_char         db 0
td_color        db 0
td_cx           dw 0
td_cy           dw 0

term_cmd_help       db "HELP", 0
term_cmd_ver        db "VER", 0
term_cmd_dir        db "DIR", 0
term_cmd_cls        db "CLS", 0
term_cmd_echo       db "ECHO", 0
term_cmd_mem        db "MEM", 0
term_cmd_time       db "TIME", 0
term_cmd_date       db "DATE", 0
term_cmd_about      db "ABOUT", 0
term_cmd_mode       db "MODE", 0
term_cmd_settings   db "SETTINGS", 0
term_cmd_reboot     db "REBOOT", 0
term_cmd_poweroff   db "POWEROFF", 0
term_cmd_halt       db "HALT", 0
term_cmd_exit       db "EXIT", 0

term_msg_banner     db "LiMaxOS Terminal v1.1", 13, 10, 0
term_msg_hint       db "Type HELP. L menu has Settings.", 13, 10, 0
term_msg_rule       db "---------------------------------", 13, 10, 0
term_msg_prompt     db "C:\> ", 0
term_msg_bad        db "Bad command or file name", 13, 10, 0

term_help_text:
    db "Commands:", 13, 10
    db "HELP VER DIR CLS ECHO MEM MODE", 13, 10
    db "TIME DATE ABOUT SETTINGS EXIT", 13, 10
    db "GSOD [code] REBOOT POWEROFF HALT", 13, 10, 0

term_ver_text:
    db "LiMaxOS 1.1 GUI edition", 13, 10
    db "16-bit real mode, VBE LFB", 13, 10
    db "Dynamic desktop resolution", 13, 10
    db "PS/2 mouse, NASM two-stage boot", 13, 10, 0

term_dir_text:
    db " Volume in drive C is LIMAXOS", 13, 10
    db " Directory of C:\", 13, 10, 13, 10
    db "COMMAND  COM      4096", 13, 10
    db "KERNEL   SYS     24576", 13, 10
    db "README   TXT      1024", 13, 10
    db "AUTOEXEC BAT       128", 13, 10
    db "4 file(s)        29824 bytes", 13, 10
    db "512000 bytes free", 13, 10, 0

term_about_text:
    db "LiMaxOS 1.1 GUI edition", 13, 10
    db "A tiny 16-bit real-mode OS.", 13, 10
    db "Desktop, windows, taskbar,", 13, 10
    db "VBE graphics and PS/2 mouse.", 13, 10
    db "(c) 2026 LiMax Systems", 13, 10, 0

term_msg_conv       db "Conventional: ", 0
term_msg_ext        db "Extended:     ", 0
term_msg_kb         db " KB", 13, 10, 0
term_msg_none       db "n/a", 13, 10, 0
term_msg_time       db "Time: ", 0
term_msg_date       db "Date: ", 0
term_msg_mode       db "Display: ", 0
term_msg_reboot     db "Rebooting LiMaxOS...", 13, 10, 0
term_msg_poweroff   db "Powering off...", 13, 10, 0
term_msg_halt       db "CPU halted. Reset the machine.", 13, 10, 0
