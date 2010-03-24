
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

struct remote_eth_connection *remote_eth_socket(const char *addr) {
	struct remote_eth_connection *c = (struct remote_eth_connection*)malloc(sizeof(struct remote_eth_connection));
	if (c == NULL) return NULL;

	if ((c->sock_fd = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
		free(c);
		return NULL;
	}

	memset(&c->ip_addr,0,sizeof(c->ip_addr));
	c->ip_addr.sin_family = AF_INET;
	c->ip_addr.sin_port = htons(777);
	c->ip_addr.sin_addr.s_addr = inet_addr(addr);
	c->machtype[0] = 0;

	return c;
}

void remote_eth_close(struct remote_eth_connection *c) {
	close(c->sock_fd);
	free(c);
}

int remote_eth_wait_for_data(struct remote_eth_connection *c,unsigned int to) {
	struct timeval tv;
	fd_set f;

	FD_ZERO(&f);
	FD_SET(c->sock_fd,&f);
	tv.tv_sec = to / 1000000;
	tv.tv_usec = to % 1000000;
	if (select(c->sock_fd+1,&f,NULL,NULL,&tv) == 1)
		return 1;

	return 0;
}

int remote_eth_test(struct remote_eth_connection *c) {
	int rd;
	char recv_buf[64];
	const char *cmd = "TEST";
	size_t cmd_len = strlen(cmd);
	unsigned int retry = 3;

	do {
		c->ip_sz = sizeof(c->ip_addr);
		fprintf(stderr," > TEST\n");
		if (sendto(c->sock_fd,cmd,cmd_len,0,(struct sockaddr*)(&(c->ip_addr)),c->ip_sz) < cmd_len)
			return 0;
		if (remote_eth_wait_for_data(c,1250000)) {
			if ((rd=recvfrom(c->sock_fd,recv_buf,sizeof(recv_buf)-1,0,(struct sockaddr*)(&(c->ip_addr)),&(c->ip_sz))) <= 0)
				return 0;

			recv_buf[rd] = 0;
			fprintf(stderr," < %s\n",recv_buf);
			if (!strncmp(recv_buf,"OK ",3))
				return 1;

			strncpy(c->machtype,recv_buf+3,sizeof(c->machtype)-1);
		}
	} while (--retry != 0);

	return 0;
}

int remote_eth_write_block(struct remote_eth_connection *c,unsigned long long addr,unsigned char *buf,int len) {
	int rd;
	size_t cmd_len;
	char recv_buf[2200];
	unsigned int retry = 3;
	static unsigned int seq = 0;

	if (len < 0 || len > 1400)
		return 0;

	if (seq == 0)
		seq = (unsigned int)time(NULL);
	else
		seq++;

	do {
		c->ip_sz = sizeof(c->ip_addr);
		fprintf(stderr," > WRITE %llX %X\n",addr,len);

		int txt_len = sprintf(recv_buf,"WRITE %llX %X\n",addr,seq);
		unsigned char *data = (unsigned char*)recv_buf + txt_len;
		memcpy(data,buf,len);

		cmd_len = (size_t)txt_len + (size_t)len;

		if (sendto(c->sock_fd,recv_buf,cmd_len,0,(struct sockaddr*)(&(c->ip_addr)),c->ip_sz) < cmd_len)
			return 0;
		if (remote_eth_wait_for_data(c,1250000)) {
			if ((rd=recvfrom(c->sock_fd,recv_buf,sizeof(recv_buf)-1,0,(struct sockaddr*)(&(c->ip_addr)),&(c->ip_sz))) <= 0)
				return 0;

			recv_buf[rd] = 0;
			char *p = recv_buf+3;
			fprintf(stderr," < %s\n",recv_buf);
			if (strncmp(recv_buf,"OK ",3))
				return 0;
			if (strtoull(p,&p,16) != addr)
				return 0;
			while (*p == ' ') p++;
			if (strtoul(p,&p,16) != seq)
				return 0;

			return 1;
		}
	} while (--retry != 0);

	return 0;
}

int remote_eth_read_block(struct remote_eth_connection *c,unsigned long long addr,unsigned char *buf,int len) {
	int rd;
	size_t cmd_len;
	char recv_buf[2000];
	unsigned int retry = 3;

	if (len < 0 || len > 1400)
		return 0;

	do {
		c->ip_sz = sizeof(c->ip_addr);
		fprintf(stderr," > READ %llX %X\n",addr,len);

		sprintf(recv_buf,"READ %llX %X",addr,len);
		cmd_len = strlen(recv_buf);

		if (sendto(c->sock_fd,recv_buf,cmd_len,0,(struct sockaddr*)(&(c->ip_addr)),c->ip_sz) < cmd_len)
			return 0;
		if (remote_eth_wait_for_data(c,1250000)) {
			if ((rd=recvfrom(c->sock_fd,recv_buf,sizeof(recv_buf)-1,0,(struct sockaddr*)(&(c->ip_addr)),&(c->ip_sz))) <= 0)
				return 0;

			recv_buf[rd] = 0;
			unsigned char *data = strchr((char*)recv_buf,'\n');
			if (data == NULL) continue;
			*data++ = 0;

			if ((char*)(data+len) > (recv_buf+rd))
				return 0;

			fprintf(stderr," < %s\n",recv_buf);
			if (strncmp(recv_buf,"OK ",3))
				return 0;
			if (strtoull(recv_buf+3,NULL,16) != addr)
				return 0;

			memcpy(buf,data,len);
			return 1;
		}
	} while (--retry != 0);

	return 0;
}

int remote_eth_read(struct remote_eth_connection *c,unsigned long long addr,unsigned char *buf,int len) {
	while (len >= 1400) {
		const int sz = 1400;
		if (!remote_eth_read_block(c,addr,buf,sz)) return 0;
		addr += sz;
		buf += sz;
		len -= sz;
	}
	if (len > 0) {
		if (!remote_eth_read_block(c,addr,buf,len)) return 0;
		addr += len;
		buf -= len;
		len -= len;
	}

	return 1;
}

int remote_eth_write(struct remote_eth_connection *c,unsigned long long addr,unsigned char *buf,int len) {
	while (len >= 1400) {
		const int sz = 1400;
		if (!remote_eth_write_block(c,addr,buf,sz)) return 0;
		addr += sz;
		buf += sz;
		len -= sz;
	}
	if (len > 0) {
		if (!remote_eth_write_block(c,addr,buf,len)) return 0;
		addr += len;
		buf -= len;
		len -= len;
	}

	return 1;
}

int remote_eth_exec(struct remote_eth_connection *c,unsigned long long addr,unsigned int timeout_s) {
	int rd;
	size_t cmd_len;
	char recv_buf[64];
	unsigned int retry = 3;
	unsigned int seq = 0;

	if (seq == 0)
		seq = (unsigned int)time(NULL);
	else
		seq++;

	do {
		c->ip_sz = sizeof(c->ip_addr);
		fprintf(stderr," > EXEC %llX\n",addr);

		cmd_len = sprintf(recv_buf,"EXEC %llX %X",addr,seq);
		if (sendto(c->sock_fd,recv_buf,cmd_len,0,(struct sockaddr*)(&(c->ip_addr)),c->ip_sz) < cmd_len)
			return 0;
		if (remote_eth_wait_for_data(c,1250000)) {
			if ((rd=recvfrom(c->sock_fd,recv_buf,sizeof(recv_buf)-1,0,(struct sockaddr*)(&(c->ip_addr)),&(c->ip_sz))) <= 0)
				return 0;

			recv_buf[rd] = 0;
			fprintf(stderr," < %s\n",recv_buf);
			if (!strncmp(recv_buf,"OK ",3))
				return 1;
		}
	} while (--retry != 0);

	return 0;
}

