#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "index.h"
#include "net/net_api.h"
#include "common.h"
#include "bootstrap/bootstrap.h"
#include "infrastructure/inf_api.h"
#include "search.h"

#define TEST_MODE   0
#define SERVER_PORT 8000

void handle_connection(int connfd)
{
    char buffer[100];
    int n, i = 0, fd;
    char c;

    while ((n = read(connfd, &c, 1)) > 0 && c!='\n') 
	buffer[i++] = c;
    buffer[i] = '\0';        

    if ((fd = open(buffer, O_RDONLY))<0) {
	printf("file open error %s\n", buffer);
	return;
    }

    while ((n = read(fd, buffer, 100))>0) {
	write(connfd, buffer, n);
    }
    close(connfd);    
}
void *server_handler(void *t)
{
    int listenfd = create_server(8000), connfd, i, pid;

    if (listenfd < 0) {
	printf("COULDNT CREATE server\n");
	return;
    }
    for (;;) {
        if ((connfd = accept(listenfd, (struct sockaddr *)NULL, NULL))<0) {
            perror("accept error : ");
            return;
        }
        handle_connection(connfd);
        close(connfd);
    }
}
void try_getting_file(char *filename, char *ip)
{
    char request[100], buffer[100];
    int connfd = create_socket(ip, SERVER_PORT), n, fd;

    strcpy(request, filename);
    strcat(request, "\n");

    if (connfd<0) {
	printf("client socket creation error : ");
	return;
    }

    write(connfd, request, strlen(request));

    if ((fd = creat(filename, 0666))<0) {
	perror("file creation error : ");
	return;
    }
    while ((n = read(connfd, buffer, 100)) > 0) {
	write(fd, buffer, n);
    }

    close(fd);
    close(connfd);                   
}
void get_searched_file(char *filename)
{
    char buffer[100], ip_address[100];
    int fd, i = 0, n;
    char c;

    strcpy(buffer, filename);
    strcat(buffer, ".meta");

    retrieve_index(buffer);
    if ((fd = open(buffer, O_RDONLY))<0) {
	printf("file open error : %s\n", buffer);
	return;
    }
    while ((n = read(fd, &c, 1)) > 0) {
	if (c=='\n') {
	    ip_address[i] = '\0';
	    try_getting_file(filename, ip_address);
	    i = 0;
        } else {
	    ip_address[i++] = c;
	}
    }
    close(fd);
}
void *do_init(void *t)
{
    boot_me_up();
    read_write_start();
}
int main()
{
    char buffer[100], action[100];
    int n, rc, in;
    pthread_t thread_id, in_id;

    strcpy(booter_main, "152.14.93.61");
    strcpy(index_meta_file, ".local_meta_file");
    pthread_mutex_init(&search_mutex, NULL);
   
    in = pthread_create(&in_id, NULL, do_init, (void *)NULL);
    rc = pthread_create(&thread_id, NULL, server_handler, (void *)NULL);
    index_init();
    printf("Booted up and running\n");

    while (scanf("%s %s", action, buffer)!=EOF) {
        printf(".... %s %s", action, buffer);
	fflush(NULL);
        if (strcmp(action, "search")==0) {
	    pthread_mutex_lock(&search_mutex);
	    do_search(buffer);
	    pthread_mutex_unlock(&search_mutex);
	    continue;
        } 
	if (strcmp(action,"get")==0) {
	    printf("getting the file %s\n", buffer);
	    get_searched_file(buffer);
	    continue;
        }
	if (strcmp(action,"print")==0) {
	    print_route();
	    continue;
	}
	if (strcmp(action, "q")==0) {
	    fflush(NULL);
	    printf("quitting in a second\n");
	    sleep(2);
	    return 0;
	}
        printf("invalid commands %s %s\n", action, buffer);
    }	    
}
