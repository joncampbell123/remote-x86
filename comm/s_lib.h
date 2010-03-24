
int remote_rs232_send_command(int fd,const char *cmd);
int remote_rs232_get_response(int fd,char *buf,size_t len,unsigned long timeout);
int remote_rs232_test(int fd);
int remote_rs232_8086(int fd);
int remote_rs232_286(int fd);
int remote_rs232_386_16(int fd);
int remote_rs232_386_32(int fd);
int remote_rs232_x64(int fd);
int remote_rs232_read(int fd,unsigned long long addr,unsigned int count,unsigned char *buf);
int remote_rs232_write(int fd,unsigned long long addr,unsigned int count,const unsigned char *buf);
int remote_rs232_exec_seg_off(int fd,unsigned int segment,unsigned long offset,unsigned int timeout_s);
int remote_rs232_exec_off(int fd,unsigned long long offset,unsigned int timeout_s);
void remote_rs232_configure(int fd);

