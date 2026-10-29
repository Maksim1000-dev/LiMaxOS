; ============================================================
;  LiMaxOS kernel / graphical desktop (CORE.SYS)
;  16-bit real mode + Bochs VBE linear framebuffer, PS/2 mouse,
;  NASM.  All BIOS calls happen before gfx_enter_unreal.
; ============================================================
[BITS 16]
[ORG 0x0000]
[CPU 386]

; ---- window structure (see ui.inc) ----
W_STATE         equ 0
W_X             equ 2
W_Y             equ 4
W_W             equ 6
W_H             equ 8
W_SX            equ 10
W_SY            equ 12
W_SW            equ 14
W_SH            equ 16
W_PAGE          equ 18
W_SIZE          equ 20

WIN_TERM        equ 0
WIN_SET         equ 1

; ---- 32-byte image header (patched by mkimg.exe) ----
hdr_magic:      db "LiMaxOSK"
hdr_sectors:    dw 0
hdr_load_seg:   dw 0x2000
hdr_entry:      dw 0x0020
hdr_size:       dw 0
hdr_checksum:   dw 0
hdr_version:    dw 0x0100
                times 32 - ($ - $$) db 0

; ============================================================
;  entry point
; ============================================================
start:
    cli
    mov ax, 0x3000
    mov ss, ax
    mov sp, 0xFFFE
    cld

    mov ax, cs
    mov ds, ax
    mov es, ax

    ; ---- catch CPU exceptions before anything else ----
    call install_exceptions

    ; ---- everything that needs the BIOS, before unreal mode ----
    call detect_memory
    call gfx_load_font
    call gfx_probe
    jnc .gfx_ok
    mov ax, [gfx_probe_error]
    test ax, ax
    jnz .gfx_error
    mov ax, ERR_NO_VBE
.gfx_error:
    jmp gsod
.gfx_ok:

    call gfx_enter_unreal

    ; ---- real VBE mode: start at 1280x720, AUTO may replace it ----
    mov ax, 1280
    mov bx, 720
    call gfx_set_mode
    jnc .mode_ok
    mov ax, ERR_VBE_MODE
    jmp gsod
.mode_ok:

    ; ---- serial link for the QEMU window-size helper ----
    call serial_init
    call serial_send_hello
    call serial_send_mode

    ; ---- interrupt controllers, timer, keyboard, mouse ----
    call irq_remap

    ; vectors 8..15 are safe now that the PIC moved to 0x20..0x2F
    mov si, ex_stub_table + 8 * 2
    mov di, 8 * 4
    mov cx, 8
    call install_exception_vectors

    call kbd_init
    call mouse_init
    jnc .mouse_ok
    mov ax, ERR_MOUSE_INIT
    jmp gsod
.mouse_ok:
    call irq_enable
    call kbd_flush

    ; ---- desktop subsystems (interrupts stay off during init) ----
    call term_init
    call rtc_update
    call ui_init

    sti

    ; first complete frame
    call ui_compose
    call blit
    mov byte [cursor_drawn], 0
    call update_cursor_visual
    mov byte [need_cursor], 0

; ============================================================
;  main loop
; ============================================================
main_loop:
    call serial_poll

    ; real-time clock: refresh once per second
    mov eax, [tick_count]
    mov ebx, [rtc_tick]
    sub eax, ebx
    cmp eax, 100
    jb .no_rtc
    mov eax, [tick_count]
    mov [rtc_tick], eax
    call rtc_update
    jnc .no_rtc
    mov byte [scene_dirty], 1
.no_rtc:

    ; resolution preview countdown (10 s rollback)
    call settings_tick

    ; terminal cursor blink (~0.64 s)
    mov eax, [tick_count]
    shr eax, 6
    and al, 1
    cmp al, [term_blink]
    je .no_blink
    mov [term_blink], al
    mov byte [scene_dirty], 1
.no_blink:

    ; window drag / terminal resize (functions no-op when idle)
    call ui_drag_window
    call ui_resize_window

    ; mouse movement
    cmp byte [mouse_moved], 0
    je .no_move
    mov byte [mouse_moved], 0
    call ui_update_hover
    mov byte [need_cursor], 1
.no_move:

    ; mouse press
    cmp byte [mouse_pressed], 0
    je .no_press
    mov byte [mouse_pressed], 0
    call ui_click
    mov byte [need_cursor], 1
.no_press:

    ; mouse release
    cmp byte [mouse_released], 0
    je .no_release
    mov byte [mouse_released], 0
    mov byte [dragging], 0
    mov byte [resizing], 0
.no_release:

    ; drain the keyboard ring
.key_loop:
    call ui_key
    jc .key_loop

    ; rebuild the scene only when something changed
    cmp byte [scene_dirty], 0
    je .no_scene
    call ui_compose
    call blit
    mov byte [cursor_drawn], 0
    mov byte [scene_dirty], 0
    mov byte [need_cursor], 1
.no_scene:

    ; draw the pointer after the scene
    cmp byte [need_cursor], 0
    je .no_cursor
    call update_cursor_visual
    mov byte [need_cursor], 0
.no_cursor:

    hlt
    jmp main_loop

; ============================================================
;  detect_memory - cache INT 12h / INT 15h E801 values
;  (BIOS calls are impossible after gfx_enter_unreal)
; ============================================================
detect_memory:
    pusha
    push ds
    push es

    mov word [mem_conv_kb], 640
    mov word [mem_ext_kb], 0

    int 0x12
    test ax, ax
    jz .extended
    mov [mem_conv_kb], ax

.extended:
    xor cx, cx
    xor dx, dx
    mov ax, 0xE801
    int 0x15
    jc .done

    ; Some BIOSes return the values in CX:DX instead of AX:BX.
    test ax, ax
    jnz .got
    test bx, bx
    jnz .got
    mov ax, cx
    mov bx, dx
.got:
    mov [mem_ext_low], ax
    mov ax, bx
    xor dx, dx
    mov cx, 64
    mul cx                  ; DX:AX = 64K blocks -> KB
    add ax, [mem_ext_low]
    adc dx, 0
    test dx, dx
    jz .store
    mov ax, 0xFFFF
.store:
    mov [mem_ext_kb], ax

.done:
    pop es
    pop ds
    popa
    ret

; ============================================================
;  cursor compositing (restore the old area from the back buffer)
; ============================================================
update_cursor_visual:
    cli
    mov ax, [mouse_x]
    mov bx, [mouse_y]
    mov [cursor_new_x], ax
    mov [cursor_new_y], bx
    sti
    cmp byte [cursor_drawn], 0
    je .draw
    mov ax, [cursor_old_x]
    shl ax, 1
    mov bx, [cursor_old_y]
    shl bx, 1
    mov cx, 20
    mov dx, 28
    call copy_buffer_rect
.draw:
    call draw_cursor
    mov ax, [cursor_new_x]
    mov [cursor_old_x], ax
    mov ax, [cursor_new_y]
    mov [cursor_old_y], ax
    mov byte [cursor_drawn], 1
    ret

; ============================================================
;  system_reboot - keyboard-controller reset, then triple fault
;  No BIOS after unreal mode.
; ============================================================
system_reboot:
    cli
    mov al, 0xFE
    out 0x64, al
    mov cx, 0xFFFF
.wait:
    loop .wait
    lidt [null_idt]
    int 3
.hang:
    hlt
    jmp .hang

; ============================================================
;  system_poweroff - ACPI PM1a control (QEMU i440fx / q35)
; ============================================================
system_poweroff:
    cli
    mov ax, 0x2000
    mov dx, 0x604
    out dx, ax
    mov dx, 0xB004
    out dx, ax
.hang:
    hlt
    jmp .hang

; ============================================================
;  kernel data
; ============================================================
scene_dirty     db 1
need_cursor     db 1
cursor_drawn    db 0
cursor_old_x    dw 160
cursor_old_y    dw 90
cursor_new_x    dw 160
cursor_new_y    dw 90
rtc_tick        dd 0
mem_conv_kb     dw 640
mem_ext_kb      dw 0
mem_ext_low     dw 0
null_idt        dw 0
                dd 0

; ============================================================
;  subsystems
; ============================================================
%include "kernel/gfx.inc"
%include "kernel/irq.inc"
%include "kernel/mouse.inc"
%include "kernel/rtc.inc"
%include "kernel/serial.inc"
%include "kernel/term.inc"
%include "kernel/settings.inc"
%include "kernel/ui.inc"
%include "include/errors.inc"
%include "kernel/gsod.inc"
