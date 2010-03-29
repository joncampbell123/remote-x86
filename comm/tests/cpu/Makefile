all: announce86.bin

announce86.bin: announce86.asm
	nasm -f bin -o $@ $<

clean:
	rm -f *.bin *.o

