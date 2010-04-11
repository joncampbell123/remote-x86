#!/usr/bin/bash

procl() {
	echo "#"define __sym_addr__$5 0x$1
}

echo "#define __sym_addr_base 0x8000"

OBJ="$1"
(objdump -t "$1" | grep \.text | sort) | while read LIN; do
	procl $LIN
done

