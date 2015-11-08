#include "routing.h"
#include "../common.h"
#include "../net/net_api.h"

#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

struct route
{
    long id;
    char hostname[100];
};

struct route table[100];

/* number of entries in the routing table */
long limit = 16, table_size = 0, entries_filled_up = 0;

/* the identity of the node immediately behind me */
long back_id;
char back_host[40];

/* Time To Live for a query... Its the max number of hops that a lookup can take */
int TTL = 20;

pthread_mutex_t table_mutex;

int node_down(char *host)
{
    int connfd = create_socket(host, ROUTING_PORT);

    if (connfd < 0) {
        return 1;
    }
    writeline(connfd, "KEEPALIVE");
    close(connfd);
    return 0;
}
int back_down()
{
    int connfd = create_socket(back_host, ROUTING_PORT);

    if (connfd < 0) {
	return 1;
    }
    writeline(connfd, "KEEPALIVE");
    close(connfd);
    return 0;
}
void update_back_neighbor(long temp_id, char *host)
{
    long obj, current;
    int i;

    if (back_id==MYID || back_down() || \
	(temp_id > back_id && temp_id < MYID) || \
        (temp_id > back_id && temp_id > MYID && MYID < back_id) || \
	(temp_id < back_id && temp_id < MYID && MYID < back_id)) { 

         back_id = temp_id;
	 strcpy(back_host, host);
	 
	 if (strcmp(table[0].hostname, MYIP)==0) {
	     strcpy(table[0].hostname, host);
	     table[0].id = temp_id;
	 }
    }
}
/*
 * Chord routing lookup from a peer is done inside this function
 *
 * A separate thread would have created the routing table.
 * Here we are just using the routing table to forward the request.
 *
 * Instead of retrieving the answer and returning it, we  only provide the 
 * limited info from our routing table.
 *
 * The end node is expected to do repeated requests to find the destination node.
 *
 * I plan to change this, so that the lookup becomes a relay of TCP/UDP connections.
 * */
void handle_route_request(int sock)
{
    char buffer[100];
    long neighbor_id, obj, temp_id;
    int fd, i;

    readline(sock, buffer);
    if (strcmp(buffer, "KEEPALIVE")==0) {
	close(sock);
	return;
    }
    if (strcmp(buffer, "ID")==0) {
	writelinenum(sock, MYID);
	close(sock);
	return;
    }
    if (strcmp(buffer, "ROUTE")==0) {
        readline(sock, buffer);
        obj = atol(buffer);
        /*
         *  Corner case
         *  */
        if (table[0].id < MYID && obj < back_id) {
            writeline(sock, "ROUTE");
            writelinenum(sock, table[0].id);
            writeline(sock, table[0].hostname);
            close(sock);
            return;
        }
        /*
	 * We own up on 3 cases :
	 *
	 *    1) Object id is greater than our neighbors and the object id is less than ours
	 *    2) We are the first node in the chord ring and the object id is greater than
	 *       ours and greater than our back neighbor 
	 * */
         if ((obj > back_id && obj <= MYID) || \
            (back_id > MYID && MYID <= obj && obj > back_id) ||\
            (back_id > MYID && MYID >= obj && obj < back_id)) {
	    writeline(sock, "RESULT");
            writelinenum(sock, MYID);
            writeline(sock, MYIP);
	    close(sock);
	    return;
	}
        /*
	 * if we are the predecessor, announce the result.. if not give the route to the next hop 
	 * */
        if (ispredecessor(obj)) {
	    writeline(sock, "RESULT");
	    i = 0;
        } else {
            i = lookup_id(obj);
	    writeline(sock, "ROUTE");
        }
        writelinenum(sock, table[i].id);
        writeline(sock, table[i].hostname);
	close(sock);
	return;
    }
    if (strcmp(buffer, "STABILIZE")==0) {
	/* 
	 * 	if a new node joins the network and he is between us and our back neighbor, 
	 * 	then replace our back neighbor with the new node.
	 * 	*/
        readline(sock, buffer); 
        temp_id = atol(buffer);
	readline(sock, buffer);
        update_back_neighbor(temp_id, buffer);
	
        writelinenum(sock, back_id);
        writeline(sock, back_host);
	close(sock);
        return;
    }
}
void *route_handler(void *t)
{
    int listenfd, connfd;

    /* TODO: convert from using TCP to UDP */
    if ((listenfd = create_server(ROUTING_PORT))<0) {
	perror("could not create server : ");
        return NULL;
    }
    for (;;) {
	if ((connfd = accept(listenfd, (struct sockaddr *)NULL, NULL))<0) {
            perror("accept error : ");
            return NULL;
        }
        handle_route_request(connfd);
    }
}
long name_to_id(char *host)
{
    int connfd;
    char id[40];

    if (strcmp(host, MYIP)==0) {
	return MYID;
    }
    if ((connfd = create_socket(host, ROUTING_PORT))<0) {
	return 0;
    }
    writeline(connfd, "ID");
    readline(connfd, id);
    close(connfd);
    return atol(id);
}
struct route *lookup(long obj, char *who)
{
    char hostname[100], next[10], id[20];
    int i, connfd;
    struct route *ret;

    strcpy(hostname, who);
    for (i = 0; i < TTL; i++) {
	if ((connfd = create_socket(hostname, ROUTING_PORT))<0) {
	    return NULL;
	}

        writeline(connfd, "ROUTE");
	writelinenum(connfd, obj);
	
	readline(connfd, next);
	readline(connfd, id);
	readline(connfd, hostname);

	if (strcmp(next, "RESULT")==0) {
	    close(connfd);
	    break;
	}
	close(connfd);
    }
    if (i==TTL) {
	return NULL;
    }

    ret = (struct route *)malloc(sizeof(struct route));
    ret->id = (long)atol(id);
    strcpy(ret->hostname, hostname);

    return ret;
}
void stabilize_sucessor()
{
    char buffer[40];
    int i, connfd = -1;
    long temp_id;

    for (i = 0; i<limit; i++) {
	if (strcmp(table[i].hostname, MYIP)==0) {
	    continue;
        }
        if ((connfd = create_socket(table[i].hostname, ROUTING_PORT))>=0) {
	    break;
        }
    }
    if (i==limit) {
	return;
    }
    if (i!=0) {
	table[0].id = table[i].id;
	strcpy(table[0].hostname, table[i].hostname);
    }

    writeline(connfd, "STABILIZE");
    writelinenum(connfd, MYID);
    writeline(connfd, MYIP);

    readline(connfd, buffer);
    temp_id = atol(buffer);
    readline(connfd, buffer);

    if (temp_id!=MYID) {
	table[0].id = temp_id;
        strcpy(table[0].hostname, buffer);
	close(connfd);
        stabilize_sucessor();
	return;
    }
    close(connfd);
    return;
}
int stabilize_finger(long obj, int i)
{
    int connfd;
    struct route *r = lookup(obj, table[i-1].hostname);

    if (r==NULL) {
	if (node_down(table[i-1].hostname)) {
	    return -1;
        }
	strcpy(table[i].hostname, table[i-1].hostname);
	return 0;
    }

    table[i].id = r->id;
    strcpy(table[i].hostname, r->hostname);
    free(r);

    return 1;
}
void initialize_routing()
{
    int i, connfd;
    char hostname[100];
    struct route *r;

    back_id = MYID;
    strcpy(back_host, MYIP);
    printf("bootstrap is %s\n", bootstrap);

    strcpy(table[0].hostname, bootstrap);
    table[0].id = name_to_id(bootstrap);

    strcpy(hostname, bootstrap);

    for (i = 1; i < limit; i++) {
	if ((strcmp(hostname, MYIP)==0) || \
           ((r = lookup(MYID + (long)pow(2,i), hostname))==NULL)) {

	    strcpy(table[i].hostname, MYIP);
	    table[i].id = MYID;
            continue;
	}
	table[i].id = r->id;
	strcpy(table[i].hostname, r->hostname);
	strcpy(hostname, r->hostname);
	free(r);
    }
    stabilize_sucessor();
    printf("table created\n");	
}
/*
 * We periodically apply a stabilizing algorithm to maintain O(lgn) routing.
 *
 * The stability code used here is quite aggressive. This is because, node departures
 * happen frequently. There is a tradeoff between the overhead of routing and routing
 * performance. I think its better to sacrifice routing overhead to gain in routing performance.
 *
 * Another important factor is that coding becomes easier if we have correct tables.
 * In that case, the routing lookup can assume that the table is correct.
 *
 * Peer to Peer networks are very dynamic and it might even happen that the number of entries in the routing
 * table is insufficient. To make the whole thing work and be self organizing, we need the stabilizing
 * layer to work correctly.
 * */ 
void *routing_protocol(void *t)
{
    int turn = 1, num = 0, i = 0, val;
    pthread_t rout_handler;

    pthread_mutex_init(&table_mutex, NULL);

    printf("before init started\n");
    initialize_routing();
    printf("routing started\n");
    pthread_create(&rout_handler, NULL, route_handler, (void *)NULL);
    routing_started = 1;

    for (; ;) {
        sleep(10);
        stabilize_sucessor();
	if ((val = stabilize_finger(MYID + (long)pow(2, turn), turn))<0) {
            turn = (turn > 0) ? turn - 1 : 1;
	    continue;
	} else if (val==0) {
	    continue;
	}
	printf("turn is %d\n", turn);
        turn = ((turn==limit-1) ? 1 : turn + 1);
    }     
    pthread_mutex_destroy(&table_mutex);
}
char *who_has_id(long obj)
{
    char *next_hop = (char *)malloc(40);
    char result[40], next_id[40];
    int i, sockfd;

    while (routing_started==0) {
	sleep(1);
    }
    if (ispredecessor(obj)) {
	strcpy(next_hop, table[0].hostname);
	return next_hop;
    }
    strcpy(next_hop, table[lookup_id(obj)].hostname);

    if (strcmp(next_hop, MYIP)==0) {
	return next_hop;
    }
    for (i = 0; i<TTL;i++) {
        if ((sockfd = create_socket(next_hop, ROUTING_PORT))<0) {
	    strcpy(next_hop, table[lookup_id(obj/2)].hostname);
	    continue;
	}
	writeline(sockfd, "ROUTE");
        writelinenum(sockfd, obj);

        readline(sockfd, result);
	readline(sockfd, next_id);
	readline(sockfd, next_hop);

        close(sockfd);
	if (strcmp(result, "RESULT")==0) {
	   break;
        }
    }  
      
    return next_hop;	
}
/*
 *
 * Given a filename, return the ip address of the node having it
 * */
char *who_has(char *filename)
{
    long obj = generate_id(filename);

    return who_has_id(obj);
}
int ispredecessor(long obj)
{
    /* check if we are the predecessor node and our successor is the one */
    pthread_mutex_lock(&table_mutex);
    if ((table[0].id > obj && MYID < obj && MYID < table[0].id)) {
       pthread_mutex_unlock(&table_mutex);
       return 1;
    }
    pthread_mutex_unlock(&table_mutex);
    return 0;
}
/*
 * The core part..
 *
 * Chord lookup is done inside this function
 * */
int lookup_id(long obj)
{
    int i;

    pthread_mutex_lock(&table_mutex);
    /* 
     * predecessor node lookup 
     *
     * Goes through the finger table and returns the next hop.
     *
     * */
    for (i = limit - 1; i >= 0; i--) {
        /* the common case */
        if (table[i].id < obj && table[i].id > MYID) {
            break;
        }
        /* the obj is in the ring after the border  and we are near the end of the ring */
        if (table[i].id > obj && table[i].id > MYID && obj < MYID) {
            break;
        }
        /* the object is behind us and the finger entry is behind the object */ 
        if (table[i].id < obj && table[i].id < MYID && obj < MYID) {
            break;
        }
    }
    pthread_mutex_unlock(&table_mutex);
    if (i==-1) {
	return 0;
    }
    return i;
}
void print_route(void)
{
    int i;

    printf("\nBack node is %ld %s\n", back_id, back_host);
    for (i = 0; i<limit; i++) {
	printf("%ld %s\n", table[i].id, table[i].hostname);
    }
}
