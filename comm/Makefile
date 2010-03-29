all: s_lib.a e_lib.a s_test vga_puke.bin vga_puke_32.bin e_test
	make -C tests

e_test: e_test.c
	gcc -o $@ $< e_lib.a

s_lib.a: s_lib.o
	ar r $@ $<

s_lib.o: s_lib.c
	gcc -c -o $@ $<

e_lib.a: e_lib.o
	ar r $@ $<

e_lib.o: e_lib.c
	gcc -c -o $@ $<

s_test: s_test.c
	gcc -o $@ $< s_lib.a

clean:
	make -C tests clean
	rm -f e_test s_test *.o *.bin *.a

vga_puke.bin: vga_puke.asm
	nasm -f bin -o $@ $<
vga_puke_32.bin: vga_puke_32.asm
	nasm -f bin -o $@ $<
