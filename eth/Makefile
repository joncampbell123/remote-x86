all: ../stage1-floppy.bin ../stage1-cdrom.bin stage2eth.bin floppyeth.bin

CFLAGS=-nostdlib -Os -fomit-frame-pointer -march=i386

../stage1-floppy.bin:
	make -C .. stage1-floppy.bin

../stage1-cdrom.bin:
	make -C .. stage1-cdrom.bin

clean:
	rm -f *.o *.bin *.iso

stage2eth.bin: stage2eth-base.o c-main.o vga.o keyb8042.o pit-8253.o interrupt.o pic-8259.o pci.o alloc.o net-pcnet32.o network.o end.o
	./assemble-stage2eth.sh $^

stage2eth-base.o: stage2eth-base.asm
	nasm -o $@ -f elf32 $<

end.o: end.asm
	nasm -o $@ -f elf32 $<

.c.o:
	i686-pc-linux-gnu-gcc $(CFLAGS) -c -o $@ $<

floppyeth-raw.bin: ../stage1-floppy.bin stage2eth.bin
	../stage1-floppy-patch-sectorcount ../stage1-floppy.bin stage2eth.bin
	cat ../stage1-floppy.bin stage2eth.bin >$@

floppyeth.bin: floppyeth-raw.bin
	(cat $< && cat /dev/zero) | dd of=$@ bs=512 count=2880

