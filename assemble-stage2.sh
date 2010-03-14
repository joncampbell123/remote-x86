#!/bin/bash
ld -o stage2.o stage2-8086.o stage2-286.o stage2-386-16.o stage2-386-32.o -Ttext 0x8000
objcopy stage2.o stage2.bin -O binary --image-base 0x8000

