    mov iv, isr # 割り込みルーチンを設定
    mov a, 10
    mov b, 0
sumloop:
    add b, b, a
    dec a, a
    jmp.nz @sumloop # @ は IP 相対アドレスの記号

    mov addr, 0x100
    mov mem, b

    # プログラムの終わり。halt 命令がないので無限ループ。
fin:
    jmp @fin

    # アセンブラで記述できない特殊な命令は dw 命令を使う。
    dw 0xDEAD
    dw 0xBEEF

isr:
    push flag
    pop flag
    iret
