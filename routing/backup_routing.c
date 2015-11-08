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
int routing_started = 0;
/* number of entries in the routing table */
long limit = 16, table_size = 0, entries_filled_up = 0;

/* the identity of the node immediately behind me */
long back_id;
char back_host[40];

/* Time To Live for a query... Its the max number of hops that a lookup can take */
int TTL = 20;

pthread_mutex_t table_mutex;

int back_down()
{
    int connfd = create_socket(back_host, ROUTING_PORT);

    if (connfd < 0) {
	return 1;
    }
    close(connfd);
    return 0;
}
void update_back_neighbor(long temp_id, char *host)
{
    long obj, current;
    int i;
    printf("checking to update back id \n");
    if (back_id==MYID || back_down() || \
	(temp_id > back_id && temp_id < MYID) || \
        (temp_id > back_id && temp_id > MYID && MYID < back_id) || \
	(temp_id < back_id && temp_id < MYID && MYID < back_id)) { 
         printf("replacing backid with %s\n", host);
         back_id = temp_id;
	 strcpy(back_host, host);
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

    if (readline(sock, buffer)<=0) {
	return;
    }
    if (strcmp(buffer, "INIT")==0) {
	readline(sock, buffer);
	temp_id = atol(buffer);
	readline(sock, buffer); 
	update_back_neighbor(temp_id, buffer);
        printf("received init message from %s\n", buffer);
        if (ispredecessor(temp_id)) {
            i = 0;
        } else {
            i = lookup_id(temp_id);
        }
        writelinenum(sock, table[i].id);
        writeline(sock, table[i].hostname);
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
        close(connfd);
    }
}
void initialize_routing()
{
    int connfd, i, next, j;
    char buffer[100], hostname[100], next_id[100];

    back_id = MYID;
    strcpy(back_host, MYIP);

    if (strcmp(bootstrap, MYIP)==0) {
	for (i = 0;i < limit; i++) {
	    table[i].id = MYID;
	    strcpy(table[i].hostname, MYIP);
	}
        return;
    }
    if ((connfd = create_socket(bootstrap, ROUTING_PORT))<0) {
	perror("socket creation error : ");
	return;
    }

    writeline(connfd, "INIT");
    writelinenum(connfd, MYID);
    writeline(connfd, MYIP);
 
    readline(connfd, buffer);
    table[0].id = atol(buffer);
    readline(connfd, buffer);
    strcpy(table[0].hostname, buffer);

    for (i = 1;i < limit; i++) {
        table[i].id = table[0].id;
        strcpy(table[i].hostname, table[0].hostname);
    }

    printf("got the first table entry %s\n", bootstrap);
    /* 
     * 	there is a neat tradeoff here... but I dont have the time to document
     * 	*/
    for (i = 1; i < limit; i++) {	
        next = create_socket(table[i-1].hostname, ROUTING_PORT);
        writeline(next, "ROUTE");
        writelinenum(next, MYID + (long)pow(2, i));	
	/* 
 	 * there is no way the size of the network is going to get greater than 2^TTL 
 	 * I could have actually put it around a loop till i get RESULT but 
 	 * its better to handle pathological cases and put a TTL on lookups
 	 * */
        for (j = 0; j<TTL; j++) {
             readline(next, buffer);
	     readline(next, next_id);
	     readline(next, hostname);

             close(next);
	     if ((strcmp(buffer, "RESULT")==0) || (strcmp(hostname, MYIP)==0)) {
		break;
	     }
             if ((next = create_socket(hostname, ROUTING_PORT))<0) {
                 printf("socket creation error : %s\n", buffer);
		 break;
	     }
             writeline(next, "ROUTE");
             writelinenum(next, MYID + (long)pow(2, i));
	     printf("init once more\n");	
        }
        if (strcmp(buffer, "RESULT")==0) {
            table[i].id = atol(next_id);
	    strcpy(table[i].hostname, hostname);
	    continue;
	}
       
	table[i].id = table[i-1].id;
	strcpy(table[i].hostname, table[i-1].hostname);	    
    }
    printf("Table created\n");
    print_route();
    fflush(NULL);
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
    int count = 0, turn = 0, connfd, temp_id, i, res, j;
    char buffer[40];
    pthread_t rout_handler;

    pthread_mutex_init(&table_mutex, NULL);
    pthread_create(&rout_handler, NULL, route_handler, (void *)NULL);
    initialize_routing();
    routing_started = 1;

    for (; ;) {
        sleep(10);
        for (i = 0; i<limit; i++) {
	    if (strcmp(table[i].hostname, MYIP)==0) {
		continue;
	    }
	    if ((connfd = create_socket(table[i].hostname, ROUTING_PORT))>=0) {
		break;
	    }
            printf("failed connect detail %d %s\n", i, table[i].hostname);
        }
	if (i==limit) {
	    if (strlen(back_host)>0 && (back_down()==0)) {
		strcpy(table[0].hostname, back_host);
		table[0].id = back_id;
	    } else {
		strcpy(table[0].hostname, MYIP);
		table[0].id = MYID;
	    }
	    continue;
        }
        if (i!=0) {
	    for (j = 0; j < i; j++) {
	         strcpy(table[j].hostname, table[i].hostname);
                 table[j].id = table[i].id;
		 turn = 1;
	    }
	}
	printf("stabilizing successor\n");
        /* check the successors neighbor */ 
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
            continue;
	}
	close(connfd);

	/* Check the finger tables entries */
        strcpy(buffer, table[turn].hostname);
        if ((connfd = create_socket(buffer, ROUTING_PORT))<0) {
	    if (turn>0) {
		    strcpy(table[turn].hostname, table[turn - 1].hostname);
	    }
	    turn = ((turn > 0) ? turn-1 : 0);
	    continue;
	}  
	printf("stabilizing finger entry\n"); 
        for (i = 0; i < TTL; i++) {
            writeline(connfd, "ROUTE");
            writelinenum(connfd, MYID + (long)pow(2, turn));

            readline(connfd, buffer);
            if (strcmp(buffer, "ROUTE")==0) {
                readline(connfd, buffer);
                temp_id = atol(buffer);
                readline(connfd, buffer);
		close(connfd);
		printf("once more... routing to %s\n", buffer);
                if ((connfd = create_socket(buffer, ROUTING_PORT))<0) {
	            break;
		}
		continue;
            }
            readline(connfd, buffer);
            temp_id = atol(buffer);
            readline(connfd, buffer);

            printf("turn is %ld\n", turn);

            table[turn].id = temp_id;
            strcpy(table[turn].hostname, buffer);
            close(connfd);	 	               
	    break;
        } 
        turn = ((turn==limit-1) ? 0 : turn + 1);
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
