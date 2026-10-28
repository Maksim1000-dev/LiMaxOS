; ============================================================
;  settings.inc - SETTINGS window (DISPLAY panel)
;
;  Presets: 0 AUTO, 1 640x360, 2 1280x720, 3 1920x1080,
;           4 2560x1440
;  AUTO follows the QEMU window size reported through COM1.
;  A manual preset locks the auto mode until AUTO is selected.
; ============================================================

SE_SIDEBAR      equ 76
SE_ROW_X        equ 82
SE_ROW_Y        equ 24
SE_ROW_H        equ 17
SE_ROW_BOX_H    equ 15
SE_MAX_PRESET   equ 4

; ------------------------------------------------------------
; settings_draw - AX=x BX=y CX=w DX=h (client area)
; ------------------------------------------------------------
settings_draw:
    pusha
    mov [se_x], ax
    mov [se_y], bx
    mov [se_w], cx
    mov [se_h], dx

    ; client background
    xor si, si
    call fill_rect

    ; left sidebar
    mov ax, [se_x]
    mov bx, [se_y]
    mov cx, SE_SIDEBAR
    mov dx, [se_h]
    mov si, 16
    call fill_rect

    mov ax, [se_x]
    add ax, SE_SIDEBAR
    mov bx, [se_y]
    mov cx, 1
    mov dx, [se_h]
    mov si, 7
    call fill_rect

    ; sidebar heading
    mov bx, [se_x]
    add bx, 6
    mov cx, [se_y]
    add cx, 6
    mov dl, 11
    mov si, se_msg_settings
    call draw_text

    ; DISPLAY button (this panel is always active)
    mov ax, [se_x]
    add ax, 4
    mov bx, [se_y]
    add bx, 22
    mov cx, SE_SIDEBAR - 8
    mov dx, 18
    mov si, 6
    call fill_rect

    mov ax, [se_x]
    add ax, 4
    mov bx, [se_y]
    add bx, 22
    mov cx, SE_SIDEBAR - 8
    mov dx, 18
    mov si, 1
    call frame_rect

    mov bx, [se_x]
    add bx, 9
    mov cx, [se_y]
    add cx, 27
    mov dl, 1
    mov si, se_msg_display
    call draw_text

    ; right panel heading
    mov bx, [se_x]
    add bx, SE_ROW_X
    mov cx, [se_y]
    add cx, 6
    mov dl, 11
    mov si, se_msg_display
    call draw_text

    ; current physical mode, right aligned
    mov si, mode_text
    call text_width
    mov bx, [se_x]
    add bx, [se_w]
    sub bx, 6
    sub bx, ax
    mov cx, [se_y]
    add cx, 6
    mov dl, 2
    call draw_text

    ; preset rows
    mov word [se_row], 0
.row_loop:
    mov ax, [se_row]
    imul ax, SE_ROW_H
    add ax, [se_y]
    add ax, SE_ROW_Y
    mov [se_rowy], ax

    mov al, [se_row]
    cmp al, [display_preset]
    je .selected
    cmp al, [set_hover]
    je .hovered
    mov si, 4
    mov dl, 2
    jmp .paint
.selected:
    mov si, 6
    mov dl, 1
    jmp .paint
.hovered:
    mov si, 5
    mov dl, 7

.paint:
    mov [se_text_color], dl
    mov ax, [se_x]
    add ax, SE_ROW_X
    mov bx, [se_rowy]
    mov cx, [se_w]
    sub cx, SE_ROW_X + 6
    mov dx, SE_ROW_BOX_H
    call fill_rect

    ; radio marker
    mov ax, [se_x]
    add ax, SE_ROW_X + 4
    mov bx, [se_rowy]
    add bx, 4
    mov cx, 7
    mov dx, 7
    mov si, 7
    call frame_rect

    mov al, [se_row]
    cmp al, [display_preset]
    jne .no_dot
    mov ax, [se_x]
    add ax, SE_ROW_X + 6
    mov bx, [se_rowy]
    add bx, 6
    mov cx, 3
    mov dx, 3
    mov si, 9
    call fill_rect
.no_dot:

    mov al, [se_row]
    call se_row_label
    mov bx, [se_x]
    add bx, SE_ROW_X + 15
    mov cx, [se_rowy]
    add cx, 4
    mov dl, [se_text_color]
    call draw_text

    inc word [se_row]
    cmp word [se_row], SE_MAX_PRESET + 1
    jb .row_loop

    ; status footer
    cmp byte [display_locked], 0
    je .auto_state
    mov si, se_msg_manual
    mov dl, 12
    jmp .footer
.auto_state:
    cmp byte [auto_valid], 0
    je .waiting
    mov si, se_msg_linked
    mov dl, 9
    jmp .footer
.waiting:
    mov si, se_msg_waiting
    mov dl, 8
.footer:
    mov bx, [se_x]
    add bx, SE_ROW_X
    mov cx, [se_y]
    add cx, [se_h]
    sub cx, 11
    call draw_text

    popa
    ret

; ------------------------------------------------------------
; se_row_label - AL=index -> SI=label
; ------------------------------------------------------------
se_row_label:
    cmp al, 0
    je .auto
    cmp al, 1
    je .r640
    cmp al, 2
    je .r1280
    cmp al, 3
    je .r1920
    mov si, se_msg_r2560
    ret
.auto:
    mov si, se_msg_auto
    ret
.r640:
    mov si, se_msg_r640
    ret
.r1280:
    mov si, se_msg_r1280
    ret
.r1920:
    mov si, se_msg_r1920
    ret

; ------------------------------------------------------------
; settings_hover_at - AX=x BX=y relative to client -> AL=row/0xFF
; ------------------------------------------------------------
settings_hover_at:
    cmp ax, SE_ROW_X
    jb .none
    mov cx, [se_w]
    sub cx, SE_ROW_X + 6
    cmp ax, cx
    jae .none
    cmp bx, SE_ROW_Y
    jb .none
    sub bx, SE_ROW_Y
    mov ax, bx
    xor dx, dx
    mov cx, SE_ROW_H
    div cx
    cmp ax, SE_MAX_PRESET
    ja .none
    ret
.none:
    mov al, 0xFF
    ret

; ------------------------------------------------------------
; settings_click - AX=x BX=y relative to client
; ------------------------------------------------------------
settings_click:
    pusha
    call settings_hover_at
    cmp al, 0xFF
    je .done
    call display_apply_preset
    mov byte [scene_dirty], 1
.done:
    popa
    ret

; ------------------------------------------------------------
; open_settings - show, restore or focus the settings window
; ------------------------------------------------------------
open_settings:
    mov bx, windows + WIN_SET * W_SIZE
    cmp byte [bx + W_STATE], 0
    jne .visible
    mov byte [bx + W_STATE], 1
    mov word [bx + W_W], 288
    mov word [bx + W_H], 150

    mov ax, [UI_W]
    sub ax, 288
    jns .x_ok
    xor ax, ax
.x_ok:
    shr ax, 1
    cmp ax, 4
    jae .x_store
    mov ax, 4
.x_store:
    mov [bx + W_X], ax

    mov ax, [taskbar_y]
    sub ax, 150
    jns .y_ok
    xor ax, ax
.y_ok:
    shr ax, 1
    cmp ax, 4
    jae .y_store
    mov ax, 4
.y_store:
    mov [bx + W_Y], ax

    mov ax, [bx + W_X]
    mov [bx + W_SX], ax
    mov ax, [bx + W_Y]
    mov [bx + W_SY], ax
    mov ax, [bx + W_W]
    mov [bx + W_SW], ax
    mov ax, [bx + W_H]
    mov [bx + W_SH], ax
    call ui_clamp_windows
    jmp .focus
.visible:
    cmp byte [bx + W_STATE], 2
    jne .focus
    mov byte [bx + W_STATE], 1
.focus:
    mov byte [z_top], WIN_SET
    mov byte [scene_dirty], 1
    ret

; ------------------------------------------------------------
; display_apply_preset - AL = 0..4
; ------------------------------------------------------------
display_apply_preset:
    pusha
    and al, 0x0F
    cmp al, SE_MAX_PRESET
    jbe .ok
    xor al, al
.ok:
    mov [display_preset], al
    test al, al
    jz .auto

    mov byte [display_locked], 1
    call serial_send_lock

    movzx bx, byte [display_preset]
    shl bx, 1
    mov ax, [display_wtab + bx]
    mov dx, [display_htab + bx]
    mov bx, dx
    call display_apply_size
    jmp .done

.auto:
    mov byte [display_locked], 0
    call serial_send_auto
    cmp byte [auto_valid], 0
    je .done
    mov ax, [auto_w]
    mov bx, [auto_h]
    call display_apply_size

.done:
    mov byte [scene_dirty], 1
    popa
    ret

; ------------------------------------------------------------
; display_apply_auto - AX=width BX=height from the QEMU helper
; Ignored while a manual preset is locked.
; ------------------------------------------------------------
display_apply_auto:
    pusha
    cmp byte [display_locked], 0
    jne .done
    cmp ax, 640
    jb .done
    cmp bx, 360
    jb .done

    mov [auto_w], ax
    mov [auto_h], bx
    mov byte [auto_valid], 1
    mov byte [display_preset], 0
    call display_apply_size
.done:
    popa
    ret

; ------------------------------------------------------------
; display_auto_select - re-apply the last reported window size
; ------------------------------------------------------------
display_auto_select:
    cmp byte [auto_valid], 0
    je .done
    mov ax, [auto_w]
    mov bx, [auto_h]
    jmp display_apply_size
.done:
    ret

; ------------------------------------------------------------
; display_apply_size - AX=width BX=height, switch the real VBE mode
; ------------------------------------------------------------
display_apply_size:
    pusha
    call gfx_set_mode
    call ui_clamp_windows
    mov byte [scene_dirty], 1
    call serial_send_mode
    popa
    ret

; ------------------------------------------------------------
; settings data
; ------------------------------------------------------------
display_locked  db 0
display_preset  db 0
auto_valid      db 0
auto_w          dw 0
auto_h          dw 0
se_x            dw 0
se_y            dw 0
se_w            dw 288
se_h            dw 150
se_row          dw 0
se_rowy         dw 0
se_text_color   db 0

display_wtab:
    dw 0, 640, 1280, 1920, 2560
display_htab:
    dw 0, 360, 720, 1080, 1440

se_msg_settings db "SETTINGS", 0
se_msg_display  db "DISPLAY", 0
se_msg_auto     db "AUTO (QEMU window)", 0
se_msg_r640     db "640 x 360", 0
se_msg_r1280    db "1280 x 720", 0
se_msg_r1920    db "1920 x 1080", 0
se_msg_r2560    db "2560 x 1440", 0
se_msg_manual   db "MANUAL - auto is paused", 0
se_msg_linked   db "AUTO - linked to window", 0
se_msg_waiting  db "AUTO - waiting for helper", 0
