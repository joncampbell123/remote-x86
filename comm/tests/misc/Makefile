all: shutoff_floppy_motor.bin a20_enable.bin a20_enable_fast.bin

a20_enable.bin: a20_enable.asm
	nasm -f bin -o $@ $<

a20_enable_fast.bin: a20_enable_fast.asm
	nasm -f bin -o $@ $<

shutoff_floppy_motor.bin: shutoff_floppy_motor.asm
	nasm -f bin -o $@ $<

clean:
	rm -f *.bin *.o

