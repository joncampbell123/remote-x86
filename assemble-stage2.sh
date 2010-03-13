#!/bin/bash
ld -o stage2.o stage2-8086.o stage2-286.o -Ttext 0x8000
objcopy stage2.o stage2.bin -O binary --image-base 0x8000

