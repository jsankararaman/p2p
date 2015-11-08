#include "p2pd.h"
#include "../routing/routing.h"
#include "../common.h"
#include "inf_api.h"

#include <stdio.h>

#define LOW 0
#define HIGH 1

long gen_id[10];

void generate_store_ids(char *name)
{
    int i;
    long base_id = generate_id(name);

    for (i = 0; i < 10; i++) {
	gen_id[i] = ((base_id/10)*i)%TTWOBIT;
    }
}
void store_file(char *name, int availability)
{
    int i, replica_limit = 0;
    char *ip;

    generate_store_ids(name);
    replica_limit = ((availability==HIGH) ? 10 : 5);
    
    while (routing_started==0) sleep(1);

    for (i = 0; i < replica_limit; i++) {
	ip = who_has_id(gen_id[i]);
        /* to be done.. this has to be fault tolerant which at the moment it is not */
	if ((strcmp(ip, MYIP)==0) || (ip==NULL)) {
	    free(ip);
	    continue;
	}
	printf("storing file %s in %s\n", name, ip); 
        write_file_to_peer_api(name, ip, 0, replica_limit/10);
	free(ip);
    } 		
}
void get_file(char *name, int availability)
{
    int i, replica_limit = 0;
    char *ip;

    generate_store_ids(name);
    replica_limit = ((availability==HIGH) ? 10 : 5);

    while (routing_started==0) sleep(1);

    for (i = 0; i < replica_limit; i++) {
	ip = who_has_id(gen_id[i]);
	if ((strcmp(ip, MYIP)==0) || (ip==NULL)) {
	    free(ip);
	    continue;
	}
        /* to be done.. this has to be fault tolerant which at the moment it is not */
        if (read_file_from_peer_api(name, ip)>0) {
	    printf("got the file from peer\n");
	    free(ip);
	    break;
        }
	printf("couldnt get the file from %s\n", ip);
	free(ip);
    } 		
}
int isavailable(char *name)
{
    int i;
    char *ip;

    generate_store_ids(name); 

    while (routing_started==0) sleep(1);

    for (i = 0; i<10; i++) {
        ip = who_has_id(gen_id[i]);
        if (strcmp(ip, MYIP)==0) {
	    free(ip);
	    continue;
	}
        if (!replica_exists(ip, name)) {
	    free(ip);
	    return 0;
        }  
        free(ip);
    }	    
    return 1;
}
void retrieve_index(char *word)
{    get_file(word, HIGH);   }
void store_index(char *word)
{    store_file(word, HIGH); }
