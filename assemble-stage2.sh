#!/bin/bash

. Makefile.shinfo

# okay glom together
$LD_32 -m elf_i386 -o stage2.o --oformat elf32-i386 -A i386 stage2-8086.o stage2-286.o stage2-386-16.o stage2-386-32.o stage2-x64-stub.o end.o -Ttext 0x8000 || exit 1
$OC_32 stage2.o stage2.bin -O binary || exit 1  # --image-base 0x8000

