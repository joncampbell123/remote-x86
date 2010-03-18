all: ../stage1-floppy.bin ../stage1-cdrom.bin stage2eth.bin floppyeth.bin

../stage1-floppy.bin:
	make -C .. stage1-floppy.bin

../stage1-cdrom.bin:
	make -C .. stage1-cdrom.bin

clean:
	rm -f *.o *.bin *.iso

stage2eth.bin: stage2eth-base.o end.o
	./assemble-stage2eth.sh

stage2eth-base.o: stage2eth-base.asm
	nasm -o $@ -f elf32 $<

end.o: end.asm
	nasm -o $@ -f elf32 $<

floppyeth-raw.bin: ../stage1-floppy.bin stage2eth.bin
	../stage1-floppy-patch-sectorcount ../stage1-floppy.bin stage2eth.bin
	cat ../stage1-floppy.bin stage2eth.bin >$@

floppyeth.bin: floppyeth-raw.bin
	(cat $< && cat /dev/zero) | dd of=$@ bs=512 count=2880

