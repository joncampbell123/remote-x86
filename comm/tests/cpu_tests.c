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
		}
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

