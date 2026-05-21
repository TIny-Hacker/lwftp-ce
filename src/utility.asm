    assume adl=1

    section .text

    public _asm_util_GetCharFromKey

_asm_util_GetCharFromKey: ; Scans for a keypress and converts it to a character
    di
    ld hl, $F50200
    ld (hl), h
    xor a, a

.loop:
    cp a, (hl)
    jr nz, .loop
    ld hl, $F50000 + 16 ; mpKeyRange + keyData
    ld bc, 56 shl 8

.getKeyLoop:
    ld a, b
    and a, 7
    jr nz, .sameGroup
    inc hl
    inc hl
    ld e, (hl)

.sameGroup:
    sla e
    jr nc, .loopCode
    xor a, a
    cp a, c
    jr nz, .return
    ld c, b

.loopCode:
    djnz .getKeyLoop

.return:
    pop de
    ex (sp), hl
    push de
    ld h, _rodata_sizeOfCharsLUT
    mlt hl
    ld de, _rodata_characters
    add hl, de
    ld a, c
    sub a, 9
    jr c, $ + 8
    cp a, _rodata_sizeOfCharsLUT + 1
    jr nc, $ + 4
    ld c, a
    add hl, bc
    ld a, (hl)
    ret

    section .rodata

    public _rodata_characters
    public _rodata_sizeOfCharsLUT

_rodata_characters:
    ; numbers
    db 0                       ; ENTER
    db '+-*/^', 0, 0           ; + - × ÷ ^ undef undef
    db '_369)!', 0, 0          ; (-) 3 6 9 ) TAN VARS undef
    db '.258(#', 0, 0          ; . 2 5 8 ( COS PRGM STAT
    db '0147,$', 0, 'X', 0     ; 0 1 4 7 , SIN APPS XT?n undef
    db '%&', 39, 0, 0, 0       ; STO LN LOG x2 x-1 MATH
_rodata_sizeOfCharsLUT := $ - _rodata_characters

    ; uppercase letters
    db 0                       ; ENTER
    db 34, 'WRMH', 0, 0        ; + - × ÷ ^ undef undef
    db '?', 0, 'VQLG', 0, 0    ; (-) 3 6 9 ) TAN VARS undef
    db ':ZUPKFC', 0            ; . 2 5 8 ( COS PRGM STAT
    db ' YTOJEBX', 0           ; 0 1 4 7 , SIN APPS XT?n undef
    db 'XSNIDA'                ; STO LN LOG x2 x-1 MATH

    ; lowercase letters
    db 0                       ; ENTER
    db 34, 'wrmh', 0, 0        ; + - × ÷ ^ undef undef
    db '?', 0, 'vqlg', 0, 0    ; (-) 3 6 9 ) TAN VARS undef
    db ':zupkfc', 0            ; . 2 5 8 ( COS PRGM STAT
    db ' ytojebX', 0           ; 0 1 4 7 , SIN APPS XT?n undef
    db 'xsnida'   
