#include "net_api.h"

int create_server(int port_number)
{
    int listenfd;
    int opt = 1;
    struct sockaddr_in servaddr;

    if ( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) <0) {
        perror("socket creation error : ");
        return -1;
    }

    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port_number);

    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr))<0) {
        perror("bind error : ");
        return -1;
    }
    listen(listenfd, 10);

    return listenfd;
}
