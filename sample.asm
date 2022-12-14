    .origin 0x10
    mov iv, isr # 割り込みルーチンを設定
    mov a, 10
    mov b, 0
sumloop:
    add b, b, a
    dec a, a
    jmp.nz @sumloop # @ は IP 相対アドレスの記号

    mov addr, 0x100
    mov mem, b

    call @fib

    # プログラムの終わり。halt 命令がないので無限ループ。
fin:
    jmp @fin

    # アセンブラで記述できない特殊な命令は dw 命令を使う。
    .dw 0xDEAD, 0xBEEF

isr:
    push flag
    store b-0x220, a
    pop flag
    iret

fib:
    add c, a, b
    ret.s
    mov e, c # UART 出力
    mov a, b
    mov b, c
    jmp fib

    .origin 0x200
msg:
    .dw 0x4865, 0x6C6C, 0x6F00  # H e l l o \0
