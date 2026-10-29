; ============================================================
;  errors.inc - shared LiMaxOS error codes and GSOD catalog
;
;  Boot sector includes this file with GSOD_CODES_ONLY defined:
;  it only needs the numeric constants and stays inside 512 bytes.
;  The loader and the kernel also include the human-readable
;  catalog, the lookup routine and the common GSOD strings.
; ============================================================

%ifndef LIMAX_ERRORS_INC
%define LIMAX_ERRORS_INC

; ---- boot / disk ------------------------------------------------
ERR_BOOT_DISK           equ 100
ERR_LOADER_IMAGE        equ 101
ERR_LOADER_READ         equ 102
ERR_LOADER_HEADER       equ 103
ERR_LOADER_LARGE        equ 104
ERR_LOADER_CHECKSUM     equ 105
ERR_KERNEL_READ         equ 106
ERR_KERNEL_HEADER       equ 107
ERR_KERNEL_LARGE        equ 108
ERR_KERNEL_LOAD         equ 109

; ---- memory / CPU -----------------------------------------------
ERR_MEMORY_TEST         equ 200
ERR_LOW_MEMORY          equ 201
ERR_EXTENDED_MEMORY     equ 202
ERR_DIVIDE_BY_ZERO      equ 203
ERR_INVALID_OPCODE      equ 204
ERR_DOUBLE_FAULT        equ 205
ERR_GENERAL_PROTECTION  equ 206
ERR_STACK_FAULT         equ 207
ERR_OVERFLOW            equ 208
ERR_BOUNDS              equ 209

; ---- video ------------------------------------------------------
ERR_NO_VBE              equ 300
ERR_VBE_MODE            equ 301
ERR_LFB                 equ 302
ERR_FONT                equ 303
ERR_VIDEO_MODE          equ 304
ERR_PALETTE             equ 305
ERR_VGA_DEVICE          equ 306
ERR_FRAMEBUFFER         equ 307
ERR_RESOLUTION          equ 308
ERR_MODE_SWITCH         equ 309

; ---- input / peripherals ---------------------------------------
ERR_KEYBOARD_INIT       equ 400
ERR_KEYBOARD_CONTROLLER equ 401
ERR_KEYBOARD_OVERFLOW   equ 402
ERR_MOUSE_INIT          equ 403
ERR_MOUSE_CONTROLLER    equ 404
ERR_MOUSE_PACKET        equ 405
ERR_MOUSE_RANGE         equ 406
ERR_SERIAL              equ 407
ERR_SERIAL_TIMEOUT      equ 408
ERR_RTC                 equ 409

; ---- kernel / runtime ------------------------------------------
ERR_KERNEL_SEGMENT      equ 500
ERR_ENTRY_POINT         equ 501
ERR_KERNEL_SMALL        equ 502
ERR_UNEXPECTED_IRQ      equ 503
ERR_IVT                 equ 504
ERR_STACK_OVERFLOW      equ 505
ERR_HEAP                equ 506
ERR_BUFFER_OVERFLOW     equ 507
ERR_POINTER             equ 508
ERR_FUNCTION            equ 509
ERR_ASSERTION           equ 510
ERR_WATCHDOG            equ 511
ERR_UNHANDLED           equ 512
ERR_CRITICAL_SECTION    equ 513
ERR_SYSTEM_PANIC        equ 514
ERR_FATAL_RUNTIME       equ 515
ERR_CHECKSUM            equ 516
ERR_CORRUPTED_KERNEL    equ 517
ERR_SYSCALL             equ 518
ERR_KERNEL_FAULT        equ 519

; ---- system / UI -----------------------------------------------
ERR_WINDOW_MANAGER     equ 600
ERR_WINDOW_STATE        equ 601
ERR_COMPOSE             equ 602
ERR_SCREEN_BOUNDS       equ 603
ERR_TERM_OVERFLOW       equ 604
ERR_TERM_STATE          equ 605
ERR_SETTINGS_APPLY      equ 606
ERR_DISPLAY_PRESET      equ 607
ERR_NULL_POINTER        equ 608
ERR_USER_HALT           equ 609

; ---- hardware --------------------------------------------------
ERR_DISK_CONTROLLER     equ 700
ERR_DISK_TIMEOUT        equ 701
ERR_DISK_NOT_READY      equ 702
ERR_DISK_GEOMETRY       equ 703
ERR_DMA                 equ 704
ERR_IRQ_STORM           equ 705
ERR_TIMER               equ 706
ERR_CMOS                equ 707
ERR_HARDWARE            equ 708
ERR_UNKNOWN             equ 709

%ifndef GSOD_CODES_ONLY

; ------------------------------------------------------------
;  common GSOD text
; ------------------------------------------------------------
gsod_msg_title      db "SOMETING WENT WRONG", 0
gsod_msg_halted     db "SYSTEM HALTED", 0
gsod_msg_code       db "CODE OF ERROR: ", 0
gsod_msg_reset      db "RESET THE MACHINE.", 0
gsod_unknown_name   db "UNKNOWN ERROR", 0

; ------------------------------------------------------------
;  code -> name table (80 entries)
; ------------------------------------------------------------
gsod_error_count    equ 80

gsod_error_codes:
    dw ERR_BOOT_DISK, ERR_LOADER_IMAGE, ERR_LOADER_READ
    dw ERR_LOADER_HEADER, ERR_LOADER_LARGE, ERR_LOADER_CHECKSUM
    dw ERR_KERNEL_READ, ERR_KERNEL_HEADER, ERR_KERNEL_LARGE
    dw ERR_KERNEL_LOAD

    dw ERR_MEMORY_TEST, ERR_LOW_MEMORY, ERR_EXTENDED_MEMORY
    dw ERR_DIVIDE_BY_ZERO, ERR_INVALID_OPCODE, ERR_DOUBLE_FAULT
    dw ERR_GENERAL_PROTECTION, ERR_STACK_FAULT, ERR_OVERFLOW
    dw ERR_BOUNDS

    dw ERR_NO_VBE, ERR_VBE_MODE, ERR_LFB, ERR_FONT, ERR_VIDEO_MODE
    dw ERR_PALETTE, ERR_VGA_DEVICE, ERR_FRAMEBUFFER, ERR_RESOLUTION
    dw ERR_MODE_SWITCH

    dw ERR_KEYBOARD_INIT, ERR_KEYBOARD_CONTROLLER, ERR_KEYBOARD_OVERFLOW
    dw ERR_MOUSE_INIT, ERR_MOUSE_CONTROLLER, ERR_MOUSE_PACKET
    dw ERR_MOUSE_RANGE, ERR_SERIAL, ERR_SERIAL_TIMEOUT, ERR_RTC

    dw ERR_KERNEL_SEGMENT, ERR_ENTRY_POINT, ERR_KERNEL_SMALL
    dw ERR_UNEXPECTED_IRQ, ERR_IVT, ERR_STACK_OVERFLOW
    dw ERR_HEAP, ERR_BUFFER_OVERFLOW, ERR_POINTER, ERR_FUNCTION
    dw ERR_ASSERTION, ERR_WATCHDOG, ERR_UNHANDLED, ERR_CRITICAL_SECTION
    dw ERR_SYSTEM_PANIC, ERR_FATAL_RUNTIME, ERR_CHECKSUM
    dw ERR_CORRUPTED_KERNEL, ERR_SYSCALL, ERR_KERNEL_FAULT

    dw ERR_WINDOW_MANAGER, ERR_WINDOW_STATE, ERR_COMPOSE
    dw ERR_SCREEN_BOUNDS, ERR_TERM_OVERFLOW, ERR_TERM_STATE
    dw ERR_SETTINGS_APPLY, ERR_DISPLAY_PRESET, ERR_NULL_POINTER
    dw ERR_USER_HALT

    dw ERR_DISK_CONTROLLER, ERR_DISK_TIMEOUT, ERR_DISK_NOT_READY
    dw ERR_DISK_GEOMETRY, ERR_DMA, ERR_IRQ_STORM, ERR_TIMER
    dw ERR_CMOS, ERR_HARDWARE, ERR_UNKNOWN

gsod_error_names:
    dw gsod_name_100, gsod_name_101, gsod_name_102, gsod_name_103
    dw gsod_name_104, gsod_name_105, gsod_name_106, gsod_name_107
    dw gsod_name_108, gsod_name_109

    dw gsod_name_200, gsod_name_201, gsod_name_202, gsod_name_203
    dw gsod_name_204, gsod_name_205, gsod_name_206, gsod_name_207
    dw gsod_name_208, gsod_name_209

    dw gsod_name_300, gsod_name_301, gsod_name_302, gsod_name_303
    dw gsod_name_304, gsod_name_305, gsod_name_306, gsod_name_307
    dw gsod_name_308, gsod_name_309

    dw gsod_name_400, gsod_name_401, gsod_name_402, gsod_name_403
    dw gsod_name_404, gsod_name_405, gsod_name_406, gsod_name_407
    dw gsod_name_408, gsod_name_409

    dw gsod_name_500, gsod_name_501, gsod_name_502, gsod_name_503
    dw gsod_name_504, gsod_name_505, gsod_name_506, gsod_name_507
    dw gsod_name_508, gsod_name_509, gsod_name_510, gsod_name_511
    dw gsod_name_512, gsod_name_513, gsod_name_514, gsod_name_515
    dw gsod_name_516, gsod_name_517, gsod_name_518, gsod_name_519

    dw gsod_name_600, gsod_name_601, gsod_name_602, gsod_name_603
    dw gsod_name_604, gsod_name_605, gsod_name_606, gsod_name_607
    dw gsod_name_608, gsod_name_609

    dw gsod_name_700, gsod_name_701, gsod_name_702, gsod_name_703
    dw gsod_name_704, gsod_name_705, gsod_name_706, gsod_name_707
    dw gsod_name_708, gsod_name_709

gsod_name_100 db "BOOT DISK READ ERROR", 0
gsod_name_101 db "INVALID LOADER IMAGE", 0
gsod_name_102 db "LOADER READ ERROR", 0
gsod_name_103 db "INVALID LOADER HEADER", 0
gsod_name_104 db "LOADER TOO LARGE", 0
gsod_name_105 db "LOADER CHECK SUM FALIED", 0
gsod_name_106 db "KERNEL READ ERROR", 0
gsod_name_107 db "INVALID KERNEL HEADER", 0
gsod_name_108 db "KERNEL TOO LARGE", 0
gsod_name_109 db "KERNEL LOAD FALIED", 0

gsod_name_200 db "MEMORY TEST FALIED", 0
gsod_name_201 db "LOW MEMORY", 0
gsod_name_202 db "EXTENDED MEMORY ERROR", 0
gsod_name_203 db "DIVIDE BY ZERO", 0
gsod_name_204 db "INVALID OPCODE", 0
gsod_name_205 db "DOUBLE FAULT", 0
gsod_name_206 db "GENERAL PROTECTION FAULT", 0
gsod_name_207 db "STACK FAULT", 0
gsod_name_208 db "OVERFLOW EXCEPTION", 0
gsod_name_209 db "BOUNDS EXCEPTION", 0

gsod_name_300 db "NO VBE DEVICE", 0
gsod_name_301 db "VBE MODE FALIED", 0
gsod_name_302 db "LFB NOT AVAILABLE", 0
gsod_name_303 db "FONT LOAD FALIED", 0
gsod_name_304 db "UNSUPPORTED VIDEO MODE", 0
gsod_name_305 db "PALETTE LOAD FALIED", 0
gsod_name_306 db "VGA DEVICE NOT FOUND", 0
gsod_name_307 db "FRAMEBUFFER OVERFLOW", 0
gsod_name_308 db "RESOLUTION TOO LARGE", 0
gsod_name_309 db "MODE SWITCH FALIED", 0

gsod_name_400 db "KEYBOARD INIT FALIED", 0
gsod_name_401 db "KEYBOARD CONTROLLER ERROR", 0
gsod_name_402 db "KEYBOARD BUFFER OVERFLOW", 0
gsod_name_403 db "MOUSE INIT FALIED", 0
gsod_name_404 db "MOUSE CONTROLLER ERROR", 0
gsod_name_405 db "MOUSE PACKET ERROR", 0
gsod_name_406 db "MOUSE OUT OF RANGE", 0
gsod_name_407 db "SERIAL PORT FALIED", 0
gsod_name_408 db "SERIAL TIMEOUT", 0
gsod_name_409 db "RTC READ FALIED", 0

gsod_name_500 db "INVALID KERNEL SEGMENT", 0
gsod_name_501 db "INVALID ENTRY POINT", 0
gsod_name_502 db "KERNEL TOO SMALL", 0
gsod_name_503 db "UNEXPECTED INTERRUPT", 0
gsod_name_504 db "INVALID IVT ENTRY", 0
gsod_name_505 db "STACK OVERFLOW", 0
gsod_name_506 db "HEAP EXHAUSTED", 0
gsod_name_507 db "BUFFER OVERFLOW", 0
gsod_name_508 db "INVALID POINTER", 0
gsod_name_509 db "INVALID FUNCTION", 0
gsod_name_510 db "ASSERTION FALIED", 0
gsod_name_511 db "WATCHDOG TIMEOUT", 0
gsod_name_512 db "UNHANDLED EXCEPTION", 0
gsod_name_513 db "CRITICAL SECTION ERROR", 0
gsod_name_514 db "SYSTEM PANIC", 0
gsod_name_515 db "FATAL RUNTIME ERROR", 0
gsod_name_516 db "CHECK SUM FALIED", 0
gsod_name_517 db "CORRUPTED KERNEL IMAGE", 0
gsod_name_518 db "INVALID SYSTEM CALL", 0
gsod_name_519 db "KERNEL FAULT", 0

gsod_name_600 db "WINDOW MANAGER ERROR", 0
gsod_name_601 db "INVALID WINDOW STATE", 0
gsod_name_602 db "SCENE COMPOSE FALIED", 0
gsod_name_603 db "OUT OF SCREEN BOUNDS", 0
gsod_name_604 db "TERMINAL BUFFER OVERFLOW", 0
gsod_name_605 db "INVALID TERMINAL STATE", 0
gsod_name_606 db "SETTINGS APPLY FALIED", 0
gsod_name_607 db "DISPLAY PRESET FALIED", 0
gsod_name_608 db "NULL POINTER", 0
gsod_name_609 db "USER REQUESTED HALT", 0

gsod_name_700 db "DISK CONTROLLER ERROR", 0
gsod_name_701 db "DISK TIMEOUT", 0
gsod_name_702 db "DISK NOT READY", 0
gsod_name_703 db "DISK GEOMETRY ERROR", 0
gsod_name_704 db "DMA ERROR", 0
gsod_name_705 db "IRQ STORM", 0
gsod_name_706 db "TIMER FALIED", 0
gsod_name_707 db "CMOS BATTERY FALIED", 0
gsod_name_708 db "HARDWARE FAULT", 0
gsod_name_709 db "UNKNOWN ERROR", 0

; ------------------------------------------------------------
; gsod_find_name - AX = code -> SI = ASCIIZ name
; Preserves AX/BX/CX/DI; DS must point at the code segment.
; ------------------------------------------------------------
gsod_find_name:
    push bx
    push cx
    push di
    mov di, gsod_error_codes
    mov bx, gsod_error_names
    mov cx, gsod_error_count
.loop:
    cmp ax, [di]
    je .found
    add di, 2
    add bx, 2
    loop .loop
    mov si, gsod_unknown_name
    jmp .done
.found:
    mov si, [bx]
.done:
    pop di
    pop cx
    pop bx
    ret

%endif

%endif
