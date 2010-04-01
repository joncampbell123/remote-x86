#!/bin/bash

. Makefile.inc.sh

# okay glom together
$LD_64 -m elf_x86_64 --oformat elf64-x86-64 -A x86_64 -o stage2-x64.o stage2-x64-8086.o stage2-x64-286.o stage2-x64-386-16.o stage2-x64-386-32.o stage2-x64-x64.o end-x64.o -Ttext 0x8000 || exit 1
$OC_64 stage2-x64.o stage2-x64.bin -O binary || exit 1 # --image-base 0x8000

