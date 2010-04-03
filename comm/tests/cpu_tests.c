#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>

#include <termios.h>

#include "../s_lib.h"

const char*			stty_dev = "/dev/ttyS0";
int				stty_fd = -1;
int				dumb_tty = 0;

void help() {
	fprintf(stderr,"s_test [options]\n");
	fprintf(stderr,"  -h --help         help\n");
	fprintf(stderr,"  -d <dev>          Which device to use such as /dev/ttyS0\n");
	fprintf(stderr,"  -tty              Act like a dumb pass-through (debug)\n");
}

int parse_argv(int argc,char **argv) {
	int i;

	for (i=1;i < argc;) {
		const char *a = argv[i++];

		if (*a == '-') {
			while (*a == '-') a++;

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 1;
			}
			else if (!strcmp(a,"d")) {
				stty_dev = argv[i++];
			}
			else if (!strcmp(a,"tty")) {
				dumb_tty = 1;
			}
		}
		else {
		}
	}

	return 0;
}

void do_dumb_tty() {
	char c;

	while (1) {
		struct timeval tv;
		fd_set f;

		/* Serial in -> TTY out */
		FD_ZERO(&f);
		FD_SET(stty_fd,&f);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		if (select(stty_fd+1,&f,NULL,NULL,&tv) == 1) {
			read(stty_fd,&c,1);
			write(1,&c,1);	/* spit to STDOUT */
		}

		/* TTY in -> Serial out */
		FD_ZERO(&f);
		FD_SET(0,&f);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		if (select(1,&f,NULL,NULL,&tv) == 1) {
			read(0,&c,1);
			write(stty_fd,&c,1);	/* spit to STDOUT */
		}
	}
}

unsigned int seg_alloc = 0x20000;
unsigned int seg_announce86;
unsigned int seg_announce86_buffer;

int upload_code(int sfd,const char *file,unsigned int addr) {
	int fd = open(file,O_RDONLY);
	if (fd < 0) return 0;
	int sz = (int)lseek(fd,0,SEEK_END);
	unsigned char buffer[4096];
	int retr=0;
	int rd;

	fprintf(stderr,"Uploading '%s' to 0x%X (%u bytes)\n",file,addr,sz);
	lseek(fd,0,SEEK_SET);
	while (sz > 0) {
		int blksz = sz > 4096 ? 4096 : sz;
		rd = read(fd,buffer,blksz);
		if (rd < blksz) {
			fprintf(stderr,"Read error\n");
			retr = 0;
			break;
		}
		else if (!remote_rs232_write(sfd,addr,rd,buffer)) {
			fprintf(stderr,"Upload error\n");
			retr = 0;
			break;
		}
		retr += rd;
		sz -= rd;
	}

	close(fd);
	return retr;
}

int announce86_call(int stty_fd,unsigned int str_seg,unsigned int str_ofs) {
	uint16_t tmp[2];
	tmp[0] = str_ofs;
	tmp[1] = str_seg;
	if (!remote_rs232_write(stty_fd,seg_announce86,4,(char*)tmp))
		return 1;
	if (!remote_rs232_exec_seg_off(stty_fd,seg_announce86>>4,0x0004,10))
		return 1;

	return 1;
}

int announce86_imm(int sfd,const char *str) {
	unsigned char c = 0;
	size_t str_len = strlen(str);
	if (str_len > 255) {
		str_len = 255;
		fprintf(stderr,"BUG: announcement is too long\n");
	}

	fprintf(stderr,"ANNOUNCING: %s\n",str);
	if (!remote_rs232_write(sfd,seg_announce86_buffer,str_len,str))
		return 0;

	c = 0;
	if (!remote_rs232_write(sfd,seg_announce86_buffer+str_len,1,&c))
		return 0;

	return announce86_call(sfd,seg_announce86>>4,seg_announce86_buffer-seg_announce86);
}

/* what we know about the CPU */
struct x86_test_results {
	/* signals INT 6 (#UD) on invalid opcode ( > 8086 ) */
	unsigned int	has_ud_exception:1;

	/* decodes 0xF as POP CS (8088/8086) */
	unsigned int 	has_pop_cs:1;

	/* has CPUID (486, Pentium+) */
	unsigned int	has_cpuid:1;

	/* the CPU supports Alignment Check and will signal AC# */
	unsigned int	has_ac_exception:1;

	/* 8086-486 standard revision eflags test result (0-4) */
	unsigned char	std0to4_eflags_revision;

	/* the maximum memory address supported by the CPU (by it's basic design, 8086=1MB 286=16MB 386+=4GB, etc. not necessarily how many address pins) */
	unsigned int	max_basic_memory_address_bits;
	unsigned int	max_address_pins;		/* if we know the CPU model, how many actual CPU address pins there are */
	unsigned int	max_upper_bios_bits;		/* number of bits decoded for the upper ROM BIOS area (4GB-1) */
	unsigned int	max_legacy_bios_bits;		/* number of bits decoded for the legacy ROM BIOS area (0xF0000-0xFFFFF) */
	unsigned int	max_legacy_vga_bits;		/* number of bits decoded for the VGA display area (0xB8000-0xB8FFF) */
	unsigned int	max_legacy_dosmem_bits;		/* number of bits decoded for the "DOS" area (first 1MB) */
	unsigned int	max_extmem_bits;		/* number of bits decoded for extended memory (beyond the first 1MB) */

	/* CPUID */
	struct cpuid {
		uint32_t	highest_basic;		/* highest CPUID index */
		uint32_t	highest_extended;	/* highest extended index */
		char		vendor[12+1];		/* vendor string */
		char		ext_vendor[12+1];	/* vendor string (ext) */
		char		processor_name[48+1];	/* extended proc name */
		unsigned char	stepping;
		unsigned char	model,ext_model;
		unsigned char	family,ext_family;
		unsigned char	type;
		unsigned char	brand_id;
		unsigned int	clflush_size;		/* in bytes */
		unsigned char	logical_processors;
		unsigned char	local_apic_id;
		unsigned char	guest_physical_addr_bits;
		unsigned char	virtual_addr_bits;
		unsigned char	physical_addr_bits;
		unsigned char	apic_id_lsbs_core_id;
		unsigned int	cores;
		union cpuid_1_cf {	/* CPUID index 1 ECX */
			struct {
				/* 0..7 */
				uint32_t	sse3:1;
				uint32_t	pclmul:1;
				uint32_t	dtes64:1;
				uint32_t	mon:1;
				uint32_t	dscpl:1;
				uint32_t	vmx:1;
				uint32_t	smx:1;
				uint32_t	est:1;
				/* 8..15 */
				uint32_t	tm2:1;
				uint32_t	ssse3:1;
				uint32_t	l1dccm:1;
				uint32_t	reserved_11:1;
				uint32_t	fma:1;
				uint32_t	cmpxchg16b:1;
				uint32_t	etprd:1;
				uint32_t	pdcm:1;
				/* 16..23 */
				uint32_t	reserved_16:1;
				uint32_t	reserved_17:1;
				uint32_t	dca:1;
				uint32_t	sse41:1;
				uint32_t	sse42:1;
				uint32_t	xapic:1;
				uint32_t	movbe:1;
				uint32_t	popcnt:1;
				/* 24..31 */
				uint32_t	reserved_24:1;
				uint32_t	aes:1;
				uint32_t	xsave:1;
				uint32_t	osxsave:1;
				uint32_t	avx:1;
				uint32_t	reserved_29:1;
				uint32_t	reserved_30:1;
				uint32_t	reserved_31:1;
			} f;
			uint32_t		raw;
		} cpuid_1_cf;
		union cpuid_1_df {	/* CPUID index 1 EDX */
			struct {
				/* 0..7 */
				uint32_t	fpu:1;
				uint32_t	vme:1;
				uint32_t	de:1;
				uint32_t	pse:1;
				uint32_t	tsc:1;
				uint32_t	msr:1;
				uint32_t	pae:1;
				uint32_t	mce:1;
				/* 8..15 */
				uint32_t	cx8:1;
				uint32_t	apic:1;
				uint32_t	reserved_10:1;
				uint32_t	sep:1;
				uint32_t	mtrr:1;
				uint32_t	pge:1;
				uint32_t	mca:1;
				uint32_t	cmov:1;
				/* 16..23 */
				uint32_t	pat:1;
				uint32_t	pse36:1;
				uint32_t	psn:1;
				uint32_t	clfl:1;
				uint32_t	reserved_20:1;
				uint32_t	dtes:1;
				uint32_t	acpi:1;
				uint32_t	mmx:1;
				/* 24..31 */
				uint32_t	fxsr:1;
				uint32_t	sse:1;
				uint32_t	sse2:1;
				uint32_t	ss:1;
				uint32_t	htt:1;
				uint32_t	tm1:1;
				uint32_t	ia64:1;
				uint32_t	pbe:1;
			} f;
			uint32_t		raw;
		} cpuid_1_df;
		union cpuidex_1_cf {	/* CPUID extended index 1 ECX */
			struct {
				/* 0..7 */
				uint32_t	ahf64:1;
				uint32_t	cmp:1;
				uint32_t	svm:1;
				uint32_t	eas:1;
				uint32_t	cr8d:1;
				uint32_t	lzcnt:1;
				uint32_t	sse4a:1;
				uint32_t	msse:1;
				/* 8..15 */
				uint32_t	_3dnow:1;
				uint32_t	osvw:1;
				uint32_t	ibs:1;
				uint32_t	sse5a:1;
				uint32_t	skinit:1;
				uint32_t	wdt:1;
				uint32_t	reserved_14:1;
				uint32_t	reserved_15:1;
				/* 16..23 */
				uint32_t	reserved_16:1;
				uint32_t	reserved_17:1;
				uint32_t	reserved_18:1;
				uint32_t	reserved_19:1;
				uint32_t	reserved_20:1;
				uint32_t	reserved_21:1;
				uint32_t	reserved_22:1;
				uint32_t	reserved_23:1;
				/* 24..31 */
				uint32_t	reserved_24:1;
				uint32_t	reserved_25:1;
				uint32_t	reserved_26:1;
				uint32_t	reserved_27:1;
				uint32_t	reserved_28:1;
				uint32_t	reserved_29:1;
				uint32_t	reserved_30:1;
				uint32_t	reserved_31:1;
			} f;
			uint32_t		raw;
		} cpuidex_1_cf;
		union cpuidex_1_df {	/* CPUID extended index 1 EDX */
			struct {
				/* 0..7 */
				uint32_t	fpu:1;
				uint32_t	vme:1;
				uint32_t	de:1;
				uint32_t	pse:1;
				uint32_t	tsc:1;
				uint32_t	msr:1;
				uint32_t	pae:1;
				uint32_t	mce:1;
				/* 8..15 */
				uint32_t	cx8:1;
				uint32_t	apic:1;
				uint32_t	reserved_10:1;
				uint32_t	sep:1;
				uint32_t	mtrr:1;
				uint32_t	pge:1;
				uint32_t	mca:1;
				uint32_t	cmov:1;
				/* 16..23 */
				uint32_t	fcmov:1;
				uint32_t	pse36:1;
				uint32_t	reserved_18:1;
				uint32_t	mp:1;
				uint32_t	nx:1;
				uint32_t	reserved_21:1;
				uint32_t	mmx2:1;
				uint32_t	mmx:1;
				/* 24..31 */
				uint32_t	fxsr:1;
				uint32_t	ffxsr:1;
				uint32_t	pg1g:1;
				uint32_t	tscp:1;
				uint32_t	reserved_28:1;
				uint32_t	am64:1;
				uint32_t	_3dnow2:1;
				uint32_t	_3dnow:1;
			} f;
			uint32_t		raw;
		} cpuidex_1_df;
		union cpuidex_7_epm {	/* CPUID extended index 7 EDX */
			struct {
				/* 0..7 */
				uint32_t	ts:1;
				uint32_t	fid:1;
				uint32_t	vid:1;
				uint32_t	ttp:1;
				uint32_t	tm:1;
				uint32_t	stc:1;
				uint32_t	mul100:1;
				uint32_t	hwps:1;
				/* 8..15 */
				uint32_t	itsc:1;
				uint32_t	reserved_9:1;
				uint32_t	reserved_10:1;
				uint32_t	reserved_11:1;
				uint32_t	reserved_12:1;
				uint32_t	reserved_13:1;
				uint32_t	reserved_14:1;
				uint32_t	reserved_15:1;
				/* 16..23 */
				uint32_t	reserved_16:1;
				uint32_t	reserved_17:1;
				uint32_t	reserved_18:1;
				uint32_t	reserved_19:1;
				uint32_t	reserved_20:1;
				uint32_t	reserved_21:1;
				uint32_t	reserved_22:1;
				uint32_t	reserved_23:1;
				/* 24..31 */
				uint32_t	reserved_24:1;
				uint32_t	reserved_25:1;
				uint32_t	reserved_26:1;
				uint32_t	reserved_27:1;
				uint32_t	reserved_28:1;
				uint32_t	reserved_29:1;
				uint32_t	reserved_30:1;
				uint32_t	reserved_31:1;
			} f;
			uint32_t		raw;
		} cpuidex_7_epm;
	} cpuid;
};

void init_x86_test_results(struct x86_test_results *r) {
	memset(r,0,sizeof(*r));
}

int a20_enabled_mem_test(int stty_fd) {
	unsigned char buf0[32],buf1[32],cross[64],zero[32];
	unsigned int i;

	if (!remote_rs232_read(stty_fd,0x300,32,buf0))			/* 0x600 */
		return 0;
	if (!remote_rs232_read(stty_fd,0x300|(1<<20),32,buf1))		/* 0x600 possible alias */
		return 0;
	if (!remote_rs232_read(stty_fd,0x100000-32,64,cross))		/* 64 bytes crossing 1MB boundary */
		return 0;
	if (!remote_rs232_read(stty_fd,0x0,32,zero))			/* first 32 bytes of all memory */
		return 0;

	fprintf(stderr,"A20 test\n");
	fprintf(stderr,"  0x000300: "); for (i=0;i < 32;i++) fprintf(stderr,"%02X ",buf0[i]); fprintf(stderr,"\n");
	fprintf(stderr,"  0x100300: "); for (i=0;i < 32;i++) fprintf(stderr,"%02X ",buf1[i]); fprintf(stderr,"\n");
	fprintf(stderr,"  0x0FFFC0: "); for (i=0;i < 64;i++) fprintf(stderr,"%02X ",cross[i]); fprintf(stderr,"\n");
	fprintf(stderr,"  0x000000: "); for (i=0;i < 32;i++) fprintf(stderr,"%02X ",zero[i]); fprintf(stderr,"\n");

	if (!memcmp(zero,cross+32,32))
		return 0;

	if (!memcmp(buf0,buf1,32)) {
		char nbuf0[32];

		fprintf(stderr,"Just to be sure...\n");

		/* it might just be a coincidence, try writing to the memory and see if the alias shows it */
		memset(nbuf0,'x',32);
		if (!remote_rs232_write(stty_fd,0x300,32,nbuf0))
			return 0;
		if (!remote_rs232_read(stty_fd,0x300|(1<<20),32,buf1))
			return 0;
		if (!remote_rs232_write(stty_fd,0x300,32,buf0))
			return 0;
		if (!memcmp(nbuf0,buf1,32))
			return 0;

		fprintf(stderr,"  0x000300: "); for (i=0;i < 32;i++) fprintf(stderr,"%02X ",buf0[i]); fprintf(stderr,"\n");
		fprintf(stderr,"  0x100300: "); for (i=0;i < 32;i++) fprintf(stderr,"%02X ",buf1[i]); fprintf(stderr,"\n");
		fprintf(stderr,"  0x0FFFC0: "); for (i=0;i < 64;i++) fprintf(stderr,"%02X ",cross[i]); fprintf(stderr,"\n");
		fprintf(stderr,"  0x000000: "); for (i=0;i < 32;i++) fprintf(stderr,"%02X ",zero[i]); fprintf(stderr,"\n");
	}

	return 1;
}

/* NOTES:
 *
 *     On a 286, we try to exit out before the 386 tests, leaving the CPU in
 *     protected mode in case the machine is funky and can't leave properly
 *
 */
int run_tests(struct x86_test_results *cpu,int stty_fd) {
	unsigned char buf[80*25*2];
	uint32_t d;
	int x,y,sz;

	remote_rs232_test(stty_fd);
	remote_rs232_test(stty_fd);
	if (!remote_rs232_test(stty_fd)) {
		fprintf(stderr,"Remote test failed\n");
		return 1;
	}

	/* switch into 8086 mode */
	if (!remote_rs232_8086(stty_fd))
		return 1;

	/* decide where sub-programs go */
	if (!(sz=upload_code(stty_fd,"cpu/announce86.bin",seg_alloc))) return 1;
	seg_announce86 = seg_alloc;
	seg_alloc = (seg_alloc + sz + 0xF) & (~0xF);
	seg_announce86_buffer = seg_alloc;
	seg_alloc += 256;

	/* the bootloader never shuts off the floppy motor and the BIOS isn't there
	 * (when interrupts disabled) to timeout the floppy drive. */
	if (!(sz=upload_code(stty_fd,"misc/shutoff_floppy_motor.bin",seg_alloc))) return 1;
	if (!remote_rs232_exec_seg_off(stty_fd,seg_alloc>>4,0x0000,10))
		return 1;

	/* upload a program and run it */
	if (!announce86_imm(stty_fd,"CPU tests commencing now. Be prepared\r\n")) return 1;

/*========= TEST: Does the CPU silently ignore invalid opcodes like an 8086, or does it signal #UD (INT 6)?
 *                Part of this test involves POP CS, so if the CPU would actually load CS from the stack, we'll
 *                know from this test. */
	{
		unsigned char result[4];

		if (!(sz=upload_code(stty_fd,"cpu/ud_exception_86.bin",seg_alloc)))
			return 1;
		if (!remote_rs232_exec_seg_off(stty_fd,seg_alloc>>4,0x0004,10))
			return 1;
		if (!remote_rs232_read(stty_fd,seg_alloc,4,result))
			return 1;

		cpu->has_ud_exception =
			(result[0] == 1);
		cpu->has_pop_cs =
			(result[1] == 1);

		fprintf(stderr,"UD exception=%u, 8086 POP CS=%u\n",
				cpu->has_ud_exception,
				cpu->has_pop_cs);
	}

/*========= TEST: The official standard EFLAGS test for 8086/286/386/486. Everybody knows this one, I'm sure */
	{
		unsigned char result[4];

		if (!(sz=upload_code(stty_fd,"cpu/legacy_cpu_detect.bin",seg_alloc)))
			return 1;
		if (!remote_rs232_exec_seg_off(stty_fd,seg_alloc>>4,0x0004,10))
			return 1;
		if (!remote_rs232_read(stty_fd,seg_alloc,4,result))
			return 1;

		cpu->has_cpuid = (result[1] == 1);
		cpu->std0to4_eflags_revision = result[0];
		fprintf(stderr,"EFLAGS rev=%u, CPUID=%u\n",
				cpu->std0to4_eflags_revision,
				cpu->has_cpuid);
	}

	/* based on that, what's the basic max memory address */
	/* TODO: x64 hosts? */
	if (cpu->std0to4_eflags_revision >= 3)
		cpu->max_basic_memory_address_bits = 32;		// 4GB
	else if (cpu->std0to4_eflags_revision >= 2)
		cpu->max_basic_memory_address_bits = 24;		// 16MB
	else
		cpu->max_basic_memory_address_bits = 20;		// 1MB

	/* assume the CPU has at least that many address pins */
	cpu->max_address_pins = cpu->max_basic_memory_address_bits;

/*========================================= 80286 or higher beyond this point ===============================*/
	if (cpu->std0to4_eflags_revision < 2)
		return 0;

	/* HACK: Apparently some really old 386 laptops I have don't properly support the 286 reset trick.
	 *       Therefore, if we go into 286 protected mode we'll never get out properly and the test
	 *       will get stuck. Apparently on one, the BIOS will happily hang or beep a lot. */
	if (cpu->std0to4_eflags_revision >= 3) {	/* 386 or higher, use 16-bit 386 mode (our tests only care that it's 16-bit) */
		if (!remote_rs232_386_16(stty_fd))
			return 1;
	}
	else {						/* else use 286 16-bit mode */
		if (!remote_rs232_286(stty_fd))
			return 1;
	}

	/* enable A20 */
	if (!a20_enabled_mem_test(stty_fd)) {
		fprintf(stderr,"A20 doesn't seem to be enabled\n");

		if (!(sz=upload_code(stty_fd,"misc/a20_enable_fast.bin",seg_alloc))) return 1;
		if (!remote_rs232_exec_off(stty_fd,seg_alloc,10))
			return 1;

		usleep(1000);
	}

	/* enable A20 */
	{
		unsigned int retry=3;

		while (retry-- != 0 && !a20_enabled_mem_test(stty_fd)) {
			fprintf(stderr,"A20 doesn't seem to be enabled\n");

			if (!(sz=upload_code(stty_fd,"misc/a20_enable.bin",seg_alloc))) return 1;
			if (!remote_rs232_exec_off(stty_fd,seg_alloc,10))
				return 1;

			usleep(50000);
		}
	}

	if (!a20_enabled_mem_test(stty_fd)) {
		fprintf(stderr,"A20 doesn't seem to be enabled. I can't reliably carry out tests for 286+ machines without extended memory.\n");
		return 1;
	}

/*========================================= 80386 or higher beyond this point ===============================*/
	if (cpu->std0to4_eflags_revision < 3)
		return 0;

	/* switch into 386 32-bit mode */
	if (!remote_rs232_8086(stty_fd))
		return 1;
	if (!remote_rs232_386_32(stty_fd))
		return 1;

/*======== TEST: My programming skills are smart enough to handle a #UD exception in 32-bit protected mode */
	if (!(sz=upload_code(stty_fd,"cpu/ud_verify_386-32.bin",0x40000)))
		return 1;
	if (!remote_rs232_exec_off(stty_fd,0x40000+4,10))
		return 1;
	if (!remote_rs232_read(stty_fd,0x40000,4,(void*)(&d)))
		return 1;

	fprintf(stderr,"UD=0x%08lX\n",d);

	if (d == 0) {
		fprintf(stderr,"#UD never happened. It might be an undocumented opcode. Stopping tests now.\n");
		return 1;
	}
	else if (d != 0x12345678) {
		fprintf(stderr,"Corruption on readback\n");
		return 1;
	}

/*======== TEST: The 80386 POPAD bug, said to affect some 386SX and all 386DX Intel+AMD CPUs */
/*               TODO: Find a 386DX and run this test on it, see if it works */
	if (cpu->std0to4_eflags_revision == 3) { /* 386 only, no later revisions */
		/* verify we know how to safely handle #UD in 32-bit */
		if (!(sz=upload_code(stty_fd,"cpu/386_popad.bin",0x40000)))
			return 1;
		if (!remote_rs232_exec_off(stty_fd,0x40000+4,10))
			return 1;
		if (!remote_rs232_read(stty_fd,0x40000,4,(void*)(&d)))
			return 1;

		fprintf(stderr,"POPAD_BUG=0x%08lX\n",d);
	}

/*========================================= 80486 or higher beyond this point ===============================*/
	if (cpu->std0to4_eflags_revision < 4)
		return 0;

	/* cause #AC and note it */
	if (!(sz=upload_code(stty_fd,"cpu/ac_exception_386-32.bin",0x40000)))
		return 1;
	if (!remote_rs232_exec_off(stty_fd,0x40000+4,10))
		return 1;
	if (!remote_rs232_read(stty_fd,0x40000,4,(void*)(&d)))
		return 1;

	fprintf(stderr,"AC=0x%08lX\n",d);

	if (d == 0) {
		if (cpu->std0to4_eflags_revision >= 4) /* if it SHOULD have happened, then say so */
			fprintf(stderr,"Awwww, AC never happened. Your 486+ is weird...\n");
	}
	else if (d != 0x12345678) {
		fprintf(stderr,"Corruption on readback\n");
		return 1;
	}
	cpu->has_ac_exception = (d == 0x12345678);

/*======== TEST: If CPUID is present, read off the basic CPUID info */
	if (cpu->has_cpuid) {
#define EAX 0
#define EBX 1
#define ECX 2
#define EDX 3
#define IDX(x) (x*4)
		uint32_t vals[4*0x4];
		uint32_t extended[4*0x9];
		uint32_t results[0x18/4];
		/* [0]   W    EAX before CPUID
		 * [1] R      EAX after
		 * [2] R      EBX after
		 * [3] R      ECX after
		 * [4] R      EDX after 
		 * [5] R      exceptions that occured */

		/* switch into 386 32-bit mode */
		if (!remote_rs232_8086(stty_fd))
			return 1;
		if (!remote_rs232_386_32(stty_fd))
			return 1;

		if (!(sz=upload_code(stty_fd,"cpu/cpuid_386-32.bin",0x40000)))
			return 1;

		for (y=0;y < 0x4;y++) {
			results[0] = y;
			if (!remote_rs232_write(stty_fd,0x40000,0x4,(void*)(&results[0])))
				return 1;	
			if (!remote_rs232_exec_off(stty_fd,0x40000+0x18,10))
				return 1;
			if (!remote_rs232_read(stty_fd,0x40000,0x18,
				(void*)(&results[0])))
				return 1;

			fprintf(stderr,"[0x%08X]: A=%08X B=%08X C=%08X D=%08X X=%08X\n",
				results[0],results[1],results[2],
				results[3],results[4],results[5]);

			vals[(y*4)+0] = results[1];
			vals[(y*4)+1] = results[2];
			vals[(y*4)+2] = results[3];
			vals[(y*4)+3] = results[4];
		}

		for (y=0;y < 0x9;y++) {
			results[0] = y + 0x80000000;
			if (!remote_rs232_write(stty_fd,0x40000,0x4,(void*)(&results[0])))
				return 1;	
			if (!remote_rs232_exec_off(stty_fd,0x40000+0x18,10))
				return 1;
			if (!remote_rs232_read(stty_fd,0x40000,0x18,
				(void*)(&results[0])))
				return 1;

			fprintf(stderr,"[0x%08X]: A=%08X B=%08X C=%08X D=%08X X=%08X\n",
				results[0],results[1],results[2],
				results[3],results[4],results[5]);

			extended[(y*4)+0] = results[1];
			extended[(y*4)+1] = results[2];
			extended[(y*4)+2] = results[3];
			extended[(y*4)+3] = results[4];
		}

		cpu->cpuid.highest_basic = vals[0];
		cpu->cpuid.highest_extended = extended[0];
		fprintf(stderr,"Highest basic CPUID index=0x%08X, "
			"Highest basic extended=0x%08X\n",
			cpu->cpuid.highest_basic,
			cpu->cpuid.highest_extended);

		((uint32_t*)cpu->cpuid.vendor)[0] = vals[1];
		((uint32_t*)cpu->cpuid.vendor)[1] = vals[3];
		((uint32_t*)cpu->cpuid.vendor)[2] = vals[2];
		cpu->cpuid.vendor[12] = 0;
		fprintf(stderr,"Vendor=%s\n",cpu->cpuid.vendor);

		if (cpu->cpuid.highest_extended >= 0x80000000) {
			((uint32_t*)cpu->cpuid.ext_vendor)[0] = extended[1];
			((uint32_t*)cpu->cpuid.ext_vendor)[1] = extended[3];
			((uint32_t*)cpu->cpuid.ext_vendor)[2] = extended[2];
			cpu->cpuid.vendor[12] = 0;
			fprintf(stderr,"Ext. vendor=%s\n",cpu->cpuid.vendor);
		}

		cpu->cpuid.stepping =	 vals[IDX(1)+EAX]        & 0xF;
		cpu->cpuid.model =	(vals[IDX(1)+EAX] >>  4) & 0xF;
		cpu->cpuid.family =	(vals[IDX(1)+EAX] >>  8) & 0xF;
		cpu->cpuid.type =	(vals[IDX(1)+EAX] >> 12) & 0x3;
		cpu->cpuid.ext_model =	(vals[IDX(1)+EAX] >> 16) & 0xF;
		cpu->cpuid.ext_family =	(vals[IDX(1)+EAX] >> 20) & 0xFF;
		fprintf(stderr,"step=%u model=%u fam=%u type=%u extmodel=%u extfam=%u\n",
			cpu->cpuid.stepping,	cpu->cpuid.model,
			cpu->cpuid.family,	cpu->cpuid.type,
			cpu->cpuid.ext_model,	cpu->cpuid.ext_family);

		cpu->cpuid.brand_id =		  vals[IDX(1)+EBX]        & 0xFF;
		cpu->cpuid.clflush_size =	((vals[IDX(1)+EBX] >>  8) & 0xFF) * 8;
		cpu->cpuid.logical_processors =	 (vals[IDX(1)+EBX] >> 16) & 0xFF;
		cpu->cpuid.local_apic_id =	 (vals[IDX(1)+EBX] >> 24);
		fprintf(stderr,"brand=0x%02X clflush=%u logical_cpus=%u lapic_id=%u\n",
			cpu->cpuid.brand_id,		cpu->cpuid.clflush_size,
			cpu->cpuid.logical_processors,	cpu->cpuid.local_apic_id);

#define X(x) cpu->cpuid.cpuid_1_cf.f.x
		cpu->cpuid.cpuid_1_cf.raw = vals[IDX(1)+ECX];
		fprintf(stderr,"sse3=%u pclmul=%u dtes64=%u mon=%u "
				"dscpl=%u vmx=%u smx=%u est=%u\n",
				X(sse3),	X(pclmul),	X(dtes64),
				X(mon),		X(dscpl),	X(vmx),
				X(smx),		X(est));
		fprintf(stderr,"tm2=%u ssse3=%u l1dccm=%u fma=%u "
				"cmpxchg16b=%u etprd=%u pdcm=%u\n",
				X(tm2),		X(ssse3),	X(l1dccm),
				X(fma),		X(cmpxchg16b),	X(etprd),
				X(pdcm));
		fprintf(stderr,"dca=%u sse4.1=%u sse4.2=%u xapic=%u "
				"movbe=%u popcnt=%u\n",
				X(dca),		X(sse41),	X(sse42),
				X(xapic),	X(movbe),	X(popcnt));
		fprintf(stderr,"aes=%u xsave=%u osxsave=%u avx=%u\n",
				X(aes),		X(xsave),	X(osxsave),
				X(avx));
#undef X

#define X(x) cpu->cpuid.cpuid_1_df.f.x
		cpu->cpuid.cpuid_1_df.raw = vals[IDX(1)+EDX];
		fprintf(stderr,"fpu=%u vme=%u de=%u pse=%u "
				"tsc=%u msr=%u pae=%u mce=%u\n",
				X(fpu),		X(vme),		X(de),
				X(pse),		X(tsc),		X(msr),
				X(pae),		X(mce));
		fprintf(stderr,"cx8=%u apic=%u sep=%u mtrr=%u "
				"pge=%u mca=%u cmov=%u\n",
				X(cx8),		X(apic),	X(sep),
				X(mtrr),	X(pge),		X(mca),
				X(cmov));
		fprintf(stderr,"pat=%u pse36=%u psn=%u clflush=%u "
				"dtes=%u acpi=%u mmx=%u\n",
				X(pat),		X(pse36),	X(psn),
				X(clfl),	X(dtes),	X(acpi),
				X(mmx));
		fprintf(stderr,"fxsr=%u sse=%u sse2=%u selfsnoop=%u "
				"hyperthread=%u therm=%u ia64=%u pbe=%u\n",
				X(fxsr),	X(sse),		X(sse2),
				X(ss),		X(htt),		X(tm1),
				X(ia64),	X(pbe));
#undef X

		/* if PSN is set, get serial number from CPUID #3
		 * (TODO: drag out the old Pentium III and test this) */

		if (cpu->cpuid.highest_extended >= 0x80000000) {
			char *src = (char*)(&extended[IDX(2)]);
			char *src_f = src+48;
			while (src < src_f && *src == ' ') src++;
			char *dst = cpu->cpuid.processor_name;
			while (*src != 0 && src < src_f) *dst++ = *src++;
			*dst = 0;
			fprintf(stderr,"CPU name='%s'\n",cpu->cpuid.processor_name);

#define X(x) cpu->cpuid.cpuidex_1_cf.f.x
			cpu->cpuid.cpuidex_1_cf.raw = extended[IDX(1)+ECX];
			fprintf(stderr,"ahf64=%u cmp=%u svm=%u eas=%u "
					"cr8d=%u lzcnt=%u sse4a=%u msse=%u\n",
					X(ahf64),	X(cmp),		X(svm),
					X(eas),		X(cr8d),	X(lzcnt),
					X(sse4a),	X(msse));
			fprintf(stderr,"3dnow=%u osvw=%u ibs=%u sse5a=%u "
					"skinit=%u wdt=%u\n",
					X(_3dnow),	X(osvw),	X(ibs),
					X(sse5a),	X(skinit),	X(wdt));
#undef X

#define X(x) cpu->cpuid.cpuidex_1_df.f.x
			cpu->cpuid.cpuidex_1_df.raw = extended[IDX(1)+EDX];
			fprintf(stderr,"fpu=%u vme=%u de=%u pse=%u "
					"tsc=%u msr=%u pae=%u mce=%u\n",
					X(fpu),		X(vme),		X(de),
					X(pse),		X(tsc),		X(msr),
					X(pae),		X(mce));
			fprintf(stderr,"cx8=%u apic=%u sep=%u mtrr=%u "
					"pge=%u mca=%u cmov=%u\n",
					X(cx8),		X(apic),	X(sep),
					X(mtrr),	X(pge),		X(mca),
					X(cmov));
			fprintf(stderr,"fcmov=%u pse36=%u mp=%u nx=%u "
					"mmx2=%u mmx=%u\n",
					X(fcmov),	X(pse36),	X(mp),
					X(nx),		X(mmx2),	X(mmx));
			fprintf(stderr,"fxsr=%u ffxsr=%u pg1g=%u tscp=%u "
					"am64=%u 3dnow2=%u 3dnow=%u\n",
					X(fxsr),	X(ffxsr),	X(pg1g),
					X(tscp),	X(am64),	X(_3dnow2),
					X(_3dnow));
#undef X
		}

		if (cpu->cpuid.highest_extended >= 0x80000007) {
#define X(x) cpu->cpuid.cpuidex_7_epm.f.x
			cpu->cpuid.cpuidex_7_epm.raw = extended[IDX(7)+EDX];
			fprintf(stderr,"ts=%u fid=%u vid=%u ttp=%u tm=%u "
					"stc=%u mul100=%u hwps=%u itsc=%u\n",
					X(ts),		X(fid),		X(vid),
					X(ttp),		X(tm),		X(stc),
					X(mul100),	X(hwps),	X(itsc));
#undef X
		}

		if (cpu->cpuid.highest_extended >= 0x80000008) {
			cpu->cpuid.guest_physical_addr_bits =
				(extended[IDX(8)+EAX] >> 16) & 0xFF;
			cpu->cpuid.virtual_addr_bits =
				(extended[IDX(8)+EAX] >> 8) & 0xFF;
			cpu->cpuid.physical_addr_bits =
				extended[IDX(8)+EAX] & 0xFF;
			cpu->cpuid.apic_id_lsbs_core_id =
				(extended[IDX(8)+ECX] >> 12) & 0xF;
			cpu->cpuid.cores =
				(extended[IDX(8)+ECX] & 0xFF) + 1;

			fprintf(stderr,"guest_phys_addr_bits=%u virt_addr_bits=%u "
				"phys_addr_bits=%u cores=%u core_apic_lsbs=%u\n",
				cpu->cpuid.guest_physical_addr_bits,
				cpu->cpuid.virtual_addr_bits,
				cpu->cpuid.physical_addr_bits,
				cpu->cpuid.cores,
				cpu->cpuid.apic_id_lsbs_core_id);

		}

#undef EAX
#undef EBX
#undef ECX
#undef EDX
#undef IDX
	}

/*======== TEST: If CPUID says that machine status registers are present, then, read them off */
	if (cpu->cpuid.cpuid_1_df.f.msr) {
#define EXCEPT 0
#define EAX 1
#define EDX 2
#define IDX(x) (x*3)
		uint32_t vals[0x40*3];
		uint32_t ext_vals[0x40*3];
		uint32_t results[0x10/4];
		/* [0]   W    ECX before CPUID
		 * [1] R      mask of exceptions that occured (bit 6=UD 13=GPF)
		 * [2] R      EAX after
		 * [3] R      EDX after */

		if (!(sz=upload_code(stty_fd,"cpu/rdmsr_386-32.bin",0x40000)))
			return 1;

		for (y=0;y < 0x40;y++) {
			results[0] = y;
			if (!remote_rs232_write(stty_fd,0x40000,4,(void*)(&results[0])))
				return 1;	
			if (!remote_rs232_exec_off(stty_fd,0x40000+0x10,10))
				return 1;
			if (!remote_rs232_read(stty_fd,0x40000,0x10,(void*)(&results[0])))
				return 1;

			fprintf(stderr,"[0x%08X]: X=%08X D:A=%08X:%08X\n",
				results[0],results[1],results[3],results[2]);

			vals[(y*3)+0] = results[1];
			vals[(y*3)+1] = results[2];
			vals[(y*3)+2] = results[3];
		}

		for (y=0;y < 0x40;y++) {
			results[0] = y + 0x80000000;
			if (!remote_rs232_write(stty_fd,0x40000,4,(void*)(&results[0])))
				return 1;	
			if (!remote_rs232_exec_off(stty_fd,0x40000+0x10,10))
				return 1;
			if (!remote_rs232_read(stty_fd,0x40000,0x10,(void*)(&results[0])))
				return 1;

			fprintf(stderr,"[0x%08X]: X=%08X D:A=%08X:%08X\n",
				results[0],results[1],results[3],results[2]);

			ext_vals[(y*3)+0] = results[1];
			ext_vals[(y*3)+1] = results[2];
			ext_vals[(y*3)+2] = results[3];
		}
	}

	return 0;
}

int run_memory_aliasing_tests(struct x86_test_results *cpu,int stty_fd) {
	uint64_t max_basic_mem_address;
	unsigned int i;

	/* aliasing tests don't apply to the 8086, because A0-A19 are typically connected and compared correctly */
	if (cpu->std0to4_eflags_revision < 2)
		return 0;
	if (cpu->max_basic_memory_address_bits < 20)
		return 0;

	max_basic_mem_address = (1ULL << (uint64_t)(cpu->max_basic_memory_address_bits)) - 1ULL;

	/* ROM aliasing test (topmost ROM BIOS region).
	 * This is the actual ROM image that the CPU boots from.
	 * On a 286 or 386SX this is 0xFFFFF0, on a 386 or higher it is 0xFFFFFFF0.
	 * Sometimes the ROM BIOS will alias differently from the rest of the memory.
	 * On anything Pentium-class or higher, this will likely differ from the
	 * "legacy" BIOS region because of BIOSes that decompress themselves from
	 * EEPROM, or because of shadowing. */
	{
		unsigned char hirom[512],compare[512];
		uint64_t max = max_basic_mem_address >> 1ULL;
		unsigned int max_bits = cpu->max_basic_memory_address_bits - 1;
		if (!remote_rs232_read(stty_fd,max_basic_mem_address + 1ULL - 512ULL,512,hirom))
			return 1;

		/* start scanning and comparing */
		while (1) {
			if (!remote_rs232_read(stty_fd,max + 1ULL - 512ULL,512,compare))
				return 1;
			if (memcmp(compare,hirom,512)) {
				/* well, now it differs */
				max_bits++;
				max <<= 1;
				break;
			}

			max >>= 1;
			if (--max_bits <= 20) { /* it probably doesn't alias at all if we got this far */
				max_bits = cpu->max_basic_memory_address_bits;
				max = max_basic_mem_address;
				break;
			}
		}

		cpu->max_upper_bios_bits = max_bits;
	}

	/* ROM aliasing test (legacy ROM BIOS region).
	 * in newer machines this is either the last 64KB-128KB of the actual ROM, a
	 * decompressed shadow copy, or something else entirely. In older machines,
	 * this is likely a direct alias of the ROM BIOS */
	{
		unsigned char legrom[512],compare[512];
		unsigned int max_bits,upper,lower;

		if (!remote_rs232_read(stty_fd,0x100000 - 512,512,legrom))	/* 512 back from the 1MB boundary */
			return 1;

		/* start scanning and comparing */
		max_bits = cpu->max_basic_memory_address_bits - 1;
		while (1) {
			uint64_t alias_base = ((~0ULL) << max_bits) & max_basic_mem_address;
			if (!remote_rs232_read(stty_fd,alias_base + 0x100000 - 512ULL,512,compare)) /* 512 back from any alias of the 1MB boundary */
				return 1;
			if (memcmp(compare,legrom,512)) {
				/* well, now it differs */
				max_bits++;
				break;
			}

			if (--max_bits <= 20) { /* it probably doesn't alias at all if we got this far */
				max_bits = cpu->max_basic_memory_address_bits;
				break;
			}
		}
		upper = max_bits;

		/* start scanning and comparing */
		max_bits = cpu->max_basic_memory_address_bits - 1;
		while (1) {
			uint64_t alias_base = 1ULL << max_bits;
			if (!remote_rs232_read(stty_fd,alias_base + 0x100000 - 512ULL,512,compare)) /* 512 back from any alias of the 1MB boundary */
				return 1;
			if (memcmp(compare,legrom,512)) {
				/* well, now it differs */
				max_bits++;
				break;
			}

			if (--max_bits <= 20) { /* it probably doesn't alias at all if we got this far */
				max_bits = cpu->max_basic_memory_address_bits;
				break;
			}
		}
		lower = max_bits;

		if (upper != lower) fprintf(stderr,"WARNING: Inconsistent legacy BIOS scan results\n");
		cpu->max_legacy_bios_bits = lower;
	}

	/* VGA legacy display aliasing. Assuming the BIOS left the VGA display in mode 3, we
	 * can play with the 0xB8000-0xBFFFF area and detect aliasing that way. On most old
	 * machines, the VGA card is on the ISA bus, so this can be an indication of ISA bus
	 * aliasing. */
	{
		unsigned char display[512],compare[512];
		unsigned int max_bits,upper,lower;

		if (!remote_rs232_read(stty_fd,0xB8000,512,display))
			return 1;

		/* start scanning and comparing */
		max_bits = cpu->max_basic_memory_address_bits - 1;
		while (1) {
			uint64_t alias_base = ((~0ULL) << max_bits) & max_basic_mem_address;
			if (!remote_rs232_read(stty_fd,alias_base + 0xB8000,512,compare)) /* 512 back from any alias of the 1MB boundary */
				return 1;
			if (memcmp(compare,display,512)) {
				/* well, now it differs */
				max_bits++;
				break;
			}

			if (--max_bits <= 20) { /* it probably doesn't alias at all if we got this far */
				max_bits = cpu->max_basic_memory_address_bits;
				break;
			}
		}
		upper = max_bits;

		/* start scanning and comparing */
		max_bits = cpu->max_basic_memory_address_bits - 1;
		while (1) {
			uint64_t alias_base = 1ULL << max_bits;
			if (!remote_rs232_read(stty_fd,alias_base + 0xB8000,512,compare)) /* 512 back from any alias of the 1MB boundary */
				return 1;
			if (memcmp(compare,display,512)) {
				/* well, now it differs */
				max_bits++;
				break;
			}

			if (--max_bits <= 20) { /* it probably doesn't alias at all if we got this far */
				max_bits = cpu->max_basic_memory_address_bits;
				break;
			}
		}
		lower = max_bits;

		if (upper != lower) fprintf(stderr,"WARNING: Inconsistent legacy VGA scan results\n");
		cpu->max_legacy_vga_bits = lower;
	}

	/* DOS memory aliasing. Perhaps some motherboards treat this range separately. Most likely not, but
	 * it's fun to look for weird flaws like that */
	{
		unsigned char display[512],compare[512];
		unsigned int max_bits,upper,lower;

		/* WARNING: we assume that the BIOS has something there.
		 *          After all---that's where the real-mode interrupt table is supposed to be!
		 *          There's something seriously wrong with a BIOS that leaves nothing there! */

		if (!remote_rs232_read(stty_fd,0,512,display))
			return 1;

		/* start scanning and comparing */
		max_bits = cpu->max_basic_memory_address_bits - 1;
		while (1) {
			uint64_t alias_base = ((~0ULL) << max_bits) & max_basic_mem_address;
			if (!remote_rs232_read(stty_fd,alias_base,512,compare)) /* 512 back from any alias of the 1MB boundary */
				return 1;
			if (memcmp(compare,display,512)) {
				/* well, now it differs */
				max_bits++;
				break;
			}

			if (--max_bits <= 20) { /* it probably doesn't alias at all if we got this far */
				max_bits = cpu->max_basic_memory_address_bits;
				break;
			}
		}
		upper = max_bits;

		/* start scanning and comparing */
		max_bits = cpu->max_basic_memory_address_bits - 1;
		while (1) {
			uint64_t alias_base = 1ULL << max_bits;
			if (!remote_rs232_read(stty_fd,alias_base,512,compare)) /* 512 back from any alias of the 1MB boundary */
				return 1;
			if (memcmp(compare,display,512)) {
				/* well, now it differs */
				max_bits++;
				break;
			}

			if (--max_bits <= 20) { /* it probably doesn't alias at all if we got this far */
				max_bits = cpu->max_basic_memory_address_bits;
				break;
			}
		}
		lower = max_bits;

		if (upper != lower) fprintf(stderr,"WARNING: Inconsistent legacy VGA scan results\n");
		cpu->max_legacy_dosmem_bits = lower;
	}

	/* extended memory aliasing. in case it differs from DOS memory, which it usually doesn't.
	 * note we don't do this test if DOS memory testing reveals alising every 1MB. */
	if (cpu->max_legacy_dosmem_bits > 20) {
		unsigned char display[512],compare[512];
		unsigned int max_bits,upper,lower;

		/* WARNING: we assume that the BIOS has something there.
		 *          After all---that's where the real-mode interrupt table is supposed to be!
		 *          There's something seriously wrong with a BIOS that leaves nothing there! */

		if (!remote_rs232_read(stty_fd,0x100000,512,display))
			return 1;

		/* start scanning and comparing */
		max_bits = cpu->max_basic_memory_address_bits - 1;
		while (1) {
			uint64_t alias_base = ((~0ULL) << max_bits) & max_basic_mem_address;
			if (!remote_rs232_read(stty_fd,alias_base+0x100000,512,compare)) /* 512 back from any alias of the 1MB boundary */
				return 1;
			if (memcmp(compare,display,512)) {
				/* well, now it differs */
				max_bits++;
				break;
			}

			if (--max_bits <= 20) { /* it probably doesn't alias at all if we got this far */
				max_bits = cpu->max_basic_memory_address_bits;
				break;
			}
		}
		upper = max_bits;

		/* start scanning and comparing */
		max_bits = cpu->max_basic_memory_address_bits - 1;
		while (1) {
			uint64_t alias_base = 1ULL << max_bits;
			if (!remote_rs232_read(stty_fd,alias_base+0x100000,512,compare)) /* 512 back from any alias of the 1MB boundary */
				return 1;
			if (memcmp(compare,display,512)) {
				/* well, now it differs */
				max_bits++;
				break;
			}

			if (--max_bits <= 20) { /* it probably doesn't alias at all if we got this far */
				max_bits = cpu->max_basic_memory_address_bits;
				break;
			}
		}
		lower = max_bits;

		if (upper != lower) fprintf(stderr,"WARNING: Inconsistent legacy VGA scan results\n");
		cpu->max_extmem_bits = lower;
	}

	fprintf(stderr,"ROM BIOS (topmost):   Matches %u/%u bits\n",cpu->max_upper_bios_bits,cpu->max_basic_memory_address_bits);
	fprintf(stderr,"ROM BIOS (legacy):    Matches %u/%u bits\n",cpu->max_legacy_bios_bits,cpu->max_basic_memory_address_bits);
	fprintf(stderr,"VGA (legacy):         Matches %u/%u bits\n",cpu->max_legacy_vga_bits,cpu->max_basic_memory_address_bits);
	fprintf(stderr,"DOS memory (legacy):  Matches %u/%u bits\n",cpu->max_legacy_dosmem_bits,cpu->max_basic_memory_address_bits);
	fprintf(stderr,"Extended memory:      Matches %u/%u bits\n",cpu->max_extmem_bits,cpu->max_basic_memory_address_bits);

	return 0;
}

int main(int argc,char **argv) {
	struct x86_test_results cpu;

	init_x86_test_results(&cpu);

	if (parse_argv(argc,argv))
		return 1;

	if ((stty_fd = open(stty_dev,O_RDWR)) < 0) {
		fprintf(stderr,"Cannot open %s\n",stty_dev);
		return 1;
	}

	remote_rs232_configure(stty_fd);

	if (dumb_tty) {
		do_dumb_tty();
	}
	else {
		if (run_tests(&cpu,stty_fd))
			return 1;
		if (run_memory_aliasing_tests(&cpu,stty_fd))
			return 1;
	}

	close(stty_fd);
	return 0;
}

