#include <stdio.h>

#include "test_helper.c"

int main()
{
    int sockfd = create_socket("127.0.0.1", 80);

    close(sockfd);
}
