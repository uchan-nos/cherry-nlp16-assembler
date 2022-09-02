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
test_stdout "D21D 5102 0015 1021" "
    call a+byte subr
subr:
    mov a, 33"

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
