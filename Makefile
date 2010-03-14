all: floppy.bin

floppy-raw.bin: stage1-floppy.bin stage2.bin
	cat stage1-floppy.bin stage2.bin >$@
	./stage1-floppy-patch-sectorcount stage1-floppy.bin stage2.bin

floppy.bin: floppy-raw.bin
	(cat $< && dd if=/dev/urandom bs=512 count=50 && cat /dev/zero) | dd of=$@ bs=512 count=2880

stage1-floppy.bin: stage1-floppy.asm
	nasm -o $@ -f bin $<

stage2.bin: stage2-8086.o stage2-286.o stage2-386-16.o stage2-386-32.o
	./assemble-stage2.sh

stage2-8086.o: stage2-8086.asm
	nasm -o $@ -f elf $<

stage2-286.o: stage2-286.asm
	nasm -o $@ -f elf $<

stage2-386-16.o: stage2-386-16.asm
	nasm -o $@ -f elf $<

stage2-386-32.o: stage2-386-32.asm
	nasm -o $@ -f elf $<

clean:
	rm -f *.bin *.o
	find -name \*~ -delete

