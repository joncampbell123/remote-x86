#!/bin/bash

. ../Makefile.inc.sh

# okay glom together
$LD_32 -m elf_i386 -o stage2eth.o --oformat elf32-i386 -A i386 $* -Ttext 0x8000 || exit 1
$OC_32 stage2eth.o stage2eth.bin -O binary || exit 1 # --image-base 0x8000

