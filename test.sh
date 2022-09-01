#!/bin/bash -u

make

function test_stdout() {
  want="$1"
  src="$2"
  got=$(echo $(echo "$src" | ./nlpasm))

  if [ "$want" = "$got" ]
  then
    echo "[  OK  ]: $src -> $got"
  else
    echo "[FAILED]: $src -> $got, want $want"
  fi
}

test_stdout "1225 E200 0032" "add.c a, sp, word 0x32"
test_stdout "1215 1605 121C 21D3 CAFE 121E E102" "
    add a, byte label1, b
    add addr, 0xCAFE, 0xd3
label1:
    add sp, sp, 2"
