#!/bin/bash
set -ex
riscv64-unknown-elf-gcc -g -Og -ffreestanding -nostdlib -nostartfiles   -T test_ctr_in_spike/spike.lds -o test_ctr_in_spike/div_example.elf test_ctr_in_spike/div_example.S
riscv64-unknown-elf-gcc -g -Og -ffreestanding -nostdlib -nostartfiles   -T test_ctr_in_spike/spike.lds -o test_ctr_in_spike/easy_ctr.elf test_ctr_in_spike/easy_ctr.c
#riscv64-unknown-elf-objdump -d -M numeric -S test_ctr_in_spike/easy_ctr.elf > test_ctr_in_spike/dump.out
  for f in test_ctr_in_spike/*.elf; do
    riscv64-unknown-elf-objdump -x -D -M no-aliases,numeric -S "$f" > "${f%.elf}.out"
  done

#riscv64-unknown-elf-objdump -x -d -M numeric test_ctr_in_spike/easy_ctr.elf > test_ctr_in_spike/fulldump.out
#SPIKE_DEP_RAW=0 .tools/bin/spike --ctr=top --o=leaks.txt -m0x10100000:0x20000 test_ctr_in_spike/div_example.elf
.tools/bin/spike --ctr=top --o=leaks.txt -m0x10100000:0x20000 test_ctr_in_spike/div_example.elf