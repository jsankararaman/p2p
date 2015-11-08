#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "../net/net_api.h"
#include "../common.h"
#include "bootstrap.h"
#include "../routing/routing.h"

#define NUM_LIST 1000
#define BOOTSTRAP_PORT 2004             

int count = 0, iter = 0;
char nodes[100][10];

struct bootstrap_message *list[NUM_LIST];

struct bootstrap_message *read_bootstrap_message(int);

void boot_me_up()
{
    struct bootstrap_message *temp;
    int connfd, tempfd = -1, i;
    char buffer[100];

    if ((connfd = create_socket(booter_main, BOOTSTRAP_PORT))<0) {
        perror("socket creation error in bootstrap : ");
	return;
    }	   
    while((temp = read_bootstrap_message(connfd))!=NULL) {
	if (strcmp(temp->your_neighbor, temp->your_ip)==0) {
	    break;
	}
	if ((tempfd = create_socket(temp->your_neighbor, ROUTING_PORT))<0) {
            perror("socket creation error in bootstrap : ");
	    free(temp);
            continue;
	}
        writeline(tempfd, "KEEPALIVE");
	break;
    }
    if (temp!=NULL) {
        strcpy(MYIP, temp->your_ip);
        strcpy(bootstrap, temp->your_neighbor);
	MYID = temp->your_id;
	printf("MYID is %ld\n", MYID);
        free(temp);
    }

    while ((temp = read_bootstrap_message(connfd))!=NULL) {
	free(temp);
    }	
    printf("booted up in bootstrap.c \n");
    fflush(NULL);
    close(tempfd);
    close(connfd);
}
void write_bootstrap_message(int fd, struct bootstrap_message *m)
{
    writeline(fd, m->your_ip);
    writeline(fd, m->your_neighbor);
    printf("sending id %ld\n", m->your_id);
    writelinenum(fd, m->your_id);
}
struct bootstrap_message *read_bootstrap_message(int fd)
{
    struct bootstrap_message *temp = (struct bootstrap_message *)malloc(sizeof(struct bootstrap_message));
    char buffer[100];

    if (readline(fd, buffer)==0) {
	free(temp);
	return NULL;
    }
    strcpy(temp->your_ip, buffer);
    readline(fd, buffer);
    strcpy(temp->your_neighbor, buffer);
    readline(fd, buffer);
    temp->your_id = atol(buffer);   
    return temp;
}
void handle_bootstrap_requests()
{
    int listenfd = create_server(BOOTSTRAP_PORT), connfd, temp_iter;
    struct sockaddr_in cliaddr;
    socklen_t len;
    char buff[100];

    if (listenfd<0) {
	perror("bootstrap creation error ");
	return;
    }

    for (;;) {
        if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &len))<0) {
             perror("socket accept error : ");
	     continue;
        }      

        list[iter] = (struct bootstrap_message *)malloc(sizeof(struct bootstrap_message));
        strcpy(list[iter]->your_ip, inet_ntop(AF_INET, &cliaddr.sin_addr, buff, sizeof(buff)));        	             
	list[iter]->your_id = generate_id(list[iter]->your_ip);
	 

        for (temp_iter = iter - 1; count>0 && temp_iter >= 0 && temp_iter > iter-4; temp_iter--) {
	    strcpy(list[iter]->your_neighbor, list[temp_iter]->your_ip);
	    write_bootstrap_message(connfd, list[iter]);
            printf("sent %s neighbor %s\n", list[iter]->your_ip, list[iter]->your_neighbor);
	}
        if (iter==0) {
	    strcpy(list[iter]->your_neighbor, list[iter]->your_ip);
	    write_bootstrap_message(connfd, list[iter]);
	}
	close(connfd);
        count++;
        iter = (iter + 1) % NUM_LIST;
        if (iter==0) {
	    free(list[iter]);
        }
    }
}
