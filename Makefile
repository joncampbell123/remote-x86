all: Makefile.inc Makefile.inc.sh floppy.bin floppy-x64.bin cdrom.iso cdrom-x64.iso
	make -C eth
	make -C comm

Makefile.inc:
	./autodetect-tools make >Makefile.inc

Makefile.inc.sh:
	./autodetect-tools sh >Makefile.inc.sh

include Makefile.inc.sh

stage1-floppy.bin: stage1-floppy.asm
	nasm -o $@ -f bin $<

stage1-cdrom.bin: stage1-cdrom.asm
	nasm -o $@ -f bin $<





floppy-raw.bin: stage1-floppy.bin stage2.bin
	./stage1-floppy-patch-sectorcount stage1-floppy.bin stage2.bin
	cat stage1-floppy.bin stage2.bin >$@

floppy.bin: floppy-raw.bin
	(cat $< && cat /dev/zero) | dd of=$@ bs=512 count=2880

stage2.bin: stage2-8086.o stage2-286.o stage2-386-16.o stage2-386-32.o stage2-x64-stub.o end.o
	./assemble-stage2.sh

end.o: end.asm
	nasm -o $@ -f elf32 $<

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

cdrom.iso: stage1-cdrom.bin
	cat stage1-cdrom.bin stage2.bin >cdrom.bin
	mkisofs -o cdrom.iso -R -J -b cdrom.bin -no-emul-boot -boot-load-seg 0x780 -exclude-list cdrom.exclude .

cdrom-x64.iso: stage1-cdrom.bin
	cat stage1-cdrom.bin stage2-x64.bin >cdrom-x64.bin
	mkisofs -o cdrom-x64.iso -R -J -b cdrom-x64.bin -no-emul-boot -boot-load-seg 0x780 -exclude-list cdrom.exclude .

floppy-x64-raw.bin: stage1-floppy.bin stage2-x64.bin
	./stage1-floppy-patch-sectorcount stage1-floppy.bin stage2-x64.bin
	cat stage1-floppy.bin stage2-x64.bin >$@

floppy-x64.bin: floppy-x64-raw.bin
	(cat $< && cat /dev/zero) | dd of=$@ bs=512 count=2880

stage2-x64.bin: stage2-x64-8086.o stage2-x64-286.o stage2-x64-386-16.o stage2-x64-386-32.o stage2-x64-x64.o end-x64.o
	./assemble-stage2-x64.sh

end-x64.o: end.asm
	nasm -o $@ -f elf64 $<

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
	rm -f *.bin *.o *.iso
	find -name \*~ -delete
	make -C comm clean
	make -C eth clean
	rm -f Makefile.inc Makefile.inc.sh

