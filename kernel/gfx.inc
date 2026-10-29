; ============================================================
;  gfx.inc - VBE 8bpp linear framebuffer, UI scale x2
;
;  Back buffer: linear 0x00400000 (up to 3840x2160 bytes)
;  Font buffer: 0x5000:0000 (256 chars x 8 bytes)
;
;  After gfx_enter_unreal ES/FS/GS are flat 4 GB selectors.
;  They must never be reloaded while the CPU is in real mode.
;  DS stays the kernel data segment (0x2000).
; ============================================================

FONT_SEG        equ 0x5000
BUF_BASE        equ 0x00400000
UI_SCALE        equ 2
VBE_INDEX_PORT  equ 0x01CE
VBE_DATA_PORT   equ 0x01CF

GFX_BACKEND_NONE  equ 0
GFX_BACKEND_BOCHS equ 1
GFX_BACKEND_VBE   equ 2

GFX_MAX_MODES   equ 32
GM_MODE         equ 0
GM_W            equ 2
GM_H            equ 4
GM_PITCH        equ 6
GM_LFB          equ 8
GM_SIZE         equ 12

; ------------------------------------------------------------
; gfx_enter_unreal - load flat ES/FS/GS and return to real mode
; ------------------------------------------------------------
gfx_enter_unreal:
    cli
    lgdt [gdt_desc]

    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 0x10:pm_entry

    ; 16-bit protected mode code segment (D=0), so the far jump
    ; back to real mode is decoded with 16-bit offsets.
pm_entry:
    mov ax, 0x08
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov eax, cr0
    and al, 0xFE
    mov cr0, eax
    jmp 0x2000:unreal_ret

unreal_ret:
    mov ax, 0x2000
    mov ds, ax
    ; ES/FS/GS keep their flat 4 GB hidden descriptors.
    ret

; ------------------------------------------------------------
; vbe_write - AX = index, BX = value
; ------------------------------------------------------------
vbe_write:
    push ax
    push dx
    mov dx, VBE_INDEX_PORT
    out dx, ax
    inc dx
    mov ax, bx
    out dx, ax
    pop dx
    pop ax
    ret

; ------------------------------------------------------------
; vbe_read - AX = index -> AX = value
; ------------------------------------------------------------
vbe_read:
    push dx
    mov dx, VBE_INDEX_PORT
    out dx, ax
    inc dx
    in ax, dx
    pop dx
    ret

; ------------------------------------------------------------
; a20_enable - make sure the A20 gate is open (fast A20)
; ------------------------------------------------------------
a20_enable:
    push ax
    in al, 0x92
    test al, 2
    jnz .done
    or al, 2
    and al, 0xFE
    out 0x92, al
.done:
    pop ax
    ret

; ------------------------------------------------------------
; vbe_bios_int10 - run INT 10h with DS/ES = kernel segment
;
; Input registers are passed straight to the BIOS.  AX returns
; the BIOS result.  ES is left at 0x2000, so the caller must run
; gfx_enter_unreal before using the flat ES/FS/GS descriptors.
; ------------------------------------------------------------
vbe_bios_int10:
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds

    push cs
    pop ds
    push cs
    pop es
    int 0x10

    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

; ------------------------------------------------------------
; pci_find_bochs - find QEMU std VGA 1234:1111, read BAR0 and
; verify the Bochs dispi interface.  Also caches its VRAM size.
; ------------------------------------------------------------
pci_find_bochs:
    push eax
    push ebx
    push ecx
    push edx

    mov dword [lfb_base], 0xE0000000
    mov byte [vga_found], 0
    mov byte [bochs_ok], 0
    xor ebx, ebx
    mov cx, 32
.device:
    mov eax, ebx
    shl eax, 11
    or eax, 0x80000000
    mov dx, 0x0CF8
    out dx, eax
    mov dx, 0x0CFC
    in eax, dx
    cmp ax, 0x1234
    jne .next
    shr eax, 16
    cmp ax, 0x1111
    jne .next

    ; BAR0
    mov eax, ebx
    shl eax, 11
    or eax, 0x80000010
    mov dx, 0x0CF8
    out dx, eax
    mov dx, 0x0CFC
    in eax, dx
    and eax, 0xFFFFFFF0
    test eax, eax
    jz .next
    mov [lfb_base], eax
    mov byte [vga_found], 1

    ; enable memory space (and keep I/O space enabled)
    mov eax, ebx
    shl eax, 11
    or eax, 0x80000004
    mov dx, 0x0CF8
    out dx, eax
    mov dx, 0x0CFC
    in ax, dx
    or ax, 0x0003
    out dx, ax

    ; the dispi interface must answer with 0xB0Cx
    xor ax, ax
    call vbe_read
    mov bx, ax
    and bx, 0xFFF0
    cmp bx, 0xB0C0
    jne .done
    mov byte [bochs_ok], 1

    ; video memory size (64K blocks)
    mov ax, 0x0A
    call vbe_read
    test ax, ax
    jz .mem_default
    movzx eax, ax
    shl eax, 6
    mov [vbe_mem_kb], eax
    jmp .done
.mem_default:
    mov dword [vbe_mem_kb], 16384

.next:
    inc bx
    dec cx
    jnz .device

.done:
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; ------------------------------------------------------------
; pci_find_any_vga - is there any PCI display controller?
; Used only to choose between error 300 and error 306.
; ------------------------------------------------------------
pci_find_any_vga:
    push eax
    push ebx
    push ecx
    push edx
    mov byte [any_vga_found], 0
    xor ebx, ebx
    mov cx, 32
.loop:
    mov eax, ebx
    shl eax, 11
    or eax, 0x80000000
    mov dx, 0x0CF8
    out dx, eax
    mov dx, 0x0CFC
    in eax, dx
    cmp ax, 0xFFFF
    je .next

    mov eax, ebx
    shl eax, 11
    or eax, 0x80000008
    mov dx, 0x0CF8
    out dx, eax
    mov dx, 0x0CFC
    in eax, dx
    shr eax, 24              ; class code
    cmp al, 0x03
    jne .next
    mov byte [any_vga_found], 1
    jmp .done

.next:
    inc bx
    loop .loop
.done:
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; ------------------------------------------------------------
; gfx_probe - choose the Bochs dispi backend or the VBE backend
; and build the list of usable 8bpp linear modes.
; CF=1 on failure, AX=error code in gfx_probe_error.
; Must be called before gfx_enter_unreal.
; ------------------------------------------------------------
gfx_probe:
    pusha
    mov byte [gfx_backend], GFX_BACKEND_NONE
    mov byte [gfx_mode_count], 0
    mov byte [vbe_ok], 0
    mov byte [bochs_ok], 0
    mov dword [vbe_mem_kb], 0
    mov word [gfx_probe_error], ERR_NO_VBE

    call a20_enable
    call pci_find_bochs
    call vbe_probe

    cmp byte [bochs_ok], 0
    je .try_vbe
    mov byte [gfx_backend], GFX_BACKEND_BOCHS
    call gfx_synth_modes
    cmp byte [gfx_mode_count], 0
    je .no_lfb
    clc
    popa
    ret

.try_vbe:
    cmp byte [vbe_ok], 0
    je .no_vbe
    cmp byte [gfx_mode_count], 0
    je .no_lfb
    mov byte [gfx_backend], GFX_BACKEND_VBE
    clc
    popa
    ret

.no_vbe:
    call pci_find_any_vga
    cmp byte [any_vga_found], 0
    je .no_vga
    mov word [gfx_probe_error], ERR_NO_VBE
    jmp .fail
.no_vga:
    mov word [gfx_probe_error], ERR_VGA_DEVICE
    jmp .fail
.no_lfb:
    mov word [gfx_probe_error], ERR_LFB
.fail:
    stc
    popa
    ret

; ------------------------------------------------------------
; vbe_probe - VESA VBE 4F00/4F01 scan for 8bpp LFB modes
; ------------------------------------------------------------
vbe_probe:
    pusha
    push ds
    push es
    push fs

    mov byte [vbe_ok], 0

    mov ax, 0x2000
    mov es, ax
    mov di, vbe_info
    mov ax, 0x4F00
    int 0x10
    cmp ax, 0x004F
    jne .fail
    cmp dword [vbe_info], 'VESA'
    jne .fail

    mov al, [vbe_info+5]
    mov [vbe_major], al

    movzx eax, word [vbe_info+0x12]
    test eax, eax
    jz .mem_default
    shl eax, 6
    mov [vbe_mem_kb], eax
.mem_default:
    cmp dword [vbe_mem_kb], 0
    jne .mem_ok
    mov dword [vbe_mem_kb], 16384
.mem_ok:

    mov si, [vbe_info+0x0E]
    mov ax, [vbe_info+0x10]
    mov [vbe_mode_seg], ax
    mov [vbe_mode_off], si
    mov fs, ax
    mov si, [vbe_mode_off]

.mode_loop:
    mov ax, [fs:si]
    add si, 2
    cmp ax, 0xFFFF
    je .modes_done
    mov [vbe_cur_mode], ax

    push si
    push fs
    mov ax, 0x2000
    mov ds, ax
    mov es, ax
    mov cx, [vbe_cur_mode]
    mov di, vbe_mode_info
    mov ax, 0x4F01
    int 0x10
    pop fs
    pop si
    mov [vbe_result], ax
    mov ax, 0x2000
    mov ds, ax
    mov es, ax
    cmp word [vbe_result], 0x004F
    jne .mode_loop
    call vbe_accept_mode
    jmp .mode_loop

.modes_done:
    call gfx_sort_modes
    cmp byte [gfx_mode_count], 0
    je .fail
    mov byte [vbe_ok], 1
    pop fs
    pop es
    pop ds
    popa
    clc
    ret
.fail:
    mov byte [vbe_ok], 0
    pop fs
    pop es
    pop ds
    popa
    stc
    ret

; ------------------------------------------------------------
; vbe_accept_mode - validate vbe_mode_info and add it to the table
; ------------------------------------------------------------
vbe_accept_mode:
    pusha

    mov dx, [vbe_mode_info+0x00]
    test dx, 0x0001
    jz .done
    test dx, 0x0010
    jz .done
    test dx, 0x0080
    jz .done
    cmp byte [vbe_mode_info+0x19], 8
    jne .done
    cmp byte [vbe_mode_info+0x1B], 4
    jne .done

    mov ax, [vbe_mode_info+0x12]
    mov bx, [vbe_mode_info+0x14]
    test ax, 1
    jnz .done
    test bx, 1
    jnz .done
    cmp ax, 640
    jb .done
    cmp bx, 360
    jb .done
    cmp ax, 3840
    ja .done
    cmp bx, 2160
    ja .done
    mov [vbe_tmp_w], ax
    mov [vbe_tmp_h], bx

    mov cx, [vbe_mode_info+0x10]
    cmp byte [vbe_major], 3
    jb .pitch
    mov dx, [vbe_mode_info+0x32]
    test dx, dx
    jz .pitch
    mov cx, dx
.pitch:
    cmp cx, ax
    jb .done
    mov [vbe_tmp_pitch], cx

    mov edx, [vbe_mode_info+0x28]
    test edx, edx
    jz .done
    mov [vbe_tmp_lfb], edx

    movzx eax, word [vbe_tmp_pitch]
    movzx ebx, word [vbe_tmp_h]
    imul eax, ebx
    mov [vbe_tmp_bytes], eax

    ; must fit in the video memory reported by the BIOS
    mov ebx, eax
    add ebx, 1023
    shr ebx, 10
    cmp ebx, [vbe_mem_kb]
    ja .done

    ; must fit in RAM above the 4 MB back buffer
    movzx eax, word [mem_ext_kb]
    add eax, 1024
    shl eax, 10
    sub eax, BUF_BASE
    jbe .done
    cmp [vbe_tmp_bytes], eax
    ja .done

    ; deduplicate by resolution
    mov si, gfx_modes
    movzx cx, byte [gfx_mode_count]
.dedup:
    jcxz .add
    mov ax, [vbe_tmp_w]
    cmp ax, [si+GM_W]
    jne .dedup_next
    mov ax, [vbe_tmp_h]
    cmp ax, [si+GM_H]
    je .done
.dedup_next:
    add si, GM_SIZE
    loop .dedup

.add:
    cmp byte [gfx_mode_count], GFX_MAX_MODES
    jae .done
    mov ax, [vbe_cur_mode]
    mov [si+GM_MODE], ax
    mov ax, [vbe_tmp_w]
    mov [si+GM_W], ax
    mov ax, [vbe_tmp_h]
    mov [si+GM_H], ax
    mov ax, [vbe_tmp_pitch]
    mov [si+GM_PITCH], ax
    mov eax, [vbe_tmp_lfb]
    mov [si+GM_LFB], eax
    inc byte [gfx_mode_count]
.done:
    popa
    ret

; ------------------------------------------------------------
; gfx_synth_modes - mode list for the Bochs dispi backend
; ------------------------------------------------------------
gfx_synth_modes:
    pusha
    mov byte [gfx_mode_count], 0

    movzx eax, word [mem_ext_kb]
    add eax, 1024
    shl eax, 10
    sub eax, BUF_BASE
    jbe .done
    mov [synth_buf_bytes], eax

    mov eax, [vbe_mem_kb]
    test eax, eax
    jnz .vram_ok
    mov eax, 16384
.vram_ok:
    shl eax, 10
    mov [synth_vram_bytes], eax

    mov si, bochs_mode_tab
.loop:
    mov ax, [si]
    test ax, ax
    jz .sort
    mov bx, [si+2]
    add si, 4
    mov [synth_w], ax
    mov [synth_h], bx

    movzx eax, word [synth_w]
    movzx ebx, word [synth_h]
    imul eax, ebx
    cmp eax, [synth_vram_bytes]
    ja .loop
    cmp eax, [synth_buf_bytes]
    ja .loop
    cmp byte [gfx_mode_count], GFX_MAX_MODES
    jae .sort

    mov di, gfx_modes
    movzx cx, byte [gfx_mode_count]
    imul cx, GM_SIZE
    add di, cx
    mov word [di+GM_MODE], 0
    mov ax, [synth_w]
    mov [di+GM_W], ax
    mov [di+GM_PITCH], ax
    mov ax, [synth_h]
    mov [di+GM_H], ax
    mov eax, [lfb_base]
    mov [di+GM_LFB], eax
    inc byte [gfx_mode_count]
    jmp .loop

.sort:
    call gfx_sort_modes
.done:
    popa
    ret

; ------------------------------------------------------------
; gfx_sort_modes - sort the table by ascending pixel count
; ------------------------------------------------------------
gfx_sort_modes:
    pusha
    movzx cx, byte [gfx_mode_count]
    cmp cx, 2
    jb .done
    mov word [sort_i], 0
.outer:
    mov ax, [sort_i]
    inc ax
    cmp ax, cx
    jae .done

    mov bx, [sort_i]
    imul bx, GM_SIZE
    mov si, gfx_modes
    add si, bx
    mov di, si
    mov dx, [sort_i]
    inc dx
.inner:
    cmp dx, cx
    jae .inner_done

    mov ax, dx
    imul ax, GM_SIZE
    mov bx, gfx_modes
    add bx, ax

    movzx eax, word [bx+GM_W]
    movzx ebp, word [bx+GM_H]
    imul eax, ebp
    mov [sort_area], eax
    movzx eax, word [di+GM_W]
    movzx ebp, word [di+GM_H]
    imul eax, ebp
    cmp [sort_area], eax
    jae .no_better
    mov di, bx
.no_better:
    inc dx
    jmp .inner

.inner_done:
    cmp di, si
    je .no_swap
    mov ax, [si+0]
    mov dx, [di+0]
    mov [si+0], dx
    mov [di+0], ax
    mov ax, [si+2]
    mov dx, [di+2]
    mov [si+2], dx
    mov [di+2], ax
    mov ax, [si+4]
    mov dx, [di+4]
    mov [si+4], dx
    mov [di+4], ax
    mov ax, [si+6]
    mov dx, [di+6]
    mov [si+6], dx
    mov [di+6], ax
    mov ax, [si+8]
    mov dx, [di+8]
    mov [si+8], dx
    mov [di+8], ax
    mov ax, [si+10]
    mov dx, [di+10]
    mov [si+10], dx
    mov [di+10], ax
.no_swap:
    inc word [sort_i]
    jmp .outer
.done:
    popa
    ret

; ------------------------------------------------------------
; gfx_find_mode - AX=w BX=h -> AX=table index, CF=1 if none
; Exact match first, then the closest pixel count.
; ------------------------------------------------------------
gfx_find_mode:
    pusha
    mov [find_req_w], ax
    mov [find_req_h], bx
    cmp byte [gfx_mode_count], 0
    je .none

    mov si, gfx_modes
    movzx cx, byte [gfx_mode_count]
    xor di, di
.exact:
    cmp ax, [si+GM_W]
    jne .exact_next
    cmp bx, [si+GM_H]
    je .found
.exact_next:
    add si, GM_SIZE
    inc di
    loop .exact

    mov dword [find_best_diff], 0xFFFFFFFF
    mov word [find_best], 0
    movzx eax, word [find_req_w]
    movzx ebx, word [find_req_h]
    imul eax, ebx
    mov [find_req_area], eax

    mov si, gfx_modes
    movzx cx, byte [gfx_mode_count]
    xor di, di
.nearest:
    movzx eax, word [si+GM_W]
    movzx ebx, word [si+GM_H]
    imul eax, ebx
    sub eax, [find_req_area]
    jns .diff_ok
    neg eax
.diff_ok:
    cmp eax, [find_best_diff]
    jae .next
    mov [find_best_diff], eax
    mov [find_best], di
.next:
    add si, GM_SIZE
    inc di
    loop .nearest

    mov ax, [find_best]
    mov [find_result], ax
    popa
    mov ax, [find_result]
    clc
    ret

.found:
    mov [find_result], di
    popa
    mov ax, [find_result]
    clc
    ret

.none:
    popa
    stc
    ret

; ------------------------------------------------------------
; gfx_set_mode - AX = width, BX = height
; CF=1 on failure.  Dispatches to Bochs dispi or VBE BIOS.
; ------------------------------------------------------------
gfx_set_mode:
    pusha
    pushf
    pop cx
    and cl, 2
    mov [gfx_if_saved], cl

    and ax, 0xFFF8          ; xres must be a multiple of 8
    and bx, 0xFFFE
    cmp ax, 640
    jae .w_min
    mov ax, 640
.w_min:
    cmp ax, 3840
    jbe .w_max
    mov ax, 3840
.w_max:
    cmp bx, 360
    jae .h_min
    mov bx, 360
.h_min:
    cmp bx, 2160
    jbe .h_max
    mov bx, 2160
.h_max:
    mov [new_w], ax
    mov [new_h], bx

    cmp ax, [SCREEN_W]
    jne .set
    cmp bx, [SCREEN_H]
    jne .set
    clc
    popa
    ret

.set:
    cli
    cmp byte [gfx_backend], GFX_BACKEND_BOCHS
    jne .vbe
    call bochs_set_mode
    jmp .after
.vbe:
    call vbe_set_mode
.after:
    jc .fail
    call gfx_mode_changed
    clc
    popa
    ret

.fail:
    cmp byte [gfx_if_saved], 0
    je .fail_if
    sti
.fail_if:
    cmp word [SCREEN_W], 0
    jne .fail_keep
    mov byte [gfx_ready], 0
.fail_keep:
    stc
    popa
    ret

; ------------------------------------------------------------
; bochs_set_mode - Bochs/QEMU dispi backend
; ------------------------------------------------------------
bochs_set_mode:
    pusha

    ; verify the Bochs VBE interface (error 300)
    xor ax, ax
    call vbe_read
    mov bx, ax
    and bx, 0xFFF0
    cmp bx, 0xB0C0
    jne .fail

    ; disable VBE before changing the mode
    mov ax, 4
    xor bx, bx
    call vbe_write

    mov ax, 1
    mov bx, [new_w]
    call vbe_write
    mov ax, 2
    mov bx, [new_h]
    call vbe_write
    mov ax, 3
    mov bx, 8
    call vbe_write
    mov ax, 6
    mov bx, [new_w]
    call vbe_write
    mov ax, 7
    mov bx, [new_h]
    call vbe_write
    mov ax, 8
    xor bx, bx
    call vbe_write
    mov ax, 9
    xor bx, bx
    call vbe_write

    ; enable + linear framebuffer
    mov ax, 4
    mov bx, 0x41
    call vbe_write

    ; verify that the requested mode is really active (error 301)
    mov ax, 4
    call vbe_read
    test ax, 1
    jz .fail
    mov ax, 1
    call vbe_read
    cmp ax, [new_w]
    jne .fail
    mov ax, 2
    call vbe_read
    cmp ax, [new_h]
    jne .fail

    ; the PCI VGA BAR must be known (error 302)
    cmp byte [vga_found], 0
    je .fail

    mov ax, [new_w]
    mov [SCREEN_PITCH], ax
    clc
    popa
    ret

.fail:
    stc
    popa
    ret

; ------------------------------------------------------------
; vbe_set_mode - switch an 8bpp VBE LFB mode through the BIOS
; ------------------------------------------------------------
vbe_set_mode:
    pusha

    ; pick the nearest valid mode from our table
    mov ax, [new_w]
    mov bx, [new_h]
    call gfx_find_mode
    jc .fail
    mov [vbe_index], ax

    mov bx, ax
    imul bx, GM_SIZE
    mov si, gfx_modes
    add si, bx
    mov ax, [si+GM_MODE]
    mov [vbe_cur_mode], ax

    ; set the mode with the linear framebuffer bit
    mov ax, 0x4F02
    mov bx, [vbe_cur_mode]
    or bx, 0x4000
    call vbe_bios_int10
    mov [vbe_result], ax
    call a20_enable
    call gfx_enter_unreal
    cmp word [vbe_result], 0x004F
    jne .fail

    ; read back the mode that is really active
    mov ax, 0x4F01
    mov cx, [vbe_cur_mode]
    mov di, vbe_mode_info
    call vbe_bios_int10
    mov [vbe_result], ax
    call a20_enable
    call gfx_enter_unreal
    cmp word [vbe_result], 0x004F
    jne .fail

    mov ax, [vbe_mode_info+0x12]
    mov bx, [vbe_mode_info+0x14]
    test ax, ax
    jz .fail
    test bx, bx
    jz .fail
    test ax, 1
    jnz .fail
    test bx, 1
    jnz .fail
    cmp byte [vbe_mode_info+0x19], 8
    jne .fail
    mov [new_w], ax
    mov [new_h], bx

    mov cx, [vbe_mode_info+0x10]
    cmp byte [vbe_major], 3
    jb .pitch_ok
    mov dx, [vbe_mode_info+0x32]
    test dx, dx
    jz .pitch_ok
    mov cx, dx
.pitch_ok:
    cmp cx, ax
    jb .fail
    mov [SCREEN_PITCH], cx

    mov eax, [vbe_mode_info+0x28]
    test eax, eax
    jz .fail
    mov [lfb_base], eax

    movzx ebx, word [SCREEN_PITCH]
    movzx ecx, word [new_h]
    imul ebx, ecx
    add eax, ebx
    jc .fail

    clc
    popa
    ret

.fail:
    stc
    popa
    ret

; ------------------------------------------------------------
; gfx_mode_changed - publish the new geometry and clear the buffer
; ------------------------------------------------------------
gfx_mode_changed:
    pusha
    mov ax, [new_w]
    mov [SCREEN_W], ax
    shr ax, 1
    mov [UI_W], ax

    mov ax, [new_h]
    mov [SCREEN_H], ax
    shr ax, 1
    mov [UI_H], ax

    ; keep the pointer speed proportional to the logical desktop
    mov ax, [UI_W]
    xor dx, dx
    mov bx, 320
    div bx
    test ax, ax
    jnz .speed_ok
    mov ax, 1
.speed_ok:
    mov [mouse_speed], ax

    call gfx_update_mode_text
    mov byte [gfx_ready], 1

    ; the BIOS call ran with interrupts disabled; restore them
    ; before the relatively long palette upload and clear
    cmp byte [gfx_if_saved], 0
    je .no_if
    sti
.no_if:
    call gfx_set_palette

    xor ax, ax
    xor bx, bx
    mov cx, [UI_W]
    mov dx, [UI_H]
    xor si, si
    call fill_rect

    ; keep the logical pointer inside the new logical surface
    mov ax, [UI_W]
    sub ax, 9
    cmp [mouse_x], ax
    jbe .mx
    mov [mouse_x], ax
.mx:
    mov ax, [UI_H]
    sub ax, 13
    cmp [mouse_y], ax
    jbe .my
    mov [mouse_y], ax
.my:
    mov byte [cursor_drawn], 0
    mov byte [need_cursor], 1
    mov byte [scene_dirty], 1
    popa
    ret

; ------------------------------------------------------------
; gfx_set_palette - upload 256 6-bit RGB entries
; ------------------------------------------------------------
gfx_set_palette:
    pusha
    push ds
    mov dx, 0x3C8
    xor al, al
    out dx, al
    inc dx
    mov si, palette_data
    mov cx, palette_entries * 3
    cld
    rep outsb
    mov si, palette_tail
    mov cx, (256 - palette_entries) * 3
    rep outsb
    pop ds
    popa
    ret

; ------------------------------------------------------------
; gfx_load_font - copy the BIOS 8x8 font to FONT_SEG:0000
; Must be called before gfx_enter_unreal.
; ------------------------------------------------------------
gfx_load_font:
    pusha
    push ds
    push es

    mov ax, 0x1130
    mov bh, 0x03
    int 0x10
    mov ax, es
    or ax, bp
    jz .fallback

    mov si, bp
    mov ax, es
    mov ds, ax
    jmp .copy

.fallback:
    mov ax, 0xF000
    mov ds, ax
    mov si, 0xFA6E

.copy:
    mov ax, FONT_SEG
    mov es, ax
    xor di, di
    mov cx, 2048
    cld
    rep movsb

    pop es
    pop ds
    popa
    ret

; ------------------------------------------------------------
; fill_rect - AX=x BX=y CX=w DX=h SI=color (logical UI coords)
; ------------------------------------------------------------
fill_rect:
    pusha

    test cx, cx
    jz .done
    test dx, dx
    jz .done
    cmp ax, [UI_W]
    jae .done
    cmp bx, [UI_H]
    jae .done

    push ax
    mov ax, si
    mov [rect_color], al
    pop ax

    mov si, [UI_W]
    sub si, ax
    cmp cx, si
    jbe .width_ok
    mov cx, si
.width_ok:
    mov si, [UI_H]
    sub si, bx
    cmp dx, si
    jbe .height_ok
    mov dx, si
.height_ok:
    mov [rect_w], cx
    shl dx, 1                ; each logical row is two physical rows
    mov [rect_h], dx

    ; physical destination
    movzx eax, ax
    shl eax, 1
    movzx ebx, bx
    shl ebx, 1
    movzx ecx, word [SCREEN_W]
    imul ebx, ecx
    add eax, ebx
    add eax, BUF_BASE
    mov [rect_dest], eax

    movzx eax, word [rect_w]
    shl eax, 1
    mov [rect_w2], eax
    movzx eax, word [SCREEN_W]
    mov [rect_stride], eax

.row:
    mov edi, [rect_dest]
    mov ecx, [rect_w2]
    mov al, [rect_color]
    cld
    a32 rep stosb

    mov eax, [rect_stride]
    add [rect_dest], eax
    dec word [rect_h]
    jnz .row

.done:
    popa
    ret

; ------------------------------------------------------------
; put_pixel - AX=x BX=y SI=color (logical UI coords)
; ------------------------------------------------------------
put_pixel:
    pusha
    cmp ax, [UI_W]
    jae .done
    cmp bx, [UI_H]
    jae .done

    push ax
    mov ax, si
    mov [rect_color], al
    pop ax

    movzx eax, ax
    shl eax, 1
    movzx ebx, bx
    shl ebx, 1
    movzx ecx, word [SCREEN_W]
    imul ebx, ecx
    add eax, ebx
    add eax, BUF_BASE
    mov edi, eax

    movzx ebp, word [SCREEN_W]
    mov al, [rect_color]
    mov [es:edi], al
    mov [es:edi+1], al
    mov [es:edi+ebp], al
    mov [es:edi+ebp+1], al

.done:
    popa
    ret

; ------------------------------------------------------------
; frame_rect - AX=x BX=y CX=w DX=h SI=color (logical)
; ------------------------------------------------------------
frame_rect:
    pusha
    mov [frm_x], ax
    mov [frm_y], bx
    mov [frm_w], cx
    mov [frm_h], dx
    mov [frm_c], si

    mov ax, [frm_x]
    mov bx, [frm_y]
    mov cx, [frm_w]
    mov dx, 1
    mov si, [frm_c]
    call fill_rect

    mov ax, [frm_x]
    mov bx, [frm_y]
    add bx, [frm_h]
    dec bx
    mov cx, [frm_w]
    mov dx, 1
    mov si, [frm_c]
    call fill_rect

    mov ax, [frm_x]
    mov bx, [frm_y]
    inc bx
    mov cx, 1
    mov dx, [frm_h]
    sub dx, 2
    jbe .right
    mov si, [frm_c]
    call fill_rect

.right:
    mov ax, [frm_x]
    add ax, [frm_w]
    dec ax
    mov bx, [frm_y]
    inc bx
    mov cx, 1
    mov dx, [frm_h]
    sub dx, 2
    jbe .done
    mov si, [frm_c]
    call fill_rect

.done:
    popa
    ret

; ------------------------------------------------------------
; draw_char - AL=char BX=x CX=y DL=color (logical, scaled x2)
; ------------------------------------------------------------
draw_char:
    pusha
    push ds

    mov [cs:dc_char], al
    mov [cs:dc_x], bx
    mov [cs:dc_y], cx
    mov [cs:dc_color], dl

    cmp bx, [UI_W]
    jae .done
    cmp cx, [UI_H]
    jae .done

    mov ax, [UI_W]
    sub ax, [cs:dc_x]
    cmp ax, 8
    jbe .cols_ok
    mov ax, 8
.cols_ok:
    mov [cs:dc_cols], ax

    mov ax, [UI_H]
    sub ax, [cs:dc_y]
    cmp ax, 8
    jbe .rows_ok
    mov ax, 8
.rows_ok:
    mov [cs:dc_rows], ax

    movzx eax, word [cs:dc_x]
    shl eax, 1
    movzx ebx, word [cs:dc_y]
    shl ebx, 1
    movzx ecx, word [SCREEN_W]
    imul ebx, ecx
    add eax, ebx
    add eax, BUF_BASE
    mov [cs:dc_dest], eax

    xor ax, ax
    mov al, [cs:dc_char]
    mov si, ax
    shl si, 3
    mov ax, FONT_SEG
    mov ds, ax

    movzx ebp, word [cs:SCREEN_W]
    mov dx, [cs:dc_rows]

.row:
    lodsb
    mov bl, al
    mov edi, [cs:dc_dest]
    mov cx, [cs:dc_cols]
.bit:
    shl bl, 1
    jnc .skip
    mov al, [cs:dc_color]
    mov [es:edi], al
    mov [es:edi+1], al
    mov [es:edi+ebp], al
    mov [es:edi+ebp+1], al
.skip:
    add edi, 2
    dec cx
    jnz .bit

    mov eax, ebp
    shl eax, 1
    add [cs:dc_dest], eax
    dec dx
    jnz .row

.done:
    pop ds
    popa
    ret

; ------------------------------------------------------------
; draw_text - SI=ASCIIZ string BX=x CX=y DL=color (logical)
; ------------------------------------------------------------
draw_text:
    pusha
    mov [dt_x], bx
    mov [dt_y], cx
    mov [dt_color], dl

.next:
    lodsb
    test al, al
    jz .done
    mov bx, [dt_x]
    mov cx, [dt_y]
    mov dl, [dt_color]
    call draw_char
    add word [dt_x], 8
    jmp .next

.done:
    popa
    ret

; ------------------------------------------------------------
; text_width - SI=ASCIIZ string -> AX = logical pixel width
; ------------------------------------------------------------
text_width:
    push si
    push bx
    xor bx, bx
.loop:
    lodsb
    test al, al
    jz .done
    inc bx
    jmp .loop
.done:
    mov ax, bx
    shl ax, 3
    pop bx
    pop si
    ret

; ------------------------------------------------------------
; blit - copy the whole back buffer to the LFB
; The LFB may have pitch > width; the back buffer is packed.
; ------------------------------------------------------------
blit:
    pusha
    mov esi, BUF_BASE
    mov edi, [lfb_base]
    movzx ebp, word [SCREEN_W]
    movzx eax, word [SCREEN_PITCH]
    cmp eax, ebp
    jne .rows

    ; fast path: packed framebuffer
    movzx eax, word [SCREEN_H]
    imul eax, ebp
    mov ecx, eax
    shr ecx, 2
    cld
    fs a32 rep movsd
    mov ecx, eax
    and ecx, 3
    fs a32 rep movsb
    popa
    ret

.rows:
    sub eax, ebp
    mov [blit_pad], eax
    mov dx, [SCREEN_H]
.row:
    mov ecx, ebp
    mov ebx, ecx
    shr ecx, 2
    cld
    fs a32 rep movsd
    mov ecx, ebx
    and ecx, 3
    fs a32 rep movsb
    add edi, [blit_pad]
    dec dx
    jnz .row
    popa
    ret

; ------------------------------------------------------------
; copy_buffer_rect - AX=x BX=y CX=w DX=h (physical)
; Copy from the back buffer to the LFB (cursor erase).
; ------------------------------------------------------------
copy_buffer_rect:
    pusha

    test cx, cx
    jz .done
    test dx, dx
    jz .done
    cmp ax, [SCREEN_W]
    jae .done
    cmp bx, [SCREEN_H]
    jae .done

    mov [cpy_x], ax
    mov [cpy_y], bx

    mov si, [SCREEN_W]
    sub si, ax
    cmp cx, si
    jbe .width_ok
    mov cx, si
.width_ok:
    mov si, [SCREEN_H]
    sub si, bx
    cmp dx, si
    jbe .height_ok
    mov dx, si
.height_ok:
    mov [cpy_w], cx
    mov [cpy_h], dx

    ; packed back-buffer source
    movzx eax, word [cpy_x]
    movzx ebx, word [cpy_y]
    movzx ecx, word [SCREEN_W]
    imul ebx, ecx
    add eax, ebx
    add eax, BUF_BASE
    mov [cpy_src_lin], eax

    ; LFB destination uses its own stride
    movzx eax, word [cpy_x]
    movzx ebx, word [cpy_y]
    movzx ecx, word [SCREEN_PITCH]
    imul ebx, ecx
    add eax, ebx
    add eax, [lfb_base]
    mov [cpy_dst_lin], eax

.row:
    mov esi, [cpy_src_lin]
    mov edi, [cpy_dst_lin]
    movzx ecx, word [cpy_w]
    mov ebx, ecx
    shr ecx, 2
    jz .tail
    cld
.dword:
    mov eax, [fs:esi]
    mov [es:edi], eax
    add esi, 4
    add edi, 4
    dec ecx
    jnz .dword
.tail:
    and ebx, 3
    jz .next
.byte:
    mov al, [fs:esi]
    mov [es:edi], al
    inc esi
    inc edi
    dec ebx
    jnz .byte
.next:
    movzx eax, word [SCREEN_W]
    add [cpy_src_lin], eax
    movzx eax, word [SCREEN_PITCH]
    add [cpy_dst_lin], eax
    dec word [cpy_h]
    jnz .row

.done:
    popa
    ret

; ------------------------------------------------------------
; draw_cursor - draw the pointer on the LFB (physical)
; ------------------------------------------------------------
draw_cursor:
    pusha
    mov ax, [cursor_new_x]
    shl ax, 1
    mov bx, [cursor_new_y]
    shl bx, 1
    add ax, 2
    add bx, 2
    mov byte [cursor_draw_color], 0
    call cursor_blit

    mov ax, [cursor_new_x]
    shl ax, 1
    mov bx, [cursor_new_y]
    shl bx, 1
    mov byte [cursor_draw_color], 1
    call cursor_blit
    popa
    ret

cursor_blit:
    pusha
    movzx eax, ax
    movzx ebx, bx
    movzx ecx, word [SCREEN_PITCH]
    imul ebx, ecx
    add eax, ebx
    add eax, [lfb_base]
    mov edi, eax

    movzx ebp, word [SCREEN_PITCH]
    mov si, cursor_bitmap
    mov dx, 12
.row:
    lodsb
    mov bl, al
    mov cx, 8
.bit:
    shl bl, 1
    jnc .skip
    mov al, [cursor_draw_color]
    mov [es:edi], al
    mov [es:edi+1], al
    mov [es:edi+ebp], al
    mov [es:edi+ebp+1], al
.skip:
    add edi, 2
    dec cx
    jnz .bit
    add edi, ebp
    add edi, ebp
    sub edi, 16
    dec dx
    jnz .row

    popa
    ret

; ------------------------------------------------------------
; gfx_update_mode_text - build "WxH" in mode_text
; ------------------------------------------------------------
gfx_update_mode_text:
    pusha
    mov di, mode_text
    mov ax, [SCREEN_W]
    call gfx_u16_dec
    mov al, 'x'
    mov [di], al
    inc di
    mov ax, [SCREEN_H]
    call gfx_u16_dec
    mov byte [di], 0
    popa
    ret

; AX=value, DI=destination, returns DI advanced
gfx_u16_dec:
    push ax
    push bx
    push cx
    push dx
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
    mov [di], al
    inc di
    loop .output
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ------------------------------------------------------------
; GDT
; ------------------------------------------------------------
gdt_start:
    dq 0
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92
    db 0xCF
    db 0x00
gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x02            ; base 0x00020000
    db 0x9A
    db 0x00            ; 16-bit code, limit 64K
    db 0x00
gdt_end:
gdt_desc:
    dw gdt_end - gdt_start - 1
    dd 0x00020000 + gdt_start

; ------------------------------------------------------------
; graphics data
; ------------------------------------------------------------
SCREEN_W        dw 0
SCREEN_H        dw 0
SCREEN_PITCH    dw 0
UI_W            dw 0
UI_H            dw 0
gfx_ready       db 0
vga_found       db 0
new_w           dw 0
new_h           dw 0
lfb_base        dd 0xE0000000
mouse_speed     dw 1
mode_text       times 12 db 0

gfx_backend       db 0
gfx_if_saved      db 0
gfx_probe_error   dw 0
gfx_mode_count    db 0
bochs_ok          db 0
vbe_ok            db 0
any_vga_found     db 0
vbe_major         db 0
vbe_mem_kb        dd 0
vbe_cur_mode      dw 0
vbe_result        dw 0
vbe_index         dw 0
vbe_tmp_w         dw 0
vbe_tmp_h         dw 0
vbe_tmp_pitch     dw 0
vbe_tmp_lfb       dd 0
vbe_tmp_bytes     dd 0
vbe_mode_seg      dw 0
vbe_mode_off      dw 0
synth_w           dw 0
synth_h           dw 0
synth_vram_bytes  dd 0
synth_buf_bytes   dd 0
sort_i            dw 0
sort_area         dd 0
find_req_w        dw 0
find_req_h        dw 0
find_req_area     dd 0
find_best_diff    dd 0
find_best         dw 0
find_result       dw 0
blit_pad          dd 0
cpy_x             dw 0
cpy_y             dw 0

rect_w          dw 0
rect_h          dw 0
rect_color      db 0
rect_dest       dd 0
rect_w2         dd 0
rect_stride     dd 0
frm_x           dw 0
frm_y           dw 0
frm_w           dw 0
frm_h           dw 0
frm_c           dw 0
dc_char         db 0
dc_x            dw 0
dc_y            dw 0
dc_color        db 0
dc_cols         dw 0
dc_rows         dw 0
dc_dest         dd 0
dt_x            dw 0
dt_y            dw 0
dt_color        db 0
cpy_w           dw 0
cpy_h           dw 0
cpy_src         dd 0
cpy_src_lin     dd 0
cpy_dst_lin     dd 0
cursor_draw_color db 0

cursor_bitmap:
    db 10000000b
    db 11000000b
    db 11100000b
    db 11110000b
    db 11111000b
    db 11111100b
    db 11111110b
    db 11111111b
    db 11111000b
    db 11011100b
    db 10001110b
    db 00000111b

; ------------------------------------------------------------
; mode table and BIOS scratch buffers
; ------------------------------------------------------------
align 2
gfx_modes:
    times GFX_MAX_MODES * GM_SIZE db 0

align 2
vbe_info:
    times 512 db 0
vbe_mode_info:
    times 256 db 0

bochs_mode_tab:
    dw 640, 360
    dw 640, 480
    dw 800, 600
    dw 1024, 768
    dw 1280, 720
    dw 1280, 800
    dw 1360, 768
    dw 1440, 900
    dw 1600, 900
    dw 1600, 1200
    dw 1680, 1050
    dw 1920, 1080
    dw 1920, 1200
    dw 2560, 1440
    dw 2560, 1600
    dw 0, 0

; 64 base colours + 36 desktop gradient shades (64..99)
palette_entries equ 100
palette_data:
    db 0,0,0            ; 0 black
    db 63,63,63         ; 1 white
    db 42,42,42         ; 2 light grey
    db 21,21,21         ; 3 dark grey
    db 6,6,10           ; 4 near black
    db 0,8,26           ; 5 deep navy
    db 0,20,50          ; 6 blue
    db 0,63,63          ; 7 cyan
    db 0,34,38          ; 8 dark cyan
    db 0,63,0           ; 9 green
    db 0,32,0           ; 10 dark green
    db 63,63,0          ; 11 yellow
    db 63,34,0          ; 12 orange
    db 63,0,0           ; 13 red
    db 63,0,40          ; 14 magenta
    db 30,0,50          ; 15 purple
    db 12,12,14         ; 16 panel dark
    db 28,28,32         ; 17 panel light
    db 48,48,52         ; 18 panel highlight
    db 63,24,24         ; 19 soft red
    db 20,48,63         ; 20 sky
    db 32,0,48          ; 21 violet
    db 0,48,32          ; 22 mint
    db 48,32,0          ; 23 amber

    ; 24..63 reserved / useful UI colours
    db 8,12,20, 10,16,28, 12,20,34, 14,24,40
    db 16,28,48, 18,32,54, 20,36,58, 22,40,60
    db 24,44,63, 6,10,16, 10,14,22, 14,18,28
    db 18,22,32, 22,26,38, 26,30,44, 30,34,50
    db 34,38,56, 38,42,60, 42,46,63, 46,50,63
    db 50,54,63, 54,58,63, 58,60,63, 60,62,63
    db 63,20,20, 63,32,0, 63,44,0, 63,56,0
    db 32,63,0, 16,63,0, 0,63,16, 0,63,32
    db 0,63,48, 0,63,63, 0,48,63, 0,32,63
    db 0,40,63, 0,24,48, 16,0,32, 32,0,16

    ; 64..99 desktop gradient (36 shades, dark blue -> teal)
    db 1,3,9
    db 1,4,10
    db 1,4,11
    db 1,5,12
    db 1,5,13
    db 1,6,14
    db 1,6,15
    db 2,7,16
    db 2,7,17
    db 2,8,18
    db 2,8,19
    db 2,9,20
    db 2,9,21
    db 3,10,22
    db 3,10,23
    db 3,11,24
    db 3,11,25
    db 3,12,26
    db 4,12,27
    db 4,13,28
    db 4,13,29
    db 4,14,30
    db 4,14,31
    db 5,15,32
    db 5,15,33
    db 5,16,34
    db 5,16,35
    db 5,17,36
    db 6,17,37
    db 6,18,38
    db 6,18,39
    db 6,19,40
    db 7,19,41
    db 7,20,42
    db 7,20,43
    db 8,22,46

palette_tail times (256 - palette_entries) * 3 db 0
