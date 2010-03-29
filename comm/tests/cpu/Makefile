all: announce86.bin ud_exception_86.bin legacy_cpu_detect.bin

announce86.bin: announce86.asm
	nasm -f bin -o $@ $<

ud_exception_86.bin: ud_exception_86.asm
	nasm -f bin -o $@ $<

legacy_cpu_detect.bin: legacy_cpu_detect.asm
	nasm -f bin -o $@ $<

clean:
	rm -f *.bin *.o

