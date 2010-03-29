#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

#include <termios.h>

int read_forced(int fd,void *ptr,int sz) {
	int r=0;

	while (sz > 0) {
		int rd = read(fd,ptr,sz);
		if (rd == 0) {
			fprintf(stderr,"read_forced() got zero bytes\n");
			return r == 0 ? -1 : r;
		}
		else if (rd < 0) continue;
		ptr = (void*)(((char*)ptr) + rd);
		sz -= rd;
		r += rd;
	}

	return r;
}

int remote_rs232_send_command(int fd,const char *cmd) {
	fprintf(stderr,">> '%s'\n",cmd);
	size_t cmd_len = strlen(cmd);
	while (cmd_len > 0) {
		int wd = write(fd,cmd,cmd_len);
		if (wd > 0) {
			cmd_len -= wd;
			cmd += wd;
		}
		else if (wd == 0) {
			break;
		}
	}
	int wd;
	while ((wd=write(fd,"\n",1)) == -1);
	if (wd == 0) return 0;
	return 1;
}

int remote_rs232_get_response(int fd,char *buf,size_t len,unsigned long timeout) {
	char *wp = buf,*fence = buf + len - 1;
	time_t et = time(NULL) + ((timeout+999999)/1000000) + 1 + 1;
	int ret = 0;
	char c;

	fcntl(fd,F_SETFL,fcntl(fd,F_GETFL) | O_NONBLOCK);

	do {
		if (time(NULL) >= et) { /* timeout */
			ret = -1;
			break;
		}

		c = 0;
		int r = read(fd,&c,1);
		if (r == 0) {
			ret = -1;
			break;
		}
		else if (r < 0)
			continue;

		if (c >= 32 || c < 0) {
			if (wp < fence) {
				*wp++ = c;
			}
		}
	} while (c != '\n');
	*wp = (char)0;

	if (ret == 0 && !strncmp(buf,"OK",2))
		ret = 1;

	fprintf(stderr,"<< '%s'\n",buf);
	if (ret == -1) fprintf(stderr,"(incomplete read, timeout)\n");
	fcntl(fd,F_SETFL,fcntl(fd,F_GETFL) & (~O_NONBLOCK));
	return ret;
}

int remote_rs232_test(int fd) {
	char line[128];
	if (!remote_rs232_send_command(fd,"TEST")) return 0;
	if (remote_rs232_get_response(fd,line,sizeof(line),1000000) != 1) return 0;
	return 1;
}

int remote_rs232_8086(int fd) {
	char line[128];
	if (!remote_rs232_send_command(fd,"8086")) return 0;
	if (remote_rs232_get_response(fd,line,sizeof(line),1000000) != 1) return 0;
	return 1;
}

int remote_rs232_286(int fd) {
	char line[128];
	if (!remote_rs232_send_command(fd,"286")) return 0;
	if (remote_rs232_get_response(fd,line,sizeof(line),1000000) != 1) return 0;
	return 1;
}

int remote_rs232_386_16(int fd) {
	char line[128];
	if (!remote_rs232_send_command(fd,"386-16")) return 0;
	if (remote_rs232_get_response(fd,line,sizeof(line),1000000) != 1) return 0;
	return 1;
}

int remote_rs232_386_32(int fd) {
	char line[128];
	if (!remote_rs232_send_command(fd,"386-32")) return 0;
	if (remote_rs232_get_response(fd,line,sizeof(line),1000000) != 1) return 0;
	return 1;
}

int remote_rs232_x64(int fd) {
	char line[128];
	if (!remote_rs232_send_command(fd,"x64")) return 0;
	if (remote_rs232_get_response(fd,line,sizeof(line),1000000) != 1) return 0;
	return 1;
}

int remote_rs232_read(int fd,unsigned long long addr,unsigned int count,unsigned char *buf) {
	char line[128];
	sprintf(line,"READB %llX %X",addr,count);
	if (!remote_rs232_send_command(fd,line)) return 0;

	struct timeval tv;
	fd_set f;
	int rd;

	FD_ZERO(&f);
	FD_SET(fd,&f);
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	if (select(fd+1,&f,NULL,NULL,&tv) != 1) {
		fprintf(stderr,"rs232_read(): select timed out\n");
		return -1;
	}
	if ((rd=read_forced(fd,line,3)) != 3) {
		fprintf(stderr,"rs232_read(): cannot read 3 bytes (got %d)\n",rd);
		return -1;
	}
	if (memcmp(line,"OK ",3) != 0)
		return remote_rs232_get_response(fd,line,sizeof(line),1000000);

	if (count > 0) {
		unsigned char *ptr = buf;
		unsigned int r = count;
		while (r > 0) {
			rd = read(fd,ptr,r);
			if (rd == 0) {
				fprintf(stderr,"rs232_read(): got zero bytes\n");
				return -1;
			}
			else if (rd < 0) continue;
//			if (rd > 0) write(2,ptr,rd);
			assert(rd <= r);
			ptr += rd;
			r -= rd;
		}
	}
	else
		rd = 0;

	if ((rd=read_forced(fd,line,2)) != 2) {
		fprintf(stderr,"rs232_read(): cannot read 2 bytes (got %d)\n",rd);
		return -1;
	}
	if (memcmp(line,"\r\n",2)) {
		line[2] = 0;
		fprintf(stderr,"rs232_read(): line did not end in \\r\\n, '%s'\n",line);
		return -1;
	}

	return 1;
}

int remote_rs232_write(int fd,unsigned long long addr,unsigned int count,const unsigned char *buf) {
	char line[128];
	sprintf(line,"WRITEB %llX %X",addr,count);
	if (!remote_rs232_send_command(fd,line)) return 0;

	const unsigned char *p = buf;
	unsigned int r = count;

	while (r > 0) {
		int wd = write(fd,p,r);
		if (wd == 0) return -1;
		assert(wd <= r);
		p += wd;
		r -= wd;
	}

	if (remote_rs232_get_response(fd,line,sizeof(line),1000000) != 1) return 0;
	return 1;
}

int remote_rs232_exec_seg_off(int fd,unsigned int segment,unsigned long offset,unsigned int timeout_s) {
	char line[128];
	sprintf(line,"EXEC %X %lX",segment,offset);
	if (!remote_rs232_send_command(fd,line)) return 0;
	if (remote_rs232_get_response(fd,line,sizeof(line),timeout_s*1000000) != 1) return 0;
	return 1;
}

int remote_rs232_exec_off(int fd,unsigned long long offset,unsigned int timeout_s) {
	char line[128];
	sprintf(line,"EXEC %llX",offset);
	if (!remote_rs232_send_command(fd,line)) return 0;
	if (remote_rs232_get_response(fd,line,sizeof(line),timeout_s*1000000) != 1) return 0;
	return 1;
}

void remote_rs232_configure(int fd) {
	struct termios t;
	if (tcgetattr(fd,&t) != 0) {
		fprintf(stderr,"Cannot get termios attributes\n");
		return;
	}

	cfmakeraw(&t);
	cfsetospeed(&t,B9600);
	cfsetispeed(&t,B9600);
	t.c_cflag = (t.c_cflag & ~CSIZE) | CS8;
	t.c_cflag &= ~(CSTOPB);
	t.c_cflag |= CREAD;

	if (tcsetattr(fd,TCSAFLUSH,&t) != 0) {
		fprintf(stderr,"Cannot set termios attributes\n");
		return;
	}
}

