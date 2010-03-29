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

int main(int argc,char **argv) {
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
			fprintf(stderr,"UD exception = %u\n",result[0]);
			fprintf(stderr,"8086 POP CS = %u\n",result[1]);
		}

		{
			unsigned char result[4];

			/* INT 6 undefined opcode test */
			if (!(sz=upload_code(stty_fd,"cpu/legacy_cpu_detect.bin",seg_alloc)))
				return 1;
			if (!remote_rs232_exec_seg_off(stty_fd,seg_alloc>>4,0x0004,10))
				return 1;
			if (!remote_rs232_read(stty_fd,seg_alloc,4,result))
				return 1;
			fprintf(stderr,"Rev = %u, CPUID = %u\n",result[0],result[1]);
		}
	}

	close(stty_fd);
	return 0;
}

