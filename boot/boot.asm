; ============================================================
;  LiMaxOS stage 1 boot sector
;  Reads LMAXLDR.SYS from floppy and jumps to it.
;  Assembled as a raw 512-byte sector with NASM.
; ============================================================
[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    cld
    mov [boot_drive], dl

    ; 80x25 text mode, clear screen
    mov ax, 0x0003
    int 0x10

    ; Paint the screen black (white text)
    mov ax, 0xB800
    mov es, ax
    xor di, di
    mov ax, 0x0F20
    mov cx, 2000
    rep stosw

    ; Title
    mov si, msg_title
    mov di, 7 * 160 + 26 * 2
    mov ah, 0x0E
    call print_at

    ; Status
    mov si, msg_loading
    mov di, 10 * 160 + 22 * 2
    mov ah, 0x0F
    call print_at

    ; Read first loader sector to 1000:0000
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    mov ax, 1
    call read_one
    jc disk_error

    ; Validate loader header
    cmp dword [es:0], 'LiMa'
    jne bad_image
    cmp dword [es:4], 'xLdr'
    jne bad_image
    mov cx, [es:8]
    cmp cx, 1
    jb bad_image
    cmp cx, 63
    ja bad_image

    ; Read remaining sectors
    mov word [lba], 2
    mov word [dest], 0x0200
    dec cx
    jz loaded
.read_loop:
    mov [remaining], cx
    mov ax, [lba]
    mov bx, [dest]
    mov dx, 0x1000
    mov es, dx
    call read_one
    jc disk_error
    inc word [lba]
    add word [dest], 512
    mov cx, [remaining]
    loop .read_loop

loaded:
    mov ax, 0xB800
    mov es, ax
    mov si, msg_ok
    mov di, 10 * 160 + 51 * 2
    mov ah, 0x0A
    call print_at

    mov si, msg_go
    mov di, 12 * 160 + 22 * 2
    mov ah, 0x07
    call print_at

    mov dl, [boot_drive]
    push word 0x1000
    push word 0x0020
    retf

; ------------------------------------------------------------
; print_at: SI = string, DI = video offset, AH = attribute
; ES must be B800
; ------------------------------------------------------------
print_at:
    lodsb
    test al, al
    jz .done
    stosw
    jmp print_at
.done:
    ret

; ------------------------------------------------------------
; read_one: AX = LBA, ES:BX = destination, DL = drive
; Returns CF=1 on error, retries 3 times.
; ------------------------------------------------------------
read_one:
    mov [io_lba], ax
    mov [io_dest], bx
    mov byte [io_try], 1
.try:
    mov ax, [io_lba]
    xor dx, dx
    mov bx, 18
    div bx                  ; AX = LBA/18, DX = LBA%18
    inc dl
    mov cl, dl              ; sector (1..18)
    xor dx, dx
    mov bx, 2
    div bx                  ; AX = cylinder, DX = head
    mov ch, al
    mov dh, dl
    mov dl, [boot_drive]
    mov bx, [io_dest]
    mov ax, 0x0201
    int 0x13
    jnc .ok

    ; Reset the disk and retry
    xor ax, ax
    int 0x13
    inc byte [io_try]
    cmp byte [io_try], 4
    jb .try
    stc
    ret
.ok:
    clc
    ret

disk_error:
    mov ax, 0xB800
    mov es, ax
    mov si, msg_disk
    mov di, 14 * 160 + 22 * 2
    mov ah, 0x0C
    call print_at
    jmp halt

bad_image:
    mov ax, 0xB800
    mov es, ax
    mov si, msg_bad
    mov di, 14 * 160 + 16 * 2
    mov ah, 0x0C
    call print_at
    jmp halt

halt:
    cli
.hang:
    hlt
    jmp .hang

; ------------------------------------------------------------
; Data
; ------------------------------------------------------------
boot_drive  db 0
io_lba      dw 0
io_dest     dw 0
io_try      db 0
lba         dw 0
dest        dw 0
remaining   dw 0

msg_title   db "LiMaxOS Bootloader v1.0", 0
msg_loading db "Loading LMAXLDR.SYS ...", 0
msg_ok      db "OK", 0
msg_go      db "Starting boot manager...", 0
msg_disk    db "Disk read error! System halted.", 0
msg_bad     db "Invalid LMAXLDR.SYS image! System halted.", 0

    times 510 - ($ - $$) db 0
    dw 0xAA55
