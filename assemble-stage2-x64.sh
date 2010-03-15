#!/bin/bash

# okay glom together
x86_64-pc-linux-gnu-ld -o stage2-x64.o stage2-x64-8086.o stage2-x64-286.o stage2-x64-386-16.o stage2-x64-386-32.o stage2-x64-x64.o end-x64.o -Ttext 0x8000 || exit 1
x86_64-pc-linux-gnu-objcopy stage2-x64.o stage2-x64.bin -O binary --image-base 0x8000 || exit 1

