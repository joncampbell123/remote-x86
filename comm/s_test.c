#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

#include <termios.h>

#include "s_lib.h"

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
		int x,y;

		remote_rs232_test(stty_fd);
		remote_rs232_test(stty_fd);
		if (!remote_rs232_test(stty_fd))
			fprintf(stderr,"Remote test failed\n");

		memset(buf,'x',sizeof(buf));
		if (!remote_rs232_read(stty_fd,0xB8000,sizeof(buf),buf))
			fprintf(stderr,"Remote read failed\n");
	
		printf("+");
		for (x=0;x < 80;x++) printf("-");
		printf("+\n");

		for (y=0;y < 25;y++) {
			printf("|");
			for (x=0;x < 80;x++) {
				char c = (char)buf[((y*80)+x)*2];
				if (c >= 32 && c < 0x7F)
					printf("%c",c);
				else
					printf(" ");
			}
			printf("|\n");
		}
	
		printf("+");
		for (x=0;x < 80;x++) printf("-");
		printf("+\n");

		if (!remote_rs232_8086(stty_fd))
			fprintf(stderr,"failed\n");

		if (!remote_rs232_8086(stty_fd))
			fprintf(stderr,"failed\n");
		if (!remote_rs232_286(stty_fd))
			fprintf(stderr,"failed\n");

		if (!remote_rs232_8086(stty_fd))
			fprintf(stderr,"failed\n");
		if (!remote_rs232_386_16(stty_fd))
			fprintf(stderr,"failed\n");

		if (!remote_rs232_8086(stty_fd))
			fprintf(stderr,"failed\n");
		if (!remote_rs232_386_32(stty_fd))
			fprintf(stderr,"failed\n");

		if (!remote_rs232_8086(stty_fd))
			fprintf(stderr,"failed\n");
		if (!remote_rs232_x64(stty_fd))
			fprintf(stderr,"failed\n");

		if (!remote_rs232_8086(stty_fd))
			fprintf(stderr,"failed\n");

		const char *str1 = "LOL I PWN3D j00 HAX HAX";
		for (x=0;x < strlen(str1);x++) {
			buf[x*2] = str1[x];
			buf[(x*2)+1] = 0x0E;
		}

		if (!remote_rs232_write(stty_fd,0xB8000,strlen(str1)*2,buf))
			fprintf(stderr,"Remote write failed\n");

		sleep(2);

		/* upload a program and run it */
		{
			int fd = open("vga_puke.bin",O_RDONLY);
			if (fd < 0) return 1;
			int sz = read(fd,buf,sizeof(buf));
			close(fd);
			assert(sz > 0);

			if (!remote_rs232_write(stty_fd,0x20000,sz,buf))
				fprintf(stderr,"Remote write fail\n");
			if (!remote_rs232_exec_seg_off(stty_fd,0x2000,0x0000,600))
				fprintf(stderr,"Exec fail\n");
		}
	}

	close(stty_fd);
	return 0;
}

