#include "server_helper.c"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

enum error_codes {INVALID_SYNTAX_ERROR, FILE_ACCESS_DENIED, FILE_NOT_FOUND};
char error_strings[10][100] = {"HTTP/1.0 400 File not found : ", "HTTP/1.0 Access Denied", "HTTP/1.0 File Not Found"};

enum request_type {GET = 1, POST = 2};
struct request
{
    int request_t;
    char req[100];

    int connfd;
};

#define handle_error(error_num, connfd)    write(connfd, error_strings[error_num], strlen(error_strings[error_num]));
int validate_request(char *line, int connfd)
{
    int num = strlen(line), fd, n;
    char c;
    char request_line[100], protocol[10];

    if (num<5) {
	handle_error(INVALID_SYNTAX_ERROR, connfd);
	return;
    }
    strncpy(request_line, line, 4);
    request_line[4] = '\0';
    printf("%s\n", line);
    if (strcmp(request_line, "POST")==0) {
        return POST;    	
    } else if (strcmp(request_line, "GET ")==0 && strlen(line) > 8) {
        strncpy(protocol, line + strlen(line) - 8, 8);
	protocol[8] = '\0';
	printf("%s\n", protocol);
        if (strcmp(protocol, "HTTP/1.0")==0) {
		strncpy(request_line, line + 4, strlen(line) - 13);
		request_line[strlen(line)-13] = '\0';	
                strcpy(line, request_line);
		return GET;
	}
	printf("not any use\n");
    } 

    handle_error(INVALID_SYNTAX_ERROR, connfd);
    return 0;
}
int getline(char *line, int connfd)
{
    int i = 0, n;
    char c;

    while (i<100 && (n = read(connfd, &c, 1))>0 && c!='\n' && c!='\r') {
	line[i++] = c;
    }
    line[i] = '\0';
    return i;
}
void *handle_request(void *arg)
{
    struct request *r = (struct request *)arg;
    int connfd = r->connfd, fd, n, len = 0;
    char buffer[1000], filename[100], resp[1000];
    
    strcpy(filename, "./");
    if(strcmp(r->req, "/")==0) {
        strcat(filename, "index.html");
    } else {
	strcat(filename, r->req);
    }
    if ((fd = open(filename, O_RDONLY))<0) {
        perror("file open error");
        printf("%s\n", filename);
        return;
    }
    printf("serving the file\n");
    while ((n = read(fd, buffer, 1000))>0) {
        write(connfd, buffer, n);
    }
    free(r);
    
    close(fd);
    close(connfd);
}
int main()
{
    int listenfd = create_server(2000), connfd, i, pid, ret;
    char line[1000];
    pthread_t tid;
    struct request *r; 

    for (; ;) {
        if ((connfd = accept(listenfd, (struct sockaddr *)NULL, NULL))<0) {
            perror("accept error : ");
            return -1;
        }
        getline(line, connfd);
        if ((ret = validate_request(line, connfd))>0) {
	    r = (struct request *)malloc(sizeof(struct request));
            r->request_t = ret;
            strcpy(r->req, line);
	    r->connfd = connfd;
	    printf("creating thread\n");
            ret = pthread_create(&tid, NULL, handle_request, (void *)r);
	}
        else {
	    close(connfd);
	}
    }
    close(listenfd);
    return 0;
}
