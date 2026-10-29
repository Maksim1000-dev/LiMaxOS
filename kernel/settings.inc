; ============================================================
;  settings.inc - SETTINGS window (DISPLAY panel)
;
;  Rows: 0 AUTO, 1 MAXIMUM, 2..2+count-1 real VBE/Bochs modes.
;  The mode list is built at boot from gfx_modes, so it always
;  matches the video card and the available video memory.
;
;  Selecting a row applies the mode immediately as a preview.
;  APPLY keeps it, CANCEL or a 10 second timeout restores the
;  resolution that was active before the preview started.
; ============================================================

SE_SIDEBAR       equ 76
SE_ROW_X         equ 82
SE_LIST_Y        equ 22
SE_ROW_H         equ 16
SE_ROW_BOX_H     equ 14
SE_FOOTER_H      equ 28
SE_BTN_W         equ 58
SE_BTN_H         equ 16
SE_SCROLL_W      equ 8
SE_PREVIEW_TICKS equ 1000

SE_HIT_NONE      equ 0xFF
SE_HIT_APPLY     equ 0x80
SE_HIT_CANCEL    equ 0x81
SE_HIT_UP        equ 0x82
SE_HIT_DOWN      equ 0x83
SE_HIT_PGUP      equ 0x84
SE_HIT_PGDN      equ 0x85

; ------------------------------------------------------------
; settings_layout - recompute visible rows and clamp scrolling
; Uses se_w / se_h from the last draw.
; ------------------------------------------------------------
settings_layout:
    pusha

    mov ax, [se_h]
    sub ax, SE_LIST_Y + SE_FOOTER_H
    cmp ax, SE_ROW_H
    jae .list_h_ok
    mov ax, SE_ROW_H
.list_h_ok:
    mov [se_list_h], ax

    xor dx, dx
    mov cx, SE_ROW_H
    div cx
    test ax, ax
    jnz .vis_ok
    mov ax, 1
.vis_ok:
    mov [se_vis_rows], ax

    movzx ax, byte [gfx_mode_count]
    add ax, 2
    mov [se_total_rows], ax

    ; max_scroll = total - visible (0 when everything fits)
    xor cx, cx
    mov bx, [se_total_rows]
    cmp bx, [se_vis_rows]
    jbe .max_ok
    sub bx, [se_vis_rows]
    mov cx, bx
.max_ok:
    cmp [se_scroll], cx
    jbe .done
    mov [se_scroll], cx
.done:
    popa
    ret

; ------------------------------------------------------------
; settings_scroll_to - AX = row, make sure it is visible
; ------------------------------------------------------------
settings_scroll_to:
    pusha
    mov bx, [se_scroll]
    cmp ax, bx
    jae .below
    mov [se_scroll], ax
    jmp .done
.below:
    add bx, [se_vis_rows]
    cmp ax, bx
    jb .done
    sub ax, [se_vis_rows]
    inc ax
    mov [se_scroll], ax
.done:
    popa
    ret

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

    ; cyan divider
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
    mov dl, 7
    mov si, se_msg_settings
    call draw_text

    ; DISPLAY tab (always active)
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
    mov si, 7
    call frame_rect

    mov bx, [se_x]
    add bx, 9
    mov cx, [se_y]
    add cx, 27
    mov dl, 1
    mov si, se_msg_display
    call draw_text

    ; sidebar footer: backend and mode count
    mov bx, [se_x]
    add bx, 6
    mov cx, [se_y]
    add cx, [se_h]
    sub cx, 33
    mov dl, 8
    mov si, se_msg_video
    call draw_text

    cmp byte [gfx_backend], GFX_BACKEND_BOCHS
    je .backend_bochs
    cmp byte [gfx_backend], GFX_BACKEND_VBE
    je .backend_vbe
    mov si, se_msg_backend_none
    jmp .backend_draw
.backend_bochs:
    mov si, se_msg_backend_bochs
    jmp .backend_draw
.backend_vbe:
    mov si, se_msg_backend_vbe
.backend_draw:
    mov bx, [se_x]
    add bx, 6
    mov cx, [se_y]
    add cx, [se_h]
    sub cx, 23
    mov dl, 7
    call draw_text

    ; "N MODES"
    mov di, se_side_buf
    movzx ax, byte [gfx_mode_count]
    call gfx_u16_dec
    mov si, se_msg_modes
    call se_copy
    mov bx, [se_x]
    add bx, 6
    mov cx, [se_y]
    add cx, [se_h]
    sub cx, 13
    mov dl, 2
    mov si, se_side_buf
    call draw_text

    ; ---- main panel header ----
    mov bx, [se_x]
    add bx, SE_ROW_X
    mov cx, [se_y]
    add cx, 6
    mov dl, 1
    mov si, se_msg_display
    call draw_text

    mov si, mode_text
    call text_width
    mov bx, [se_x]
    add bx, [se_w]
    sub bx, 8
    sub bx, ax
    mov cx, [se_y]
    add cx, 6
    mov dl, 7
    mov si, mode_text
    call draw_text

    ; separator under the header
    mov ax, [se_x]
    add ax, SE_ROW_X
    mov bx, [se_y]
    add bx, 18
    mov cx, [se_w]
    sub cx, SE_ROW_X + 6
    mov dx, 1
    mov si, 3
    call fill_rect

    call settings_layout

    ; row width depends on whether the scrollbar is visible
    mov ax, [se_w]
    sub ax, SE_ROW_X + 8
    mov cx, [se_total_rows]
    cmp cx, [se_vis_rows]
    jbe .row_w_ok
    mov ax, [se_w]
    sub ax, SE_ROW_X + 14
.row_w_ok:
    mov [se_row_w], ax

    ; ---- mode rows ----
    mov word [se_row], 0
.row_loop:
    mov ax, [se_row]
    cmp ax, [se_vis_rows]
    jae .rows_done
    add ax, [se_scroll]
    mov [se_abs_row], ax
    cmp ax, [se_total_rows]
    jae .rows_done

    ; absolute row y
    mov bx, [se_row]
    imul bx, SE_ROW_H
    add bx, [se_y]
    add bx, SE_LIST_Y
    mov [se_rowy], bx

    ; selection / hover state
    mov al, byte [se_abs_row]
    mov byte [se_sel_flag], 0
    cmp byte [preview_pending], 0
    je .check_committed
    cmp al, [preview_choice]
    jne .check_hover
    mov byte [se_sel_flag], 1
    jmp .selected
.check_committed:
    cmp al, [display_preset]
    jne .check_hover
    mov byte [se_sel_flag], 1
.selected:
    mov si, 6
    mov dl, 1
    jmp .paint
.check_hover:
    cmp al, [set_hover]
    jne .normal
    mov si, 5
    mov dl, 7
    jmp .paint
.normal:
    mov si, 4
    mov dl, 2
.paint:
    mov [se_text_color], dl

    mov ax, [se_x]
    add ax, SE_ROW_X
    mov bx, [se_rowy]
    mov cx, [se_row_w]
    mov dx, SE_ROW_BOX_H
    call fill_rect

    ; accent bar on the selected row
    cmp byte [se_sel_flag], 0
    je .marker
    mov ax, [se_x]
    add ax, SE_ROW_X
    mov bx, [se_rowy]
    mov cx, 2
    mov dx, SE_ROW_BOX_H
    mov si, 7
    call fill_rect

.marker:
    ; radio square
    mov ax, [se_x]
    add ax, SE_ROW_X + 5
    mov bx, [se_rowy]
    add bx, 4
    mov cx, 7
    mov dx, 7
    mov si, 7
    call frame_rect

    cmp byte [se_sel_flag], 0
    je .label
    mov ax, [se_x]
    add ax, SE_ROW_X + 7
    mov bx, [se_rowy]
    add bx, 6
    mov cx, 3
    mov dx, 3
    mov si, 7
    call fill_rect

.label:
    mov al, byte [se_abs_row]
    call settings_row_label
    mov bx, [se_x]
    add bx, SE_ROW_X + 16
    mov cx, [se_rowy]
    add cx, 3
    mov dl, [se_text_color]
    call draw_text

    inc word [se_row]
    jmp .row_loop
.rows_done:

    ; ---- scrollbar ----
    mov ax, [se_total_rows]
    cmp ax, [se_vis_rows]
    jbe .no_scroll

    mov ax, [se_x]
    add ax, [se_w]
    sub ax, 10
    mov [se_bar_x], ax

    ; track
    mov ax, [se_bar_x]
    mov bx, [se_y]
    add bx, SE_LIST_Y
    mov cx, SE_SCROLL_W
    mov dx, [se_list_h]
    mov si, 16
    call fill_rect

    ; up arrow
    mov bx, [se_bar_x]
    mov cx, [se_y]
    add cx, SE_LIST_Y
    mov dl, 2
    cmp byte [set_hover], SE_HIT_UP
    jne .up_colour
    mov dl, 7
.up_colour:
    mov al, '^'
    call draw_char

    ; down arrow
    mov bx, [se_bar_x]
    mov cx, [se_y]
    add cx, SE_LIST_Y
    add cx, [se_list_h]
    sub cx, 8
    mov dl, 2
    cmp byte [set_hover], SE_HIT_DOWN
    jne .down_colour
    mov dl, 7
.down_colour:
    mov al, 'v'
    call draw_char

    ; thumb geometry
    mov ax, [se_list_h]
    sub ax, 16
    mov [se_track_h], ax

    mov bx, [se_vis_rows]
    mul bx
    mov cx, [se_total_rows]
    div cx
    cmp ax, 8
    jae .thumb_min
    mov ax, 8
.thumb_min:
    cmp ax, [se_track_h]
    jbe .thumb_max
    mov ax, [se_track_h]
.thumb_max:
    mov [se_thumb_h], ax

    mov cx, [se_total_rows]
    sub cx, [se_vis_rows]
    mov ax, [se_track_h]
    sub ax, [se_thumb_h]
    mov bx, [se_scroll]
    mul bx
    div cx
    add ax, [se_y]
    add ax, SE_LIST_Y + 8
    mov [se_thumb_y], ax

    mov ax, [se_bar_x]
    inc ax
    mov bx, [se_thumb_y]
    mov cx, SE_SCROLL_W - 2
    mov dx, [se_thumb_h]
    mov si, 18
    call fill_rect

.no_scroll:

    ; ---- footer ----
    mov bx, [se_x]
    add bx, SE_ROW_X
    mov cx, [se_y]
    add cx, [se_h]
    sub cx, 10

    cmp byte [preview_pending], 0
    je .status_idle

    ; "PREVIEW Ns"
    mov di, se_status_buf
    mov si, se_msg_preview
    call se_copy
    mov al, ' '
    mov [di], al
    inc di
    movzx ax, byte [preview_secs]
    call gfx_u16_dec
    mov al, 's'
    mov [di], al
    inc di
    mov byte [di], 0

    mov si, se_status_buf
    mov dl, 11
    call draw_text
    jmp .draw_buttons

.status_idle:
    cmp byte [display_locked], 0
    je .status_auto
    mov si, se_msg_manual
    mov dl, 12
    jmp .status_draw
.status_auto:
    cmp byte [auto_valid], 0
    je .status_waiting
    mov si, se_msg_linked
    mov dl, 9
    jmp .status_draw
.status_waiting:
    mov si, se_msg_waiting
    mov dl, 8
.status_draw:
    call draw_text
    jmp .footer_done

.draw_buttons:
    ; APPLY / CANCEL
    mov ax, [se_x]
    add ax, [se_w]
    sub ax, SE_BTN_W + 8
    mov [se_btn_x], ax
    sub ax, SE_BTN_W + 4
    mov [se_btn_cx], ax

    mov ax, [se_y]
    add ax, [se_h]
    sub ax, SE_FOOTER_H - 2
    mov [se_btn_y], ax

    ; APPLY button
    mov si, 9
    cmp byte [set_hover], SE_HIT_APPLY
    jne .apply_fill
    mov si, 22
.apply_fill:
    mov ax, [se_btn_x]
    mov bx, [se_btn_y]
    mov cx, SE_BTN_W
    mov dx, SE_BTN_H
    call fill_rect

    mov ax, [se_btn_x]
    mov bx, [se_btn_y]
    mov cx, SE_BTN_W
    mov dx, SE_BTN_H
    mov si, 7
    call frame_rect

    mov bx, [se_btn_x]
    add bx, 9
    mov cx, [se_btn_y]
    add cx, 4
    mov dl, 0
    mov si, se_msg_apply
    call draw_text

    ; CANCEL button
    mov si, 17
    cmp byte [set_hover], SE_HIT_CANCEL
    jne .cancel_fill
    mov si, 19
.cancel_fill:
    mov ax, [se_btn_cx]
    mov bx, [se_btn_y]
    mov cx, SE_BTN_W
    mov dx, SE_BTN_H
    call fill_rect

    mov ax, [se_btn_cx]
    mov bx, [se_btn_y]
    mov cx, SE_BTN_W
    mov dx, SE_BTN_H
    mov si, 7
    call frame_rect

    mov bx, [se_btn_cx]
    add bx, 5
    mov cx, [se_btn_y]
    add cx, 4
    mov dl, 1
    mov si, se_msg_cancel
    call draw_text

.footer_done:
    popa
    ret

; ------------------------------------------------------------
; se_copy - copy ASCIIZ DS:SI to DS:DI, DI ends on the terminator
; ES must not be used here: it is a flat selector after unreal.
; ------------------------------------------------------------
se_copy:
    lodsb
    mov [di], al
    inc di
    test al, al
    jnz se_copy
    dec di
    ret

; ------------------------------------------------------------
; settings_row_label - AL=row -> SI=label
; ------------------------------------------------------------
settings_row_label:
    cmp al, 0
    je .auto
    cmp al, 1
    je .maximum

    ; real mode row
    sub al, 2
    movzx ax, al
    imul ax, GM_SIZE
    mov si, gfx_modes
    add si, ax

    mov di, se_label_buf
    mov ax, [si + GM_W]
    call gfx_u16_dec
    mov al, ' '
    mov [di], al
    inc di
    mov al, 'x'
    mov [di], al
    inc di
    mov al, ' '
    mov [di], al
    inc di
    mov ax, [si + GM_H]
    call gfx_u16_dec
    mov byte [di], 0
    mov si, se_label_buf
    ret

.auto:
    mov si, se_msg_auto
    ret

.maximum:
    mov di, se_label_buf
    mov si, se_msg_max
    call se_copy
    movzx bx, byte [gfx_mode_count]
    test bx, bx
    jz .max_empty
    dec bx
    imul bx, GM_SIZE
    mov si, gfx_modes
    add si, bx
    mov ax, [si + GM_W]
    call gfx_u16_dec
    mov al, 'x'
    mov [di], al
    inc di
    mov ax, [si + GM_H]
    call gfx_u16_dec
.max_empty:
    mov byte [di], 0
    mov si, se_label_buf
    ret

; ------------------------------------------------------------
; settings_hover_at - AX=x BX=y relative to client
; -> AL=row, SE_HIT_* or 0xFF
; ------------------------------------------------------------
settings_hover_at:
    push bx
    push cx
    push dx

    mov [se_mx], ax
    mov [se_my], bx
    call settings_layout

    ; buttons (only while a preview is pending)
    cmp byte [preview_pending], 0
    je .list
    mov ax, [se_my]
    mov cx, [se_h]
    sub cx, SE_FOOTER_H - 2
    cmp ax, cx
    jb .list
    add cx, SE_BTN_H
    cmp ax, cx
    jae .list

    mov ax, [se_mx]
    mov cx, [se_w]
    sub cx, SE_BTN_W + 8
    cmp ax, cx
    jb .try_cancel
    mov dx, cx
    add dx, SE_BTN_W
    cmp ax, dx
    jb .hit_apply
.try_cancel:
    sub cx, SE_BTN_W + 4
    cmp ax, cx
    jb .list
    mov dx, cx
    add dx, SE_BTN_W
    cmp ax, dx
    jb .hit_cancel
    jmp .list

.hit_apply:
    mov al, SE_HIT_APPLY
    jmp .done
.hit_cancel:
    mov al, SE_HIT_CANCEL
    jmp .done

.list:
    mov ax, [se_mx]
    cmp ax, SE_ROW_X
    jb .scroll
    mov cx, [se_w]
    sub cx, 8
    cmp ax, cx
    jae .scroll

    mov ax, [se_my]
    cmp ax, SE_LIST_Y
    jb .none
    mov cx, SE_LIST_Y
    add cx, [se_list_h]
    cmp ax, cx
    jae .scroll
    sub ax, SE_LIST_Y
    xor dx, dx
    mov cx, SE_ROW_H
    div cx
    add ax, [se_scroll]
    cmp ax, [se_total_rows]
    jae .none
    jmp .done

.scroll:
    mov cx, [se_total_rows]
    cmp cx, [se_vis_rows]
    jbe .none

    mov ax, [se_mx]
    mov cx, [se_w]
    sub cx, 10
    cmp ax, cx
    jb .none
    add cx, SE_SCROLL_W
    cmp ax, cx
    jae .none

    mov ax, [se_my]
    mov cx, [se_y]
    add cx, SE_LIST_Y
    cmp ax, cx
    jb .none
    mov dx, cx
    add dx, [se_list_h]
    cmp ax, dx
    jae .none

    add cx, 8
    cmp ax, cx
    jb .hit_up

    mov cx, [se_y]
    add cx, SE_LIST_Y
    add cx, [se_list_h]
    sub cx, 8
    cmp ax, cx
    jae .hit_down

    cmp ax, [se_thumb_y]
    jb .hit_pgup
    mov cx, [se_thumb_y]
    add cx, [se_thumb_h]
    cmp ax, cx
    jae .hit_pgdn
    jmp .none

.hit_up:
    mov al, SE_HIT_UP
    jmp .done
.hit_down:
    mov al, SE_HIT_DOWN
    jmp .done
.hit_pgup:
    mov al, SE_HIT_PGUP
    jmp .done
.hit_pgdn:
    mov al, SE_HIT_PGDN
    jmp .done
.none:
    mov al, SE_HIT_NONE
.done:
    pop dx
    pop cx
    pop bx
    ret

; ------------------------------------------------------------
; settings_click - AX=x BX=y relative to client
; ------------------------------------------------------------
settings_click:
    pusha
    call settings_hover_at
    cmp al, SE_HIT_NONE
    je .done
    cmp al, SE_HIT_APPLY
    je .apply
    cmp al, SE_HIT_CANCEL
    je .cancel
    cmp al, SE_HIT_UP
    je .up
    cmp al, SE_HIT_DOWN
    je .down
    cmp al, SE_HIT_PGUP
    je .pgup
    cmp al, SE_HIT_PGDN
    je .pgdn
    call settings_select_row
    jmp .done

.apply:
    call settings_commit_preview
    jmp .done
.cancel:
    call settings_cancel_preview
    jmp .done

.up:
    cmp word [se_scroll], 0
    je .refresh
    dec word [se_scroll]
    jmp .refresh
.down:
    mov ax, [se_total_rows]
    sub ax, [se_vis_rows]
    cmp [se_scroll], ax
    jae .refresh
    inc word [se_scroll]
    jmp .refresh
.pgup:
    mov ax, [se_vis_rows]
    cmp [se_scroll], ax
    jae .pgup_sub
    mov word [se_scroll], 0
    jmp .refresh
.pgup_sub:
    sub [se_scroll], ax
    jmp .refresh
.pgdn:
    mov ax, [se_total_rows]
    sub ax, [se_vis_rows]
    mov bx, [se_scroll]
    add bx, [se_vis_rows]
    cmp bx, ax
    jbe .pgdn_store
    mov bx, ax
.pgdn_store:
    mov [se_scroll], bx
.refresh:
    mov byte [set_hover], SE_HIT_NONE
    mov byte [scene_dirty], 1
.done:
    popa
    ret

; ------------------------------------------------------------
; settings_select_row - AL=row, start a preview of that mode
; ------------------------------------------------------------
settings_select_row:
    pusha
    movzx ax, al
    call settings_layout
    mov cx, [se_total_rows]
    test cx, cx
    jz .done
    cmp ax, cx
    jb .row_ok
    mov ax, cx
    dec ax
.row_ok:
    mov [se_tmp_row], ax

    ; remember the state before the preview session
    cmp byte [preview_pending], 0
    jne .prev_saved
    mov al, [display_locked]
    mov [preview_prev_locked], al
    mov al, [display_preset]
    mov [preview_prev_choice], al
    mov ax, [SCREEN_W]
    mov [preview_prev_w], ax
    mov ax, [SCREEN_H]
    mov [preview_prev_h], ax
.prev_saved:

    ; resolve the target size
    mov ax, [se_tmp_row]
    cmp ax, 0
    je .auto
    cmp ax, 1
    je .maximum
    sub ax, 2
    jmp .mode_at
.maximum:
    movzx ax, byte [gfx_mode_count]
    dec ax
.mode_at:
    imul ax, GM_SIZE
    mov si, gfx_modes
    add si, ax
    mov ax, [si + GM_W]
    mov bx, [si + GM_H]
    jmp .apply_mode
.auto:
    cmp byte [auto_valid], 0
    je .store
    mov ax, [auto_w]
    mov bx, [auto_h]
.apply_mode:
    test ax, ax
    jz .store
    test bx, bx
    jz .store
    call display_apply_size

.store:
    mov ax, [se_tmp_row]
    mov [preview_choice], al
    mov byte [preview_pending], 1
    mov eax, [tick_count]
    add eax, SE_PREVIEW_TICKS
    mov [preview_deadline], eax
    mov byte [preview_secs], 10

    mov ax, [se_tmp_row]
    call settings_scroll_to
    mov byte [set_hover], SE_HIT_NONE
    mov byte [scene_dirty], 1
.done:
    popa
    ret

; ------------------------------------------------------------
; settings_commit_preview - keep the previewed mode
; ------------------------------------------------------------
settings_commit_preview:
    cmp byte [preview_pending], 0
    je .done
    mov byte [preview_pending], 0

    mov al, [preview_choice]
    cmp al, 0
    je .auto

    mov byte [display_locked], 1
    mov [display_preset], al
    call serial_send_lock
    jmp .refresh

.auto:
    mov byte [display_locked], 0
    mov byte [display_preset], 0
    call serial_send_auto
    cmp byte [auto_valid], 0
    je .refresh
    mov ax, [auto_w]
    mov bx, [auto_h]
    call display_apply_size

.refresh:
    mov byte [set_hover], SE_HIT_NONE
    movzx ax, byte [display_preset]
    call settings_scroll_to
    mov byte [scene_dirty], 1
.done:
    ret

; ------------------------------------------------------------
; settings_cancel_preview - restore the mode before the preview
; ------------------------------------------------------------
settings_cancel_preview:
    cmp byte [preview_pending], 0
    je .done
    mov byte [preview_pending], 0

    mov al, [preview_prev_locked]
    mov [display_locked], al
    mov al, [preview_prev_choice]
    mov [display_preset], al

    mov ax, [preview_prev_w]
    test ax, ax
    jz .refresh
    mov bx, [preview_prev_h]
    test bx, bx
    jz .refresh
    cmp ax, [SCREEN_W]
    jne .restore
    cmp bx, [SCREEN_H]
    je .refresh
.restore:
    call display_apply_size

.refresh:
    mov byte [set_hover], SE_HIT_NONE
    movzx ax, byte [display_preset]
    call settings_scroll_to
    mov byte [scene_dirty], 1
.done:
    ret

; ------------------------------------------------------------
; settings_tick - count down the preview, roll back on timeout
; Called from the main loop (100 Hz).
; ------------------------------------------------------------
settings_tick:
    cmp byte [preview_pending], 0
    je .done

    mov eax, [tick_count]
    cmp eax, [preview_deadline]
    jae .timeout

    mov ebx, [preview_deadline]
    sub ebx, eax
    add ebx, 99
    mov eax, ebx
    xor edx, edx
    mov ebx, 100
    div ebx
    cmp al, [preview_secs]
    je .done
    mov [preview_secs], al
    mov byte [scene_dirty], 1
    ret

.timeout:
    call settings_cancel_preview
.done:
    ret

; ------------------------------------------------------------
; settings_move_sel - AL=0xFF up / 0x01 down (keyboard)
; ------------------------------------------------------------
settings_move_sel:
    pusha
    mov bl, al
    call settings_layout

    mov al, [display_preset]
    cmp byte [preview_pending], 0
    je .base_ready
    mov al, [preview_choice]
.base_ready:
    movzx ax, al
    cmp ax, [se_total_rows]
    jb .base_ok
    mov ax, [se_total_rows]
    dec ax
.base_ok:
    cmp bl, 0xFF
    jne .down
    test ax, ax
    jz .done
    dec ax
    jmp .go
.down:
    inc ax
    cmp ax, [se_total_rows]
    jb .go
    mov ax, [se_total_rows]
    dec ax
.go:
    call settings_select_row
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

    ; generous default size, clamped to the logical desktop
    mov ax, 380
    mov cx, [UI_W]
    sub cx, 8
    cmp ax, cx
    jbe .w_ok
    mov ax, cx
.w_ok:
    mov [bx + W_W], ax

    mov ax, 280
    mov cx, [taskbar_y]
    sub cx, 8
    cmp ax, cx
    jbe .h_ok
    mov ax, cx
.h_ok:
    mov [bx + W_H], ax

    mov ax, [UI_W]
    sub ax, [bx + W_W]
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
    sub ax, [bx + W_H]
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

    ; use the clamped size for the first layout and show the
    ; currently selected row
    mov ax, [bx + W_W]
    mov [se_w], ax
    mov ax, [bx + W_H]
    sub ax, 15
    mov [se_h], ax
    call settings_layout
    movzx ax, byte [display_preset]
    call settings_scroll_to

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
; display_apply_auto - AX=width BX=height from the QEMU helper
; Ignored while a manual preview is active or auto is locked.
; ------------------------------------------------------------
display_apply_auto:
    pusha
    cmp byte [preview_pending], 0
    jne .done
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
; display_apply_size - AX=width BX=height, switch the real mode
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
display_locked       db 0
display_preset       db 0
auto_valid           db 0
auto_w               dw 0
auto_h               dw 0

preview_pending      db 0
preview_choice       db 0
preview_prev_choice  db 0
preview_prev_locked  db 0
preview_prev_w       dw 0
preview_prev_h       dw 0
preview_deadline     dd 0
preview_secs         db 10

se_x                 dw 0
se_y                 dw 0
se_w                 dw 380
se_h                 dw 280
se_row               dw 0
se_abs_row           dw 0
se_rowy              dw 0
se_row_w             dw 0
se_list_h            dw 0
se_total_rows        dw 0
se_vis_rows          dw 0
se_scroll            dw 0
se_tmp_row           dw 0
se_text_color        db 0
se_sel_flag          db 0
se_mx                dw 0
se_my                dw 0
se_bar_x             dw 0
se_track_h           dw 0
se_thumb_y           dw 0
se_thumb_h           dw 0
se_btn_x             dw 0
se_btn_cx            dw 0
se_btn_y             dw 0
se_label_buf         times 20 db 0
se_status_buf        times 24 db 0
se_side_buf          times 16 db 0

se_msg_settings      db "SETTINGS", 0
se_msg_display       db "DISPLAY", 0
se_msg_video         db "VIDEO", 0
se_msg_backend_bochs db "BOCHS", 0
se_msg_backend_vbe   db "VBE", 0
se_msg_backend_none  db "-", 0
se_msg_modes         db " MODES", 0
se_msg_auto          db "AUTO (WINDOW)", 0
se_msg_max           db "MAX ", 0
se_msg_manual        db "MANUAL", 0
se_msg_linked        db "AUTO - LINKED", 0
se_msg_waiting       db "AUTO - NO HELPER", 0
se_msg_preview       db "PREVIEW", 0
se_msg_apply         db "APPLY", 0
se_msg_cancel        db "CANCEL", 0
