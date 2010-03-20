all: s_lib.a s_test vga_puke.bin

s_lib.a: s_lib.o
	ar r $@ $<

s_lib.o: s_lib.c
	gcc -c -o $@ $<

s_test: s_test.c
	gcc -o $@ $< s_lib.a

clean:
	rm -f s_test *.o *.bin *.a

vga_puke.bin: vga_puke.asm
	nasm -f bin -o $@ $<
