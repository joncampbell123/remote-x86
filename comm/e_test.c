
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include "e_lib.h"

int main(int argc,char **argv) {
	char buffer[4096];
	char *ip_addr = "192.168.250.254";
	struct remote_eth_connection *reth;
	int rd,i,x,y;

	if (argc >= 2)
		ip_addr = argv[1];

	fprintf(stderr,"Debugger address: %s\n",ip_addr);

	if ((reth = remote_eth_socket(ip_addr)) == NULL) {
		fprintf(stderr,"Cannot create socket\n");
		return 1;
	}

	if (!remote_eth_test(reth)) {
		fprintf(stderr,"TEST command failed\n");
		return 1;
	}

	if (!remote_eth_read(reth,0xB8000,buffer,4096)) {
		fprintf(stderr,"READ failed\n");
		return 1;
	}

	{	
		printf("+");
		for (x=0;x < 80;x++) printf("-");
		printf("+\n");

		for (y=0;y < 25;y++) {
			printf("|");
			for (x=0;x < 80;x++) {
				char c = (char)buffer[((y*80)+x)*2];
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
	}

	{
		const char *message = "I'm in ur ethernets haxin ur cpu";
		for (i=0;message[i] != 0;i++) {
			buffer[(i*2)  ] = message[i];
			buffer[(i*2)+1] = 0xE;
		}

		if (!remote_eth_write(reth,0xB8000,buffer,strlen(message)*2)) {
			fprintf(stderr,"WRITE failed\n");
			return 1;
		}
	}

	sleep(2);

	/* upload a program and run it */
	{
		int fd = open("vga_puke_32.bin",O_RDONLY);
		if (fd < 0) return 1;
		int sz = read(fd,buffer,sizeof(buffer));
		close(fd);

		if (!remote_eth_write(reth,0x80000,buffer,sz))
			fprintf(stderr,"Remote write fail\n");
		if (!remote_eth_exec(reth,0x80000,600))
			fprintf(stderr,"Exec fail\n");
	}

	remote_eth_close(reth);
	return 0;
}

