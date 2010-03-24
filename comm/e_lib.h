
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

struct remote_eth_connection {
	int			sock_fd;
	struct sockaddr_in	ip_addr;
	socklen_t		ip_sz;
	char			machtype[32];
};

struct remote_eth_connection *remote_eth_socket(const char *addr);
void remote_eth_close(struct remote_eth_connection *c);
int remote_eth_wait_for_data(struct remote_eth_connection *c,unsigned int to);
int remote_eth_test(struct remote_eth_connection *c);
int remote_eth_write_block(struct remote_eth_connection *c,unsigned long long addr,unsigned char *buf,int len);
int remote_eth_read_block(struct remote_eth_connection *c,unsigned long long addr,unsigned char *buf,int len);
int remote_eth_read(struct remote_eth_connection *c,unsigned long long addr,unsigned char *buf,int len);
int remote_eth_write(struct remote_eth_connection *c,unsigned long long addr,unsigned char *buf,int len);
int remote_eth_exec(struct remote_eth_connection *c,unsigned long long addr,unsigned int timeout_s);

