all: floppy.bin floppy-x64.bin

stage1-floppy.bin: stage1-floppy.asm
	nasm -o $@ -f bin $<






floppy-raw.bin: stage1-floppy.bin stage2.bin
	./stage1-floppy-patch-sectorcount stage1-floppy.bin stage2.bin
	cat stage1-floppy.bin stage2.bin >$@

floppy.bin: floppy-raw.bin
	(cat $< && cat /dev/zero) | dd of=$@ bs=512 count=2880

stage2.bin: stage2-8086.o stage2-286.o stage2-386-16.o stage2-386-32.o stage2-x64-stub.o
	./assemble-stage2.sh

stage2-8086.o: stage2-8086.asm
	nasm -o $@ -f elf32 $<

stage2-286.o: stage2-286.asm
	nasm -o $@ -f elf32 $<

stage2-386-16.o: stage2-386-16.asm
	nasm -o $@ -f elf32 $<

stage2-386-32.o: stage2-386-32.asm
	nasm -o $@ -f elf32 $<

stage2-x64-stub.o: stage2-x64-stub.asm
	nasm -o $@ -f elf32 $<






floppy-x64-raw.bin: stage1-floppy.bin stage2-x64.bin
	./stage1-floppy-patch-sectorcount stage1-floppy.bin stage2-x64.bin
	cat stage1-floppy.bin stage2-x64.bin >$@

floppy-x64.bin: floppy-x64-raw.bin
	(cat $< && cat /dev/zero) | dd of=$@ bs=512 count=2880

stage2-x64.bin: stage2-x64-8086.o stage2-x64-286.o stage2-x64-386-16.o stage2-x64-386-32.o stage2-x64-x64.o
	./assemble-stage2-x64.sh

stage2-x64-8086.o: stage2-8086.asm
	nasm -o $@ -f elf64 $<

stage2-x64-286.o: stage2-286.asm
	nasm -o $@ -f elf64 $<

stage2-x64-386-16.o: stage2-386-16.asm
	nasm -o $@ -f elf64 $<

stage2-x64-386-32.o: stage2-386-32.asm
	nasm -o $@ -f elf64 $<

stage2-x64-x64.o: stage2-x64.asm
	nasm -o $@ -f elf64 $<





clean:
	rm -f *.bin *.o
	find -name \*~ -delete

