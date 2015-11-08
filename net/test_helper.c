#include "net_api.h"

int create_socket(char *ip_address, int port_number)
{
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) <0) {
        perror("socket creation error : ");
        return -1;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port_number);
 
    bzero(&cliaddr, sizeof(servaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(port_number + 1);

    if (inet_pton(AF_INET, ip_address, &servaddr.sin_addr)<=0) {
        perror("inet_pton error : ");
        printf("%s %d\n", ip_address, port_number);
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *)&cliaddr, sizeof(servaddr))<0) {
        perror("bind error : ");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))<0) {
        perror("connect error : ");
        printf("%s %d\n", ip_address, port_number);
        return -1;
    }

    return sockfd;
}

