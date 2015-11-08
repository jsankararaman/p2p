#include "../net/net_api.h"
#include "../routing/routing.h"
#include "p2pd.h"
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#define READ_WRITE_PORT 2000

struct store_list
{
    char filename[100];
    int type;
    struct store_list *next;
};

pthread_mutex_t list_mutex;

static struct store_list *head = NULL, *iter = NULL;
static int HIGH = 1, LOW = 0;

void store_list_insert(char *filename, int avail)
{
    if (head==NULL) {
	head = (struct store_list *)malloc(sizeof(struct store_list));
        iter = head;
    } else {
	iter->next = (struct store_list *)malloc(sizeof(struct store_list));
        iter = iter->next;
    }
    strcpy(iter->filename, filename);
    iter->type = avail;
    iter->next = NULL;
}
void check_list()
{
    struct store_list *temp;

    for (temp = head; temp!=NULL; temp = temp->next) {
	printf("checking if %s is available\n", temp->filename);
	if (temp->type==HIGH && !isavailable(temp->filename)) {
            printf("replicating %s to maintain availability", temp->filename);
	    store_index(temp->filename);
	}
    }
}
void *append_file_for_remote_peer(void *t)
{
    int connfd = *(int *)t, fd, c;
    char filename[100], temp[10];

    readline(connfd, filename);
    readline(connfd, temp);

    if ((fd = open(filename, O_APPEND))<0) {
	fd = creat(filename, 0666);
    }

    while(read(connfd, &c, 1)>0) {
	write(fd, &c, 1);
    }    
    close(connfd);
    close(fd); 
}
/*
 * Services a read file request from a remote peer
 *
 * */
void *read_file_for_remote_peer(void *t)
{
    int connfd = *(int *)t, fd, c;
    char buffer[100];
    
    readline(connfd, buffer);
    if ((fd = open(buffer, O_RDONLY))<0) {
        printf("could not read %s*\n", buffer);
	writeline(connfd, "NOT_FOUND");
	close(connfd);
	return NULL;
    }
    writeline(connfd, "FOUND");

    while(read(fd, &c, 1)>0) {
	write(connfd, &c, 1);
    }    
    close(connfd);
    close(fd); 
}
/*
 * Writes a file locally from a remote peer
 * */
void *write_file_for_remote_peer(void *t)
{
    int connfd = *(int *)t;
    int fd, c;
    char filename[100], avail[10];

    readline(connfd, filename);
    readline(connfd, avail);

    fd = creat(filename, 0666);

    while(read(connfd, &c, 1)>0) {
	write(fd, &c, 1);
    }    
    close(connfd);
    close(fd); 

    pthread_mutex_lock(&list_mutex);
    if (strcmp(avail, "HIGH")==0) {
        store_list_insert(filename, HIGH);
    } else {
	store_list_insert(filename, LOW);
    }	
    pthread_mutex_unlock(&list_mutex);
}
/*
 * API used by our code to place replicas on to the peer to peer network
 * */
int write_file_to_peer_api(char *filename, char *ip, int append, int avail)
{
    int fd, connfd, c;
    char buffer[100];
    
    if ((connfd = create_socket(ip, READ_WRITE_PORT))<0) {
	return -1;
    }
    if (append==0) {
        writeline(connfd, "WRITE");
    } else {
	writeline(connfd, "APPEND");
    }
    writeline(connfd, filename);
    if (avail==HIGH) {
	writeline(connfd, "HIGH");
    } else {
	writeline(connfd, "LOW");
    }
    if ((fd = open(filename, O_RDONLY))<0) {
        printf("file read failed\n");
	close(connfd);
	return -1;
    }

    while(read(fd, &c, 1)>0) {
	write(connfd, &c, 1);
    }    
    close(connfd);
    close(fd);

    return 0;
}
/*
 * Reads a file/replica from a remote peer
 *
 * All the intelligence is from the calling function.
 *
 * This function just contacts the peer and gets the file
 * */
int read_file_from_peer_api(char *filename, char *ip)
{
    int connfd = create_socket(ip, READ_WRITE_PORT);
    char result[10];
    int fd, c;

    if (connfd<0) {
        printf("connection failed\n");
	return -1;
    }
    printf("request sent\n");
    writeline(connfd, "READ");
    writeline(connfd, filename);
    readline(connfd, result);

    if (strcmp(result,"NOT_FOUND")==0) {
        printf("file not found\n");
	return -1;
    }
    if ((fd = creat(filename, 0666))<0) {
        printf("couldnt create local file\n");
	return -1;
    }
    while (read(connfd, &c, 1)>0) {
	write(fd, &c, 1);
    }
    close(connfd);
    close(fd);   
    return 1;	
}
void check_and_return(int connfd)
{
    char buffer[100];
    int fd;    

    readline(connfd, buffer);
    if ((fd = open(buffer, O_RDONLY))<0) {
	writeline(connfd, "NO");
    }
    close(fd);
    writeline(connfd, "YES");
}
int replica_exists(char *ip, char *replica_name)
{
    int connfd = create_socket(ip, READ_WRITE_PORT);
    char result[10];

    if (connfd<0) {
	return 0;
    }
    writeline(connfd, "CHECK");
    writeline(connfd, replica_name);
    readline(connfd, result);
    close(connfd);

    return (strcmp(result, "YES")==0);
}
void *read_write_handler(void *t)
{
    int listenfd = create_server(READ_WRITE_PORT), connfd, i;
    char buffer[10];
    pthread_t rid[1000], wid[1000], aid[1000];
    short riter = 0, witer = 0, aiter = 0;

    for (;;) {
        if ((connfd = accept(listenfd, (struct sockaddr *)NULL, NULL))<0) {
            perror("accept error : ");
            continue;
        }
        readline(connfd, buffer);
	if (strcmp(buffer, "READ")==0) {
            pthread_create(&rid[riter], NULL, read_file_for_remote_peer, (void *)&connfd);
	    riter = (riter + 1)%1000;
	    continue;
	}
	if (strcmp(buffer, "WRITE")==0) {
            pthread_create(&wid[witer], NULL, write_file_for_remote_peer, (void *)&connfd);
	    witer = (witer + 1)%1000;
            continue;
	}
        if (strcmp(buffer, "APPEND")==0) {
	    pthread_create(&aid[aiter], NULL, append_file_for_remote_peer, (void *)&connfd);
	    aiter = (aiter + 1)%1000;
	    continue;
	}
	if (strcmp(buffer, "CHECK")==0) {
	    check_and_return(connfd);
	    close(connfd);
        }
    }
}
void *monitor(void *t)
{
    DIR *dp;
    struct dirent *dirp;

    sleep(3);
    if ((dp = opendir("."))==NULL) {
        perror("cant open directory :");
        return;
    }

    while ((dirp = readdir(dp))!=NULL) {
	if ((strstr(dirp->d_name, ".index")!=NULL) || \
	    (strstr(dirp->d_name, ".meta")!=NULL)) {
            pthread_mutex_lock(&list_mutex);
            store_list_insert(dirp->d_name, HIGH);
            pthread_mutex_unlock(&list_mutex);
	}
    }
    for (;;) {
	pthread_mutex_lock(&list_mutex);
	printf("checking list\n");
        check_list();
	pthread_mutex_unlock(&list_mutex);
	sleep(20);
    }	
}
void *read_write_start(void *t)
{
    pthread_t route, rwid, mon;

    pthread_mutex_init(&list_mutex, NULL);
    pthread_create(&route, NULL, routing_protocol, NULL);
    pthread_create(&mon, NULL, monitor, NULL);
    read_write_handler(NULL);
    return;
}
