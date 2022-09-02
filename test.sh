#!/bin/bash -u

make

ok=0
fail=0

function test_stdout() {
  want="$1"
  src="$2"
  got=$(echo $(echo "$src" | ./nlpasm))

  if [ "$want" = "$got" ]
  then
    echo "[  OK  ]: $src -> $got"
    ok=$((ok + 1))
  else
    echo "[FAILED]: $src -> $got, want $want"
    fail=$((fail + 1))
  fi
}

test_stdout "1225 E132"      "add.c a, sp, 0x32"
test_stdout "1225 E200 0032" "add.c a, sp, word 0x32"
test_stdout "1225 E200 FFFE" "add.c a, sp, @1"
test_stdout "1215 1605 121C 21D3 CAFE 121E E102" "
    add a, byte label1, b
    add addr, 0xCAFE, 0xd3
label1:
    add sp, sp, 2"
test_stdout "0074 1042" "mov.nz iv, 0x42"
test_stdout "001D 2000 CAFE" "jmp 0xCAFE"
test_stdout "1215 5101 117D D104" "
loop:
    add a, a, 1
    jmp.nz @loop"
test_stdout "111D 5600" "jmp a - b"
test_stdout "D015" "push a"
test_stdout "C01C" "pop addr"
test_stdout "B21D 5102 0015 1021 C01D" "
    call a+byte subr
subr:
    mov a, 33
    ret"
test_stdout "8015 2000 0123" "load a, 0x123"
test_stdout "9216 7200 0005" "store c + word 5, b"
test_stdout "111F A200 0123" "cmp mem, 0x123"
test_stdout "E01D" "iret"
test_stdout "1115 160A" "sub a, 10, b"
test_stdout "1617 5600" "addc c, a, b"
test_stdout "1517 5200 0400" "subc c, a, 1024"
test_stdout "0A15 61A5" "or a, b, 0xa5"
test_stdout "0C15 6000" "not a, b"
test_stdout "0E15 16FF" "xor a, 0xff, b"
test_stdout "0605 16F0" "and.nop a, 0xf0, b"
test_stdout "1B15 7000" "inc a, c"
test_stdout "1816 8000" "dec b, d"
test_stdout "1F17 5000" "incc c, a"
test_stdout "1C18 8000" "decc d, d"
test_stdout "2C15 5000" "slr a, a"
test_stdout "2015 6000" "sll a, b"
test_stdout "2C15 6000" "sar a, b"
test_stdout "2415 6000" "sal a, b"
test_stdout "2A16 7000" "ror b, c"
test_stdout "2216 7000" "rol b, c"
test_stdout "BEEF" "dw 0xbeef"
test_stdout "1215 E132" "
    add a, sp, 0x32 # sp+0x32 を add に代入
    # コメントは無視
    "

echo "----"
echo "PASSED: $ok, FAILED $fail"

if [ $fail -eq 0 ]
then
  echo "all tests passed!"
  exit 0
else
  echo "$fail test(s) failed..."
  exit 1
fi
