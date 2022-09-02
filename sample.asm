    mov iv, isr
    mov a, 10
    mov b, 0
sumloop:
    add b, b, a
    dec a
    jmp.nz @sumloop

    mov addr, 0x100
    mov mem, b

fin:
    jmp @fin

isr:
    push flag
    pop flag
    iret
