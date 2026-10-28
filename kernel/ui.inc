; ============================================================
;  ui.inc - desktop, two windows, taskbar, clock, start menu
; ============================================================

TASKBAR_H       equ 20
MENU_W          equ 180
MENU_ITEM_H     equ 16
MENU_H          equ 102
MENU_X          equ 2

TERM_MIN_W      equ 168
TERM_MIN_H      equ 64
SET_MIN_W       equ 260
SET_MIN_H       equ 150

; ------------------------------------------------------------
; ui_init - set the dynamic taskbar position and defaults
; ------------------------------------------------------------
ui_init:
    mov ax, [UI_H]
    sub ax, TASKBAR_H
    mov [taskbar_y], ax
    mov byte [z_top], WIN_TERM
    mov byte [menu_hover], 0xFF
    mov byte [task_hover], 0xFF
    mov byte [set_hover], 0xFF
    ret

; ------------------------------------------------------------
; ui_geometry_win - BX=window ptr -> cur_x/cur_y/cur_w/cur_h
; ------------------------------------------------------------
ui_geometry_win:
    cmp byte [bx + W_STATE], 3
    jne .normal
    mov word [cur_x], 0
    mov word [cur_y], 0
    mov ax, [UI_W]
    mov [cur_w], ax
    mov ax, [taskbar_y]
    mov [cur_h], ax
    ret
.normal:
    mov ax, [bx + W_X]
    mov [cur_x], ax
    mov ax, [bx + W_Y]
    mov [cur_y], ax
    mov ax, [bx + W_W]
    mov [cur_w], ax
    mov ax, [bx + W_H]
    mov [cur_h], ax
    ret

; ------------------------------------------------------------
; ui_point_in_rect - AX=px BX=py CX=x DX=y SI=w DI=h, CF=1 inside
; ------------------------------------------------------------
ui_point_in_rect:
    push bp
    cmp ax, cx
    jb .no
    mov bp, cx
    add bp, si
    cmp ax, bp
    jae .no
    cmp bx, dx
    jb .no
    mov bp, dx
    add bp, di
    cmp bx, bp
    jae .no
    pop bp
    stc
    ret
.no:
    pop bp
    clc
    ret

; ------------------------------------------------------------
; ui_compose - draw the complete scene into the back buffer
; ------------------------------------------------------------
ui_compose:
    call draw_desktop
    call draw_icon

    cmp byte [z_top], WIN_TERM
    jne .term_below
    call ui_draw_settings
    call ui_draw_term
    jmp .taskbar
.term_below:
    call ui_draw_term
    call ui_draw_settings
.taskbar:
    call draw_taskbar

    cmp byte [menu_open], 0
    je .done
    call draw_menu
.done:
    ret

ui_draw_term:
    mov bx, windows + WIN_TERM * W_SIZE
    cmp byte [bx + W_STATE], 0
    je .done
    cmp byte [bx + W_STATE], 2
    je .done
    xor al, al
    jmp draw_window
.done:
    ret

ui_draw_settings:
    mov bx, windows + WIN_SET * W_SIZE
    cmp byte [bx + W_STATE], 0
    je .done
    cmp byte [bx + W_STATE], 2
    je .done
    mov al, WIN_SET
    jmp draw_window
.done:
    ret

; ------------------------------------------------------------
; draw_desktop - dynamic gradient, stars, resolution watermark
; ------------------------------------------------------------
draw_desktop:
    mov ax, [UI_H]
    add ax, 35
    xor dx, dx
    mov bx, 36
    div bx
    test ax, ax
    jnz .band_h_ok
    mov ax, 1
.band_h_ok:
    mov [ui_band_h], ax

    mov word [ui_band], 0
.bands:
    mov ax, [ui_band]
    mul word [ui_band_h]
    mov bx, ax
    xor ax, ax
    mov cx, [UI_W]
    mov dx, [ui_band_h]
    mov si, 64
    add si, [ui_band]
    call fill_rect
    inc word [ui_band]
    cmp word [ui_band], 36
    jb .bands

    mov word [ui_star_x], 7
.stars1:
    mov ax, [ui_star_x]
    cmp ax, [UI_W]
    jae .stars1_done
    mov bx, 37
    mul bx
    add ax, 17
    xor dx, dx
    mov bx, [UI_H]
    sub bx, 30
    test bx, bx
    jnz .mod1
    mov bx, 1
.mod1:
    div bx
    add dx, 8
    mov bx, dx
    mov ax, [ui_star_x]
    mov si, 2
    call put_pixel
    add word [ui_star_x], 19
    jmp .stars1
.stars1_done:

    mov word [ui_star_x], 11
.stars2:
    mov ax, [ui_star_x]
    cmp ax, [UI_W]
    jae .stars2_done
    mov bx, 53
    mul bx
    add ax, 7
    xor dx, dx
    mov bx, [UI_H]
    sub bx, 34
    test bx, bx
    jnz .mod2
    mov bx, 1
.mod2:
    div bx
    add dx, 10
    mov bx, dx
    mov ax, [ui_star_x]
    mov si, 8
    call put_pixel
    add word [ui_star_x], 31
    jmp .stars2
.stars2_done:

    mov si, mode_text
    call text_width
    mov bx, [UI_W]
    sub bx, ax
    sub bx, 8
    mov cx, [taskbar_y]
    sub cx, 12
    mov dl, 8
    call draw_text
    ret

; ------------------------------------------------------------
; draw_icon - TERMINAL desktop icon
; ------------------------------------------------------------
draw_icon:
    ; shadow and body
    mov ax, 12
    mov bx, 10
    mov cx, 32
    mov dx, 32
    mov si, 4
    call fill_rect

    mov ax, 10
    mov bx, 8
    mov cx, 32
    mov dx, 32
    mov si, 6
    call fill_rect

    mov si, 8
    cmp byte [ui_selected], 0
    je .frame
    mov si, 7
.frame:
    mov ax, 10
    mov bx, 8
    mov cx, 32
    mov dx, 32
    call frame_rect

    ; monitor
    mov ax, 13
    mov bx, 11
    mov cx, 26
    mov dx, 20
    mov si, 16
    call fill_rect
    mov ax, 13
    mov bx, 11
    mov cx, 26
    mov dx, 20
    mov si, 2
    call frame_rect

    mov ax, 15
    mov bx, 13
    mov cx, 22
    mov dx, 16
    mov si, 0
    call fill_rect

    mov al, '>'
    mov bx, 17
    mov cx, 17
    mov dl, 9
    call draw_char
    mov al, '_'
    mov bx, 25
    mov cx, 17
    mov dl, 11
    call draw_char

    ; stand
    mov ax, 21
    mov bx, 31
    mov cx, 10
    mov dx, 2
    mov si, 2
    call fill_rect
    mov ax, 17
    mov bx, 33
    mov cx, 18
    mov dx, 2
    mov si, 17
    call fill_rect

    ; label background
    cmp byte [ui_selected], 0
    je .label
    mov ax, 0
    mov bx, 41
    mov cx, 64
    mov dx, 10
    mov si, 6
    call fill_rect
.label:
    mov bx, 0
    mov cx, 42
    mov dl, 1
    cmp byte [ui_selected], 0
    je .label_text
    mov dl, 11
.label_text:
    mov si, ui_msg_terminal
    call draw_text
    ret

; ------------------------------------------------------------
; draw_window - BX=window ptr, AL=window index
; ------------------------------------------------------------
draw_window:
    pusha
    mov [dw_index], al
    mov [dw_ptr], bx
    call ui_geometry_win

    ; shadow
    mov ax, [cur_x]
    add ax, 2
    mov bx, [cur_y]
    add bx, 2
    mov cx, [cur_w]
    mov dx, [cur_h]
    mov si, 0
    call fill_rect

    ; outer frame
    mov ax, [cur_x]
    mov bx, [cur_y]
    mov cx, [cur_w]
    mov dx, [cur_h]
    mov si, 7
    call fill_rect

    ; inner panel
    mov ax, [cur_x]
    inc ax
    mov bx, [cur_y]
    inc bx
    mov cx, [cur_w]
    sub cx, 2
    mov dx, [cur_h]
    sub dx, 2
    mov si, 3
    call fill_rect

    ; title bar (active colour depends on z-order)
    mov si, 5
    mov al, [z_top]
    cmp al, [dw_index]
    jne .title_colour
    mov si, 6
.title_colour:
    mov ax, [cur_x]
    inc ax
    mov bx, [cur_y]
    inc bx
    mov cx, [cur_w]
    sub cx, 2
    mov dx, 11
    call fill_rect

    ; title text
    mov al, [dw_index]
    call ui_win_title
    mov bx, [cur_x]
    add bx, 5
    mov cx, [cur_y]
    add cx, 2
    mov dl, 1
    call draw_text

    ; close button
    mov ax, [cur_x]
    add ax, [cur_w]
    sub ax, 13
    mov [ui_btn_x], ax
    mov bx, [cur_y]
    add bx, 2
    mov [ui_btn_y], bx
    mov cx, 11
    mov dx, 8
    mov si, 13
    call fill_rect
    mov al, 'X'
    mov bx, [ui_btn_x]
    add bx, 2
    mov cx, [ui_btn_y]
    mov dl, 1
    call draw_char

    ; maximize / restore button
    mov ax, [cur_x]
    add ax, [cur_w]
    sub ax, 26
    mov [ui_btn_x], ax
    mov bx, [cur_y]
    add bx, 2
    mov [ui_btn_y], bx
    mov cx, 11
    mov dx, 8
    mov si, 9
    call fill_rect
    mov al, 'O'
    mov bx, [dw_ptr]
    cmp byte [bx + W_STATE], 3
    jne .max_glyph
    mov al, '='
.max_glyph:
    mov bx, [ui_btn_x]
    add bx, 2
    mov cx, [ui_btn_y]
    mov dl, 0
    call draw_char

    ; minimize button
    mov ax, [cur_x]
    add ax, [cur_w]
    sub ax, 39
    mov [ui_btn_x], ax
    mov bx, [cur_y]
    add bx, 2
    mov [ui_btn_y], bx
    mov cx, 11
    mov dx, 8
    mov si, 11
    call fill_rect
    mov al, '_'
    mov bx, [ui_btn_x]
    add bx, 2
    mov cx, [ui_btn_y]
    mov dl, 0
    call draw_char

    ; title separator
    mov ax, [cur_x]
    inc ax
    mov bx, [cur_y]
    add bx, 12
    mov cx, [cur_w]
    sub cx, 2
    mov dx, 1
    mov si, 7
    call fill_rect

    ; client background
    mov ax, [cur_x]
    add ax, 2
    mov bx, [cur_y]
    add bx, 13
    mov cx, [cur_w]
    sub cx, 4
    mov dx, [cur_h]
    sub dx, 15
    mov si, 0
    call fill_rect

    ; window contents
    cmp byte [dw_index], WIN_TERM
    jne .settings
    mov ax, [cur_x]
    add ax, 4
    mov bx, [cur_y]
    add bx, 15
    mov cx, [cur_w]
    sub cx, 8
    mov dx, [cur_h]
    sub dx, 19
    call term_draw
    jmp .done
.settings:
    mov ax, [cur_x]
    add ax, 2
    mov bx, [cur_y]
    add bx, 13
    mov cx, [cur_w]
    sub cx, 4
    mov dx, [cur_h]
    sub dx, 15
    call settings_draw
.done:
    popa
    ret

ui_win_title:
    cmp al, WIN_TERM
    jne .settings
    mov si, ui_msg_terminal
    ret
.settings:
    mov si, ui_msg_settings
    ret

; ------------------------------------------------------------
; draw_taskbar - L button, window buttons, clock and date
; ------------------------------------------------------------
draw_taskbar:
    xor ax, ax
    mov bx, [taskbar_y]
    mov cx, [UI_W]
    mov dx, TASKBAR_H
    mov si, 16
    call fill_rect

    xor ax, ax
    mov bx, [taskbar_y]
    mov cx, [UI_W]
    mov dx, 1
    mov si, 7
    call fill_rect

    ; L button
    mov si, 6
    cmp byte [l_hover], 0
    je .l_hover
    mov si, 8
.l_hover:
    cmp byte [menu_open], 0
    je .l_fill
    mov si, 8
.l_fill:
    mov ax, 2
    mov bx, [taskbar_y]
    add bx, 2
    mov cx, 20
    mov dx, 16
    call fill_rect

    mov ax, 2
    mov bx, [taskbar_y]
    add bx, 2
    mov cx, 20
    mov dx, 16
    mov si, 7
    call frame_rect

    mov al, 'L'
    mov bx, 8
    mov cx, [taskbar_y]
    add cx, 5
    mov dl, 11
    call draw_char

    ; window buttons
    xor al, al
    call draw_task_button
    mov al, WIN_SET
    call draw_task_button

    ; clock and date, right aligned
    mov bx, [UI_W]
    sub bx, 46
    mov cx, [taskbar_y]
    add cx, 1
    mov dl, 11
    mov si, clock_time
    call draw_text

    mov bx, [UI_W]
    sub bx, 86
    mov cx, [taskbar_y]
    add cx, 11
    mov dl, 8
    mov si, clock_date
    call draw_text
    ret

; ------------------------------------------------------------
; draw_task_button - AL = window index
; ------------------------------------------------------------
draw_task_button:
    pusha
    call ui_task_button_geom
    jnc .done
    mov [tb_x], cx
    mov [tb_w], dx
    mov [tb_idx], al

    mov si, 5
    movzx bx, al
    imul bx, W_SIZE
    add bx, windows
    cmp byte [bx + W_STATE], 2
    jne .active
    mov si, 4
    jmp .fill
.active:
    cmp al, [z_top]
    jne .fill
    mov si, 6
.fill:
    mov ax, [tb_x]
    mov bx, [taskbar_y]
    add bx, 2
    mov cx, [tb_w]
    mov dx, 16
    call fill_rect

    mov ax, [tb_x]
    mov bx, [taskbar_y]
    add bx, 2
    mov cx, [tb_w]
    mov dx, 16
    mov si, 8
    call frame_rect

    mov al, [tb_idx]
    call ui_task_label
    mov bx, [tb_x]
    add bx, 4
    mov cx, [taskbar_y]
    add cx, 4
    mov dl, 1
    call draw_text
.done:
    popa
    ret

ui_task_label:
    cmp al, WIN_TERM
    jne .settings
    mov si, ui_msg_term_short
    ret
.settings:
    mov si, ui_msg_settings
    ret

; ------------------------------------------------------------
; ui_task_button_geom - AL=index -> CX=x DX=w, CF=1 if visible
; ------------------------------------------------------------
ui_task_button_geom:
    push ax
    push bx
    mov [tb_idx], al
    movzx bx, al
    imul bx, W_SIZE
    add bx, windows
    cmp byte [bx + W_STATE], 0
    je .hidden

    cmp byte [tb_idx], WIN_TERM
    jne .settings
    mov cx, 26
    mov dx, 52
    jmp .visible

.settings:
    mov cx, 26
    mov bx, windows + WIN_TERM * W_SIZE
    cmp byte [bx + W_STATE], 0
    je .settings_x
    add cx, 56
.settings_x:
    mov dx, 76

.visible:
    pop bx
    pop ax
    stc
    ret
.hidden:
    pop bx
    pop ax
    clc
    ret

; ------------------------------------------------------------
; ui_task_click - AL=index, CF=1 if the click hit its button
; ------------------------------------------------------------
ui_task_click:
    pusha
    mov [tb_idx], al
    call ui_task_button_geom
    jnc .miss

    mov ax, [mouse_y]
    mov bx, [taskbar_y]
    add bx, 2
    cmp ax, bx
    jb .miss
    add bx, 16
    cmp ax, bx
    jae .miss

    mov ax, [mouse_x]
    cmp ax, cx
    jb .miss
    add cx, dx
    cmp ax, cx
    jae .miss

    movzx bx, byte [tb_idx]
    imul bx, W_SIZE
    add bx, windows
    cmp byte [bx + W_STATE], 2
    jne .focus
    mov byte [bx + W_STATE], 1
.focus:
    mov al, [tb_idx]
    call ui_focus_window
    mov byte [scene_dirty], 1
    popa
    stc
    ret
.miss:
    popa
    clc
    ret

; ------------------------------------------------------------
; draw_menu - L start menu, opens upward from the taskbar
; ------------------------------------------------------------
draw_menu:
    mov ax, [taskbar_y]
    sub ax, 2
    sub ax, MENU_H
    mov [menu_top], ax

    ; shadow
    mov bx, ax
    add bx, 2
    mov ax, MENU_X
    add ax, 2
    mov cx, MENU_W
    mov dx, MENU_H
    mov si, 0
    call fill_rect

    ; panel
    mov ax, MENU_X
    mov bx, [menu_top]
    mov cx, MENU_W
    mov dx, MENU_H
    mov si, 16
    call fill_rect

    mov ax, MENU_X
    mov bx, [menu_top]
    mov cx, MENU_W
    mov dx, MENU_H
    mov si, 7
    call frame_rect

    ; header
    mov ax, MENU_X
    inc ax
    mov bx, [menu_top]
    inc bx
    mov cx, MENU_W
    sub cx, 2
    mov dx, 14
    mov si, 6
    call fill_rect

    mov bx, MENU_X
    add bx, 6
    mov cx, [menu_top]
    add cx, 3
    mov dl, 1
    mov si, ui_msg_version
    call draw_text

    mov si, mode_text
    call text_width
    mov bx, MENU_X
    add bx, MENU_W
    sub bx, ax
    sub bx, 6
    mov cx, [menu_top]
    add cx, 3
    mov dl, 2
    call draw_text

    ; items
    mov word [ui_item], 0
.items:
    mov ax, [ui_item]
    imul ax, MENU_ITEM_H
    mov bx, [menu_top]
    add bx, 16
    add bx, ax
    mov [ui_item_y], bx

    mov si, 5
    mov al, [menu_hover]
    cmp al, byte [ui_item]
    jne .item_colour
    mov si, 6
.item_colour:
    mov ax, MENU_X
    inc ax
    mov bx, [ui_item_y]
    mov cx, MENU_W
    sub cx, 2
    mov dx, MENU_ITEM_H
    call fill_rect

    mov al, byte [ui_item]
    call ui_menu_label
    mov bx, MENU_X
    add bx, 8
    mov cx, [ui_item_y]
    add cx, 4
    mov dl, 1
    call draw_text

    inc word [ui_item]
    cmp word [ui_item], 4
    jb .items

    ; footer
    mov ax, MENU_X
    inc ax
    mov bx, [menu_top]
    add bx, MENU_H
    sub bx, 14
    mov cx, MENU_W
    sub cx, 2
    mov dx, 1
    mov si, 3
    call fill_rect

    mov bx, MENU_X
    add bx, 6
    mov cx, [menu_top]
    add cx, MENU_H
    sub cx, 11
    mov dl, 8
    mov si, ui_msg_hint
    call draw_text
    ret

ui_menu_label:
    cmp al, 0
    je .terminal
    cmp al, 1
    je .settings
    cmp al, 2
    je .restart
    mov si, ui_msg_shutdown
    ret
.terminal:
    mov si, ui_msg_terminal
    ret
.settings:
    mov si, ui_msg_settings
    ret
.restart:
    mov si, ui_msg_restart
    ret

; ------------------------------------------------------------
; ui_menu_item_at - AL = 0..3, or 0xFF when outside
; ------------------------------------------------------------
ui_menu_item_at:
    mov ax, [mouse_x]
    cmp ax, MENU_X
    jb .none
    cmp ax, MENU_X + MENU_W
    jae .none

    mov bx, [taskbar_y]
    sub bx, 2
    sub bx, MENU_H
    add bx, 16
    mov ax, [mouse_y]
    cmp ax, bx
    jb .none
    mov cx, bx
    add cx, 4 * MENU_ITEM_H
    cmp ax, cx
    jae .none

    sub ax, bx
    xor dx, dx
    mov bx, MENU_ITEM_H
    div bx
    ret
.none:
    mov al, 0xFF
    ret

; ------------------------------------------------------------
; ui_menu_activate - AL = 0..3
; ------------------------------------------------------------
ui_menu_activate:
    mov byte [menu_open], 0
    mov byte [scene_dirty], 1
    cmp al, 0
    je open_terminal
    cmp al, 1
    je open_settings
    cmp al, 2
    je .restart
    jmp system_poweroff
.restart:
    jmp system_reboot

; ------------------------------------------------------------
; ui_update_hover - L / taskbar / menu / settings hover states
; ------------------------------------------------------------
ui_update_hover:
    ; L button
    mov ax, [mouse_x]
    mov bx, [mouse_y]
    mov cx, 2
    mov dx, [taskbar_y]
    add dx, 2
    mov si, 20
    mov di, 16
    call ui_point_in_rect
    mov bl, 0
    jnc .l_set
    mov bl, 1
.l_set:
    cmp bl, [l_hover]
    je .task
    mov [l_hover], bl
    mov byte [scene_dirty], 1

.task:
    mov al, [task_hover]
    mov [task_hover_old], al
    mov byte [task_hover], 0xFF
    xor al, al
    call ui_task_hover
    mov al, WIN_SET
    call ui_task_hover
    mov al, [task_hover]
    cmp al, [task_hover_old]
    je .menu
    mov byte [scene_dirty], 1

.menu:
    cmp byte [menu_open], 0
    je .settings
    call ui_menu_item_at
    cmp al, [menu_hover]
    je .settings
    mov [menu_hover], al
    mov byte [scene_dirty], 1

.settings:
    mov byte [se_hover], 0xFF
    mov byte [set_hover], 0xFF
    mov bx, windows + WIN_SET * W_SIZE
    mov al, [bx + W_STATE]
    cmp al, 1
    je .settings_visible
    cmp al, 3
    jne .settings_done
.settings_visible:
    call ui_geometry_win
    mov ax, [mouse_x]
    mov bx, [mouse_y]
    mov cx, [cur_x]
    add cx, 2
    mov dx, [cur_y]
    add dx, 13
    mov si, [cur_w]
    sub si, 4
    mov di, [cur_h]
    sub di, 15
    call ui_point_in_rect
    jnc .settings_done
    mov ax, [mouse_x]
    sub ax, [cur_x]
    sub ax, 2
    mov bx, [mouse_y]
    sub bx, [cur_y]
    sub bx, 13
    call settings_hover_at
    mov [se_hover], al
    cmp al, [set_hover]
    je .settings_done
    mov [set_hover], al
    mov byte [scene_dirty], 1
.settings_done:
    ret

ui_task_hover:
    pusha
    mov [tb_idx], al
    call ui_task_button_geom
    jnc .done
    mov ax, [mouse_y]
    mov bx, [taskbar_y]
    add bx, 2
    cmp ax, bx
    jb .done
    add bx, 16
    cmp ax, bx
    jae .done
    mov ax, [mouse_x]
    cmp ax, cx
    jb .done
    add cx, dx
    cmp ax, cx
    jae .done
    mov al, [tb_idx]
    cmp al, [task_hover]
    je .done
    mov [task_hover], al
    mov byte [scene_dirty], 1
.done:
    popa
    ret

; ------------------------------------------------------------
; ui_drag_window - move a normal window while left button is held
; ------------------------------------------------------------
ui_drag_window:
    cmp byte [dragging], 0
    je .done
    test byte [mouse_left], 1
    jz .stop

    movzx bx, byte [drag_win]
    imul bx, W_SIZE
    add bx, windows
    cmp byte [bx + W_STATE], 1
    jne .stop

    mov ax, [mouse_x]
    sub ax, [drag_dx]
    cmp ax, 0
    jge .x_low
    xor ax, ax
.x_low:
    mov cx, [UI_W]
    sub cx, [bx + W_W]
    cmp ax, cx
    jle .x_ok
    mov ax, cx
.x_ok:
    cmp ax, [bx + W_X]
    je .y_calc
    mov [bx + W_X], ax
    mov byte [scene_dirty], 1

.y_calc:
    mov ax, [mouse_y]
    sub ax, [drag_dy]
    cmp ax, 0
    jge .y_low
    xor ax, ax
.y_low:
    mov cx, [taskbar_y]
    sub cx, [bx + W_H]
    cmp ax, cx
    jle .y_ok
    mov ax, cx
.y_ok:
    cmp ax, [bx + W_Y]
    je .done
    mov [bx + W_Y], ax
    mov byte [scene_dirty], 1
    jmp .done
.stop:
    mov byte [dragging], 0
.done:
    ret

; ------------------------------------------------------------
; ui_resize_window - resize the terminal from its bottom-right grip
; ------------------------------------------------------------
ui_resize_window:
    cmp byte [resizing], 0
    je .done
    test byte [mouse_left], 1
    jz .stop

    mov bx, windows + WIN_TERM * W_SIZE
    cmp byte [bx + W_STATE], 1
    jne .stop

    mov ax, [mouse_x]
    sub ax, [bx + W_X]
    cmp ax, TERM_MIN_W
    jae .w_min
    mov ax, TERM_MIN_W
.w_min:
    mov cx, [UI_W]
    sub cx, [bx + W_X]
    cmp ax, cx
    jbe .w_max
    mov ax, cx
.w_max:
    cmp ax, [bx + W_W]
    je .h_calc
    mov [bx + W_W], ax
    mov byte [scene_dirty], 1

.h_calc:
    mov ax, [mouse_y]
    sub ax, [bx + W_Y]
    cmp ax, TERM_MIN_H
    jae .h_min
    mov ax, TERM_MIN_H
.h_min:
    mov cx, [taskbar_y]
    sub cx, [bx + W_Y]
    cmp ax, cx
    jbe .h_max
    mov ax, cx
.h_max:
    cmp ax, [bx + W_H]
    je .done
    mov [bx + W_H], ax
    mov byte [scene_dirty], 1
    jmp .done
.stop:
    mov byte [resizing], 0
.done:
    ret

; ------------------------------------------------------------
; open_terminal - open, restore or focus the terminal window
; ------------------------------------------------------------
open_terminal:
    mov bx, windows + WIN_TERM * W_SIZE
    cmp byte [bx + W_STATE], 0
    je .fresh
    cmp byte [bx + W_STATE], 2
    jne .focus
    mov byte [bx + W_STATE], 1
.focus:
    xor al, al
    call ui_focus_window
    mov byte [scene_dirty], 1
    ret
.fresh:
    call term_init
    mov bx, windows + WIN_TERM * W_SIZE
    mov byte [bx + W_STATE], 1
    mov word [bx + W_X], 80
    mov word [bx + W_Y], 12
    mov word [bx + W_W], 272
    mov word [bx + W_H], 140
    call ui_clamp_windows
    xor al, al
    call ui_focus_window
    mov byte [scene_dirty], 1
    ret

; ------------------------------------------------------------
; ui_focus_window - AL = index, bring to front
; ------------------------------------------------------------
ui_focus_window:
    cmp al, [z_top]
    je .done
    mov [z_top], al
    mov byte [scene_dirty], 1
.done:
    ret

; ------------------------------------------------------------
; ui_pick_top - after close/minimize choose another visible window
; ------------------------------------------------------------
ui_pick_top:
    mov al, [z_top]
    movzx bx, al
    imul bx, W_SIZE
    add bx, windows
    mov al, [bx + W_STATE]
    cmp al, 1
    je .done
    cmp al, 3
    je .done

    mov al, [z_top]
    xor al, 1
    movzx bx, al
    imul bx, W_SIZE
    add bx, windows
    cmp byte [bx + W_STATE], 0
    je .done
    mov al, [z_top]
    xor al, 1
    mov [z_top], al
.done:
    ret

; ------------------------------------------------------------
; ui_clamp_windows - fit all windows after a mode change
; ------------------------------------------------------------
ui_clamp_windows:
    pusha
    mov ax, [UI_H]
    sub ax, TASKBAR_H
    mov [taskbar_y], ax

    mov al, WIN_TERM
    call ui_clamp_one
    mov al, WIN_SET
    call ui_clamp_one
    mov byte [scene_dirty], 1
    popa
    ret

ui_clamp_one:
    pusha
    movzx bx, al
    imul bx, W_SIZE
    add bx, windows

    cmp al, WIN_TERM
    jne .settings_min
    mov ax, TERM_MIN_W
    mov dx, TERM_MIN_H
    jmp .clamp
.settings_min:
    mov ax, SET_MIN_W
    mov dx, SET_MIN_H

.clamp:
    push bx
    push ax
    push dx
    lea bx, [bx + W_X]
    call ui_clamp_rect
    pop dx
    pop ax
    pop bx
    push bx
    push ax
    push dx
    lea bx, [bx + W_SX]
    call ui_clamp_rect
    pop dx
    pop ax
    pop bx
    popa
    ret

; ------------------------------------------------------------
; ui_clamp_rect - BX points to x,y,w,h; AX=min w, DX=min h
; ------------------------------------------------------------
ui_clamp_rect:
    pusha
    ; width
    mov cx, [bx + 4]
    cmp cx, ax
    jae .w_min_ok
    mov cx, ax
.w_min_ok:
    mov si, [UI_W]
    cmp cx, si
    jbe .w_max_ok
    mov cx, si
.w_max_ok:
    mov [bx + 4], cx

    ; x
    mov si, [UI_W]
    sub si, cx
    mov cx, [bx]
    cmp cx, 0
    jge .x_low_ok
    xor cx, cx
.x_low_ok:
    cmp cx, si
    jle .x_ok
    mov cx, si
.x_ok:
    mov [bx], cx

    ; height
    mov cx, [bx + 6]
    cmp cx, dx
    jae .h_min_ok
    mov cx, dx
.h_min_ok:
    mov si, [taskbar_y]
    cmp cx, si
    jbe .h_max_ok
    mov cx, si
.h_max_ok:
    mov [bx + 6], cx

    ; y
    mov si, [taskbar_y]
    sub si, cx
    mov cx, [bx + 2]
    cmp cx, 0
    jge .y_low_ok
    xor cx, cx
.y_low_ok:
    cmp cx, si
    jle .y_ok
    mov cx, si
.y_ok:
    mov [bx + 2], cx
    popa
    ret

; ------------------------------------------------------------
; ui_click - handle a left mouse press
; ------------------------------------------------------------
ui_click:
    ; L button
    mov ax, [mouse_x]
    mov bx, [mouse_y]
    mov cx, 2
    mov dx, [taskbar_y]
    add dx, 2
    mov si, 20
    mov di, 16
    call ui_point_in_rect
    jnc .not_l
    xor byte [menu_open], 1
    mov byte [menu_hover], 0
    mov byte [scene_dirty], 1
    ret
.not_l:

    ; open menu consumes the click
    cmp byte [menu_open], 0
    je .no_menu
    call ui_menu_item_at
    cmp al, 0xFF
    je .close_menu
    call ui_menu_activate
    ret
.close_menu:
    mov byte [menu_open], 0
    mov byte [scene_dirty], 1
    ret
.no_menu:

    ; taskbar window buttons
    mov al, WIN_TERM
    call ui_task_click
    jc .done
    mov al, WIN_SET
    call ui_task_click
    jc .done

    ; empty taskbar area
    mov ax, [mouse_y]
    cmp ax, [taskbar_y]
    jb .not_taskbar
    ret
.not_taskbar:

    ; windows, topmost first
    mov al, [z_top]
    call ui_click_window
    jc .done
    mov al, [z_top]
    xor al, 1
    call ui_click_window
    jc .done

    ; desktop
    mov ax, [mouse_x]
    mov bx, [mouse_y]
    mov cx, 4
    mov dx, 4
    mov si, 62
    mov di, 48
    call ui_point_in_rect
    jc .icon
    mov byte [ui_selected], 0
    mov byte [last_click_icon], 0
    mov byte [scene_dirty], 1
    ret
.icon:
    call ui_icon_click
.done:
    ret

; ------------------------------------------------------------
; ui_click_window - AL=index, CF=1 when the click hit the window
; ------------------------------------------------------------
ui_click_window:
    pusha
    mov [cw_idx], al
    movzx bx, al
    imul bx, W_SIZE
    add bx, windows
    mov [cw_ptr], bx

    mov al, [bx + W_STATE]
    cmp al, 0
    je .miss
    cmp al, 2
    je .miss

    call ui_geometry_win

    mov ax, [mouse_x]
    mov bx, [mouse_y]
    mov cx, [cur_x]
    mov dx, [cur_y]
    mov si, [cur_w]
    mov di, [cur_h]
    call ui_point_in_rect
    jnc .miss

    ; focus on any click inside
    mov al, [cw_idx]
    call ui_focus_window

    ; close
    mov ax, [mouse_x]
    mov bx, [mouse_y]
    mov cx, [cur_x]
    add cx, [cur_w]
    sub cx, 13
    mov dx, [cur_y]
    add dx, 2
    mov si, 11
    mov di, 8
    call ui_point_in_rect
    jc .close

    ; minimize
    mov ax, [mouse_x]
    mov bx, [mouse_y]
    mov cx, [cur_x]
    add cx, [cur_w]
    sub cx, 39
    mov dx, [cur_y]
    add dx, 2
    mov si, 11
    mov di, 8
    call ui_point_in_rect
    jc .minimize

    ; maximize / restore
    mov ax, [mouse_x]
    mov bx, [mouse_y]
    mov cx, [cur_x]
    add cx, [cur_w]
    sub cx, 26
    mov dx, [cur_y]
    add dx, 2
    mov si, 11
    mov di, 8
    call ui_point_in_rect
    jc .maximize

    ; terminal resize grip
    cmp byte [cw_idx], WIN_TERM
    jne .title
    mov bx, [cw_ptr]
    cmp byte [bx + W_STATE], 1
    jne .title
    mov ax, [mouse_x]
    mov bx, [mouse_y]
    mov cx, [cur_x]
    add cx, [cur_w]
    sub cx, 12
    mov dx, [cur_y]
    add dx, [cur_h]
    sub dx, 12
    mov si, 12
    mov di, 12
    call ui_point_in_rect
    jc .resize

.title:
    ; title bar starts dragging
    mov ax, [mouse_x]
    mov bx, [mouse_y]
    mov cx, [cur_x]
    mov dx, [cur_y]
    mov si, [cur_w]
    mov di, 12
    call ui_point_in_rect
    jc .start_drag

    ; client area
    mov ax, [mouse_x]
    mov bx, [mouse_y]
    mov cx, [cur_x]
    add cx, 2
    mov dx, [cur_y]
    add dx, 13
    mov si, [cur_w]
    sub si, 4
    mov di, [cur_h]
    sub di, 15
    call ui_point_in_rect
    jnc .handled

    cmp byte [cw_idx], WIN_SET
    jne .handled
    mov ax, [mouse_x]
    sub ax, [cur_x]
    sub ax, 2
    mov bx, [mouse_y]
    sub bx, [cur_y]
    sub bx, 13
    call settings_click
.handled:
    popa
    stc
    ret

.miss:
    popa
    clc
    ret

.close:
    mov bx, [cw_ptr]
    mov byte [bx + W_STATE], 0
    mov byte [dragging], 0
    mov byte [resizing], 0
    call ui_pick_top
    mov byte [scene_dirty], 1
    popa
    stc
    ret

.minimize:
    mov bx, [cw_ptr]
    mov byte [bx + W_STATE], 2
    mov byte [dragging], 0
    mov byte [resizing], 0
    call ui_pick_top
    mov byte [scene_dirty], 1
    popa
    stc
    ret

.maximize:
    mov bx, [cw_ptr]
    cmp byte [bx + W_STATE], 3
    je .restore
    mov ax, [bx + W_X]
    mov [bx + W_SX], ax
    mov ax, [bx + W_Y]
    mov [bx + W_SY], ax
    mov ax, [bx + W_W]
    mov [bx + W_SW], ax
    mov ax, [bx + W_H]
    mov [bx + W_SH], ax
    mov byte [bx + W_STATE], 3
    mov byte [dragging], 0
    mov byte [resizing], 0
    mov byte [scene_dirty], 1
    popa
    stc
    ret
.restore:
    mov byte [bx + W_STATE], 1
    mov ax, [bx + W_SX]
    mov [bx + W_X], ax
    mov ax, [bx + W_SY]
    mov [bx + W_Y], ax
    mov ax, [bx + W_SW]
    mov [bx + W_W], ax
    mov ax, [bx + W_SH]
    mov [bx + W_H], ax
    call ui_clamp_windows
    mov byte [scene_dirty], 1
    popa
    stc
    ret

.resize:
    mov byte [resizing], 1
    mov byte [dragging], 0
    mov al, [cw_idx]
    mov [resize_win], al
    popa
    stc
    ret

.start_drag:
    mov bx, [cw_ptr]
    cmp byte [bx + W_STATE], 1
    jne .handled
    mov byte [dragging], 1
    mov byte [resizing], 0
    mov al, [cw_idx]
    mov [drag_win], al
    mov ax, [mouse_x]
    sub ax, [bx + W_X]
    mov [drag_dx], ax
    mov ax, [mouse_y]
    sub ax, [bx + W_Y]
    mov [drag_dy], ax
    popa
    stc
    ret

; ------------------------------------------------------------
; ui_icon_click - select or open TERMINAL with double click
; ------------------------------------------------------------
ui_icon_click:
    mov eax, [tick_count]
    mov ebx, [last_click_tick]
    sub eax, ebx
    cmp eax, 4
    jbe .maybe_double
.single:
    mov byte [ui_selected], 1
    mov eax, [tick_count]
    mov [last_click_tick], ax
    mov byte [last_click_icon], 1
    mov byte [scene_dirty], 1
    ret
.maybe_double:
    cmp byte [last_click_icon], 0
    je .single
    mov byte [last_click_icon], 0
    call open_terminal
    ret

; ------------------------------------------------------------
; ui_key - consume one keyboard character
; CF=1 when a key was consumed
; ------------------------------------------------------------
ui_key:
    call kbd_get
    jnc .none
    mov [ui_key_char], al

    cmp byte [menu_open], 0
    je .no_menu

    cmp al, 27
    je .close_menu
    cmp al, 0x80
    je .menu_up
    cmp al, 0x81
    je .menu_down
    cmp al, 13
    je .menu_enter
    mov byte [scene_dirty], 1
    stc
    ret
.close_menu:
    mov byte [menu_open], 0
    mov byte [scene_dirty], 1
    stc
    ret
.menu_up:
    mov al, [menu_hover]
    cmp al, 0xFF
    jne .up_ok
    xor al, al
    jmp .up_store
.up_ok:
    test al, al
    jz .up_store
    dec al
.up_store:
    mov [menu_hover], al
    mov byte [scene_dirty], 1
    stc
    ret
.menu_down:
    mov al, [menu_hover]
    cmp al, 0xFF
    jne .down_ok
    xor al, al
    jmp .down_store
.down_ok:
    cmp al, 3
    jae .down_store
    inc al
.down_store:
    mov [menu_hover], al
    mov byte [scene_dirty], 1
    stc
    ret
.menu_enter:
    mov al, [menu_hover]
    cmp al, 0xFF
    je .consumed
    call ui_menu_activate
    stc
    ret
.consumed:
    stc
    ret

.no_menu:
    cmp al, 27
    je .consumed
    cmp byte [z_top], WIN_TERM
    jne .consumed
    mov bx, windows + WIN_TERM * W_SIZE
    mov al, [bx + W_STATE]
    cmp al, 1
    je .terminal
    cmp al, 3
    jne .consumed
.terminal:
    ; restore AL from the key ring is not possible here, so re-read it
    ; from the byte we saved below.
    mov al, [ui_key_char]
    call term_key
    stc
    ret

.none:
    clc
    ret

; ------------------------------------------------------------
; ui data
; ------------------------------------------------------------
windows:
    ; state, x, y, w, h, saved x, y, w, h, page
    dw 1,  80,  12, 272, 140,  80,  12, 272, 140, 0
    dw 0,  16,   5, 288, 150,  16,   5, 288, 150, 0

z_top           db WIN_TERM
menu_open       db 0
menu_hover      db 0xFF
menu_top        dw 0
l_hover         db 0
task_hover      db 0xFF
task_hover_old  db 0xFF
set_hover       db 0xFF
se_hover        db 0
ui_selected     db 0
ui_band         dw 0
ui_band_h       dw 0
ui_star_x       dw 0
ui_btn_x        dw 0
ui_btn_y        dw 0
ui_item         dw 0
ui_item_y       dw 0
tb_idx          db 0
tb_x            dw 0
tb_w            dw 0
dw_index        db 0
dw_ptr          dw 0
cw_idx          db 0
cw_ptr          dw 0
cw_client_x     dw 0
cw_client_y     dw 0
dragging        db 0
drag_win        db 0
drag_dx         dw 0
drag_dy         dw 0
resizing        db 0
resize_win      db 0
last_click_tick dw 0xFFFF
last_click_icon db 0
ui_key_char     db 0
taskbar_y       dw 160
cur_x           dw 0
cur_y           dw 0
cur_w           dw 0
cur_h           dw 0

ui_msg_terminal     db "TERMINAL", 0
ui_msg_term_short   db "TERM", 0
ui_msg_settings     db "SETTINGS", 0
ui_msg_version      db "LiMaxOS 1.1", 0
ui_msg_restart      db "Restart", 0
ui_msg_shutdown     db "Shutdown", 0
ui_msg_hint         db "L = close menu", 0
