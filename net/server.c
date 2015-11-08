#include <stdio.h>

#include "server_helper.c"

int main()
{
    int listenfd = create_server(80), connfd;
    struct sockaddr_in cli;

    int len = sizeof(struct sockaddr);
    char buffer[100];

    for (;;) {
	connfd = accept(listenfd, (struct sockaddr *)&cli, &len);
	inet_ntop(AF_INET, &cli.sin_addr, buffer, 100);
        printf("connection from %s   %d\n", buffer, ntohs(cli.sin_port));
    }
}
