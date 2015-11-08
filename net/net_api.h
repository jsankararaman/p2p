#ifndef NET_API_H

#define NET_API_H

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<strings.h>

int create_server(int);
int create_socket(char *, int);

#endif
