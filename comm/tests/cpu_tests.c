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

	/* CPUID */
	struct cpuid {
		uint32_t	highest_basic;		/* highest CPUID index */
		uint32_t	highest_extended;	/* highest extended index */
		char		vendor[12+1];		/* vendor string */
		unsigned char	stepping;
		unsigned char	model,ext_model;
		unsigned char	family,ext_family;
		unsigned char	type;
		unsigned char	brand_id;
		unsigned int	clflush_size;		/* in bytes */
		unsigned char	logical_processors;
		unsigned char	local_apic_id;
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
	} cpuid;
};

void init_x86_test_results(struct x86_test_results *r) {
	memset(r,0,sizeof(*r));
}

int run_tests(struct x86_test_results *cpu,int stty_fd) {
	unsigned char buf[80*25*2];
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

	/* upload a program and run it */
	if (!announce86_imm(stty_fd,"CPU tests commencing now. Be prepared\r\n")) return 1;

	{
		unsigned char result[4];

		/* INT 6 undefined opcode test */
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

	{
		unsigned char result[4];

		/* standard 8086-486 EFLAGS test */
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

	if (cpu->std0to4_eflags_revision >= 3) { /* 80386 or higher */
		/* switch into 386 32-bit mode */
		if (!remote_rs232_8086(stty_fd))
			return 1;
		if (!remote_rs232_386_32(stty_fd))
			return 1;

		{
			uint32_t d;

			/* verify we know how to safely handle #UD in 32-bit */
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
		}
	}

	if (cpu->std0to4_eflags_revision >= 4) { /* 80486 or higher */
		/* switch into 386 32-bit mode */
		if (!remote_rs232_8086(stty_fd))
			return 1;
		if (!remote_rs232_386_32(stty_fd))
			return 1;

		{
			uint32_t d;

			/* cause #AC and note it */
			if (!(sz=upload_code(stty_fd,"cpu/ac_exception_386-32.bin",0x40000)))
				return 1;
			if (!remote_rs232_exec_off(stty_fd,0x40000+4,10))
				return 1;
			if (!remote_rs232_read(stty_fd,0x40000,4,(void*)(&d)))
				return 1;

			fprintf(stderr,"AC=0x%08lX\n",d);

			if (d == 0) {
				fprintf(stderr,"Awwww, AC never happened\n");
			}
			else if (d != 0x12345678) {
				fprintf(stderr,"Corruption on readback\n");
				return 1;
			}
			cpu->has_ac_exception = (d == 0x12345678);
		}
	}

	/* CPUID */
	if (cpu->has_cpuid) {
#define EAX 0
#define EBX 1
#define ECX 2
#define EDX 3
#define IDX(x) (x*4)
		uint32_t vals[4*0x10];
		uint32_t extended[4*0x10];
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

		for (y=0;y < 0x10;y++) {
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

		for (y=0;y < 0x10;y++) {
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

#undef EAX
#undef EBX
#undef ECX
#undef EDX
#undef IDX
	}

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
		if (!run_tests(&cpu,stty_fd))
			return 1;
	}

	close(stty_fd);
	return 0;
}

