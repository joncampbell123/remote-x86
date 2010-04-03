all: announce86.bin ud_exception_86.bin legacy_cpu_detect.bin ud_verify_386-32.bin ac_exception_386-32.bin cpuid_386-32.bin rdmsr_386-32.bin 386_popad.bin

386_popad.bin: 386_popad.asm
	nasm -f bin -o $@ $<

announce86.bin: announce86.asm
	nasm -f bin -o $@ $<

rdmsr_386-32.bin: rdmsr_386-32.asm
	nasm -f bin -o $@ $<

cpuid_386-32.bin: cpuid_386-32.asm
	nasm -f bin -o $@ $<

ud_exception_86.bin: ud_exception_86.asm
	nasm -f bin -o $@ $<

ud_verify_386-32.bin: ud_verify_386-32.asm
	nasm -f bin -o $@ $<

legacy_cpu_detect.bin: legacy_cpu_detect.asm
	nasm -f bin -o $@ $<

ac_exception_386-32.bin: ac_exception_386-32.asm
	nasm -f bin -o $@ $<

clean:
	rm -f *.bin *.o

