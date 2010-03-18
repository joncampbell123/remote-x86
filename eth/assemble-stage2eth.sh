#!/bin/bash

# okay glom together
i686-pc-linux-gnu-ld -o stage2eth.o stage2eth-base.o end.o -Ttext 0x8000 || exit 1
i686-pc-linux-gnu-objcopy stage2eth.o stage2eth.bin -O binary --image-base 0x8000 || exit 1

