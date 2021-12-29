#ifndef __MESSAGE_H_
#define __MESSAGE_H_

#define MAX_REPLY_SIZE     1024
#define MAX_REQUEST_SIZE   1024

void read_str(int fd, char* str, int max_size);
int write_msg(int fd, char* buf, int size);

#endif /*__MESSAGE_H_*/