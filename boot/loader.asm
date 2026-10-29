; ============================================================
;  LiMaxOS stage 2 boot manager (LMAXLDR.SYS)
;  Minimal black boot screen: big neon L + real disk progress bar,
;  then jumps to CORE.SYS.
; ============================================================
[BITS 16]
[ORG 0x0000]

; ---- 32-byte image header (patched by mkimg.exe) ----
hdr_magic:      db "LiMaxLdr"
hdr_sectors:    dw 0
hdr_load_seg:   dw 0x1000
hdr_entry:      dw 0x0020
hdr_size:       dw 0
hdr_checksum:   dw 0
hdr_version:    dw 0x0100
                times 32 - ($ - $$) db 0

KERNEL_LBA      equ 64
KERNEL_SEG      equ 0x2000
KERNEL_ENTRY    equ 0x0020

start:
    cli
    xor ax, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    cld

    mov ax, cs
    mov ds, ax
    mov [boot_drive], dl

    mov ax, 0x0003
    int 0x10

    call draw_ui
    mov byte [ui_ready], 1

    ; ---------------- stage 0: hardware ----------------
    mov al, 0
    mov dl, 1
    call set_stage
    call detect_hw
    call show_hw_detail
    mov ax, 200
    call delay_ms
    mov al, 0
    mov dl, 2
    call set_stage
    mov al, 4
    call set_progress

    ; ---------------- stage 1: loader ----------------
    mov al, 1
    mov dl, 1
    call set_stage
    mov si, msg_loader_ok
    call show_detail
    mov ax, 180
    call delay_ms
    mov al, 1
    mov dl, 2
    call set_stage
    mov al, 8
    call set_progress

    ; ---------------- stage 2: kernel ----------------
    mov al, 2
    mov dl, 1
    call set_stage
    mov si, msg_read_kernel
    call show_detail
    mov al, 10
    call set_progress
    call load_kernel
    mov al, 2
    mov dl, 2
    call set_stage
    mov al, 82
    call set_progress

    ; ---------------- stage 3: verify ----------------
    mov al, 3
    mov dl, 1
    call set_stage
    mov si, msg_verify
    call show_detail
    mov ax, 180
    call delay_ms
    call verify_kernel
    mov al, 3
    mov dl, 2
    call set_stage
    mov al, 86
    call set_progress

    ; ---------------- stage 4: memory ----------------
    mov al, 4
    mov dl, 1
    call set_stage
    mov si, msg_mem_start
    call show_detail
    mov ax, 150
    call delay_ms
    call memory_test
    mov al, 4
    mov dl, 2
    call set_stage
    mov al, 96
    call set_progress

    ; ---------------- stage 5: start ----------------
    mov al, 5
    mov dl, 1
    call set_stage
    mov si, msg_shell
    call show_detail
    mov ax, 350
    call delay_ms
    mov al, 5
    mov dl, 2
    call set_stage
    mov al, 100
    call set_progress
    mov si, msg_booting
    call show_detail
    mov ax, 650
    call delay_ms

    mov dl, [boot_drive]
    push word KERNEL_SEG
    push word KERNEL_ENTRY
    retf

; ============================================================
;  draw_ui
; ============================================================
draw_ui:
    ; hide the hardware text cursor
    mov ah, 0x01
    mov ch, 0x20
    mov cl, 0x00
    int 0x10

    mov ax, 0xB800
    mov es, ax
    xor di, di
    mov ax, 0x0F20
    mov cx, 2000
    rep stosw

    ; neon shadow of the L (offset one cell right and down)
    mov byte [loop_row], 5
.shadow_stem:
    mov al, [loop_row]
    mov ah, 32
    call goto_xy
    mov al, 0x03
    call set_attr
    mov al, 0xDB
    mov cx, 8
    call putc_n
    inc byte [loop_row]
    cmp byte [loop_row], 18
    jb .shadow_stem

    mov byte [loop_row], 16
.shadow_foot:
    mov al, [loop_row]
    mov ah, 32
    call goto_xy
    mov al, 0x03
    call set_attr
    mov al, 0xDB
    mov cx, 19
    call putc_n
    inc byte [loop_row]
    cmp byte [loop_row], 18
    jb .shadow_foot

    ; big neon L
    mov byte [loop_row], 4
.stem:
    mov al, [loop_row]
    mov ah, 31
    call goto_xy
    mov al, 0x0B
    call set_attr
    mov al, 0xDB
    mov cx, 8
    call putc_n
    inc byte [loop_row]
    cmp byte [loop_row], 17
    jb .stem

    mov byte [loop_row], 15
.foot:
    mov al, [loop_row]
    mov ah, 31
    call goto_xy
    mov al, 0x0B
    call set_attr
    mov al, 0xDB
    mov cx, 19
    call putc_n
    inc byte [loop_row]
    cmp byte [loop_row], 17
    jb .foot

    xor al, al
    call set_progress
    ret

; ============================================================
;  text output
; ============================================================
; goto_xy: AL = row, AH = column
goto_xy:
    mov [cur_row], al
    mov [cur_col], ah
    ret

; set_attr: AL = attribute
set_attr:
    mov [cur_attr], al
    ret

; putchar: AL = character, uses cur_row/cur_col/cur_attr
putchar:
    push es
    push ax
    push bx
    push cx
    push di

    mov bl, al
    mov bh, [cur_attr]

    cmp bl, 13
    je .cr
    cmp bl, 10
    je .lf

    xor ax, ax
    mov al, [cur_row]
    mov cl, 80
    mul cl
    xor cx, cx
    mov cl, [cur_col]
    add ax, cx
    shl ax, 1
    mov di, ax

    mov ax, 0xB800
    mov es, ax
    mov [es:di], bx

    inc byte [cur_col]
    cmp byte [cur_col], 80
    jb .done
    mov byte [cur_col], 0
    inc byte [cur_row]
    jmp .done
.cr:
    mov byte [cur_col], 0
    jmp .done
.lf:
    mov byte [cur_col], 0
    inc byte [cur_row]
.done:
    pop di
    pop cx
    pop bx
    pop ax
    pop es
    ret

; puts: SI = ASCIIZ string
puts:
    push ax
    push si
.loop:
    lodsb
    test al, al
    jz .done
    call putchar
    jmp .loop
.done:
    pop si
    pop ax
    ret

; putc_n: AL = char, CX = count
putc_n:
    push ax
    push cx
.loop:
    test cx, cx
    jz .done
    call putchar
    dec cx
    jmp .loop
.done:
    pop cx
    pop ax
    ret

; print_spaces: CX = count, uses current attribute
print_spaces:
    mov al, ' '
    jmp putc_n

; print_dec16: AX = unsigned decimal
print_dec16:
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
    call putchar
    loop .output
    popa
    ret

; print_hex8: AL = byte, prints two hex digits
print_hex8:
    pusha
    mov bl, al
    shr al, 4
    call .nibble
    mov al, bl
    call .nibble
    popa
    ret
.nibble:
    and al, 0x0F
    mov si, hex_digits
    xor ah, ah
    add si, ax
    lodsb
    call putchar
    ret

; draw_logo_line: SI = line, '#' is converted to CP437 block
draw_logo_line:
    push ax
    push si
.loop:
    lodsb
    test al, al
    jz .done
    cmp al, '.'
    jne .check_hash
    mov al, ' '
    jmp .emit
.check_hash:
    cmp al, '#'
    jne .emit
    mov al, 0xDB
.emit:
    call putchar
    jmp .loop
.done:
    pop si
    pop ax
    ret

; set_progress: AL = percent (0..100)
set_progress:
    push ax
    push bx
    push cx
    push dx
    mov bl, al
    xor bh, bh
    mov ax, bx
    mov cx, 52
    mul cx
    mov cx, 100
    div cx
    mov [bar_fill], ax

    ; opening bracket
    mov al, 19
    mov ah, 14
    call goto_xy
    mov al, 0x0B
    call set_attr
    mov al, '['
    call putchar

    ; filled part
    mov al, 19
    mov ah, 15
    call goto_xy
    mov al, 0x0A
    call set_attr
    mov ax, [bar_fill]
    mov cx, ax
    mov al, 0xDB
    call putc_n

    ; empty part
    mov ax, 52
    sub ax, [bar_fill]
    mov cx, ax
    mov al, 0x08
    call set_attr
    mov al, 0xB0
    call putc_n

    ; closing bracket
    mov al, 19
    mov ah, 67
    call goto_xy
    mov al, 0x0B
    call set_attr
    mov al, ']'
    call putchar

    ; percentage
    mov al, 19
    mov ah, 69
    call goto_xy
    mov al, 0x0F
    call set_attr
    mov al, bl
    call print_percent3
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; print_percent3: AL = percent, prints "  0%".."100%"
print_percent3:
    pusha
    xor ah, ah
    mov cl, 100
    div cl
    mov bl, al              ; hundreds
    mov al, ah
    xor ah, ah
    mov cl, 10
    div cl
    mov bh, al              ; tens
    mov ch, ah              ; ones

    mov al, bl
    test al, al
    jz .h_space
    add al, '0'
    jmp .h_out
.h_space:
    mov al, ' '
.h_out:
    call putchar

    mov al, bh
    test al, al
    jnz .t_digit
    test bl, bl
    jz .t_space
.t_digit:
    add al, '0'
    jmp .t_out
.t_space:
    mov al, ' '
.t_out:
    call putchar

    mov al, ch
    add al, '0'
    call putchar
    mov al, '%'
    call putchar
    popa
    ret

; set_stage: the minimal boot screen has no stage list
set_stage:
    ret

; show_detail: progress is shown only by the bar
show_detail:
    ret

; panic: AX = error code, shows the green screen of death
panic:
    jmp gsod

; ============================================================
;  hardware detection
; ============================================================
detect_hw:
    int 0x12
    mov [ram_kb], ax
    mov ah, 0x08
    mov dl, [boot_drive]
    int 0x13
    ret

show_hw_detail:
    ret

; ============================================================
;  kernel loading
; ============================================================
load_kernel:
    ; read the first sector and parse the header
    mov ax, KERNEL_SEG
    mov es, ax
    xor bx, bx
    mov ax, KERNEL_LBA
    call read_sector
    jc .disk_fail

    cmp dword [es:0], 'LiMa'
    jne .bad_image
    cmp dword [es:4], 'xOSK'
    jne .bad_image
    cmp word [es:10], KERNEL_SEG
    jne .bad_image
    cmp word [es:12], KERNEL_ENTRY
    jne .bad_image

    mov ax, [es:8]
    mov [kern_sectors], ax
    mov ax, [es:14]
    mov [kern_size], ax
    mov ax, [es:16]
    mov [kern_checksum], ax

    mov ax, [kern_sectors]
    cmp ax, 1
    jb .bad_image
    cmp ax, 127
    ja .too_large
    mov ax, [kern_size]
    cmp ax, KERNEL_ENTRY
    jbe .bad_image

    mov word [loaded], 1
    call update_load_progress
    mov ax, 25
    call delay_ms

    mov ax, [kern_sectors]
    dec ax
    mov [remain], ax
    jz .all_loaded

    mov word [io_lba], KERNEL_LBA + 1
    mov word [io_dest], 0x0200
.load_loop:
    mov ax, [io_lba]
    mov bx, [io_dest]
    mov dx, KERNEL_SEG
    mov es, dx
    call read_sector
    jc .disk_fail

    inc word [loaded]
    inc word [io_lba]
    add word [io_dest], 512
    call update_load_progress
    mov ax, 25
    call delay_ms

    dec word [remain]
    jnz .load_loop

.all_loaded:
    mov ax, KERNEL_SEG
    mov es, ax
    ret

.disk_fail:
    mov al, 2
    mov dl, 3
    call set_stage
    mov ax, ERR_KERNEL_READ
    jmp panic
.bad_image:
    mov al, 2
    mov dl, 3
    call set_stage
    mov ax, ERR_KERNEL_HEADER
    jmp panic
.too_large:
    mov al, 2
    mov dl, 3
    call set_stage
    mov ax, ERR_KERNEL_LARGE
    jmp panic

update_load_progress:
    mov ax, [loaded]
    mov cx, 70
    mul cx
    mov cx, [kern_sectors]
    div cx
    add ax, 10
    cmp ax, 80
    jbe .ok
    mov ax, 80
.ok:
    call set_progress
    call show_read_detail
    ret

show_read_detail:
    ret

show_retry:
    call clear_detail
    mov al, 0x0E
    call set_attr
    mov si, msg_retry
    call puts
    mov ax, [io_lba]
    call print_dec16
    mov si, msg_retry2
    call puts
    mov al, [io_try]
    xor ah, ah
    call print_dec16
    mov si, msg_retry3
    call puts
    ret

; ============================================================
;  kernel checksum
; ============================================================
verify_kernel:
    mov ax, KERNEL_SEG
    mov es, ax
    mov si, KERNEL_ENTRY
    mov cx, [kern_size]
    sub cx, KERNEL_ENTRY
    jbe .bad
    xor dx, dx
.sum:
    mov al, [es:si]
    xor ah, ah
    add dx, ax
    inc si
    loop .sum
    cmp dx, [kern_checksum]
    jne .bad
    ret
.bad:
    mov al, 3
    mov dl, 3
    call set_stage
    mov ax, ERR_CHECKSUM
    jmp panic

; ============================================================
;  memory test: 0x40000..0x8FFFF (320 KB)
; ============================================================
memory_test:
    mov word [mem_seg], 0x4000
    mov word [mem_left], 5
    mov byte [mem_step], 0
.block:
    ; write AA55
    mov ax, [mem_seg]
    mov es, ax
    xor di, di
    mov cx, 0x8000
    mov ax, 0xAAAA
    rep stosw

    ; verify AA55
    mov ax, [mem_seg]
    mov es, ax
    xor di, di
    mov cx, 0x8000
    mov ax, 0xAAAA
.check1:
    cmp word [es:di], ax
    jne .fail
    add di, 2
    loop .check1

    ; write 5555
    mov ax, [mem_seg]
    mov es, ax
    xor di, di
    mov cx, 0x8000
    mov ax, 0x5555
    rep stosw

    ; verify 5555
    mov ax, [mem_seg]
    mov es, ax
    xor di, di
    mov cx, 0x8000
    mov ax, 0x5555
.check2:
    cmp word [es:di], ax
    jne .fail
    add di, 2
    loop .check2

    mov al, [mem_step]
    xor ah, ah
    mov bx, ax
    mov al, [mem_progress + bx]
    call set_progress
    call show_mem_detail
    mov ax, 40
    call delay_ms

    inc byte [mem_step]
    add word [mem_seg], 0x1000
    dec word [mem_left]
    jnz .block
    ret
.fail:
    mov al, 4
    mov dl, 3
    call set_stage
    mov ax, ERR_MEMORY_TEST
    jmp panic

show_mem_detail:
    ret

; ============================================================
;  disk access (CHS, 1.44 MB floppy geometry)
; ============================================================
read_sector:
    push es
    mov [io_seg], es
    mov [io_lba], ax
    mov [io_dest], bx
    mov byte [io_try], 1
.try:
    mov ax, [io_lba]
    xor dx, dx
    mov bx, 18
    div bx
    inc dl
    mov cl, dl
    xor dx, dx
    mov bx, 2
    div bx
    mov ch, al
    mov dh, dl
    mov dl, [boot_drive]

    mov ax, [io_seg]
    mov es, ax
    mov bx, [io_dest]
    mov ax, 0x0201
    int 0x13
    jnc .ok

    pusha
    cmp byte [ui_ready], 0
    je .quiet
    call show_retry
.quiet:
    popa
    xor ax, ax
    int 0x13
    inc byte [io_try]
    cmp byte [io_try], 4
    jb .try
    pop es
    stc
    ret
.ok:
    pop es
    clc
    ret

; ============================================================
;  helpers
; ============================================================
clear_detail:
    mov al, 20
    mov ah, 4
    call goto_xy
    mov al, 0x07
    call set_attr
    mov cx, 72
    call print_spaces
    mov al, 20
    mov ah, 4
    jmp goto_xy

print_hex16:
    push ax
    mov al, ah
    call print_hex8
    pop ax
    call print_hex8
    ret

; delay_ms: AX = milliseconds, BIOS wait
delay_ms:
    pusha
    mov cx, 1000
    mul cx
    mov bx, ax
    mov cx, dx
    mov dx, bx
    mov ax, 0x8600
    int 0x15
    popa
    ret

; ============================================================
;  data
; ============================================================
boot_drive      db 0
ui_ready        db 0
cur_row         db 0
cur_col         db 0
cur_attr        db 0x0F
loop_row        db 0
stage_idx       db 0
kern_sectors    dw 0
kern_size       dw 0
kern_checksum   dw 0
loaded          dw 0
remain          dw 0
io_seg          dw 0
io_lba          dw 0
io_dest         dw 0
io_try          db 0
bar_fill        dw 0
ram_kb          dw 0
mem_seg         dw 0
mem_left        dw 0
mem_step        db 0
mem_progress    db 89, 91, 93, 94, 95

hex_digits      db "0123456789ABCDEF", 0

; ============================================================
;  strings
; ============================================================
msg_title       db " LiMaxOS 1.0  Boot Manager ", 0
msg_subtitle    db "A tiny real-mode operating system", 0
msg_build       db "2-stage NASM boot  |  x86 real mode  |  1.44 MB floppy", 0
msg_progress    db "Boot progress", 0
msg_init        db "Initializing boot manager...", 0
msg_footer      db "LiMax Systems (c) 2026  |  real disk I/O  |  QEMU", 0
msg_loader_ok   db "LMAXLDR.SYS loaded by boot sector. Starting checks...", 0
msg_read_kernel db "Reading CORE.SYS from the floppy image...", 0
msg_verify      db "Verifying CORE.SYS checksum...", 0
msg_mem_start   db "Testing conventional memory (320 KB)...", 0
msg_shell       db "Preparing the LiMaxOS shell...", 0
msg_booting     db "Boot complete. Starting LiMaxOS...", 0
msg_halted      db "System halted. Reset the machine or close QEMU.", 0
msg_disk_fail   db "Floppy read failed! CORE.SYS could not be loaded.", 0
msg_bad_kernel  db "Invalid CORE.SYS image! Boot halted.", 0
msg_checksum_fail db "CORE.SYS checksum mismatch! Boot halted.", 0
msg_mem_fail    db "Memory test failed! Boot halted.", 0
msg_ram         db "RAM: ", 0
msg_kb          db " KB  |  Boot drive: 0x", 0
msg_video       db "  |  Video: VGA 80x25 colour", 0
msg_reading     db "Reading CORE.SYS  sector ", 0
msg_lba         db "   LBA ", 0
msg_real_io     db "   (real disk I/O)", 0
msg_retry       db "Disk read error at LBA ", 0
msg_retry2      db " - retry ", 0
msg_retry3      db "/3, waiting for drive...", 0
msg_mem_block   db "Testing RAM block ", 0
msg_mem_at      db "  at 0x", 0
msg_mem_ok      db "  OK (AA55/55AA)", 0

lbl0            db "Detect hardware", 0
lbl1            db "Load LMAXLDR.SYS", 0
lbl2            db "Load CORE.SYS", 0
lbl3            db "Verify kernel", 0
lbl4            db "Test memory", 0
lbl5            db "Start shell", 0

st_pending      db "[    ]", 0
st_active       db "[ .. ]", 0
st_done         db "[ OK ]", 0
st_fail         db "[FAIL]", 0

stage_attr      db 0x07, 0x0E, 0x0A, 0x0C
stage_status    dw st_pending, st_active, st_done, st_fail
stage_labels    dw lbl0, lbl1, lbl2, lbl3, lbl4, lbl5

logo1           db "#.... .###. #...# .###. #...# .###. .####", 0
logo2           db "#.... ..#.. ##.## #...# .#.#. #...# #....", 0
logo3           db "#.... ..#.. #.#.# ##### ..#.. #...# .###.", 0
logo4           db "#.... ..#.. #...# #...# .#.#. #...# ....#", 0
logo5           db "##### .###. #...# .###. #...# .###. ####.", 0

; ============================================================
;  shared error catalog + green screen of death
; ============================================================
%include "include/errors.inc"
%include "boot/gsod.inc"

