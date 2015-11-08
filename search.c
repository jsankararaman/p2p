#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "infrastructure/p2pd.h"
#include "search.h"

void cache_results()
{
    /* To be done */
}
int hash(char *name)
{
    int sum = 0, i;

    for (i = 0; name[i]!='\0'; i++) {
	if (name[i]>='a' && name[i]<='z') 
	        sum = (sum + name[i] - 'a') % MAX_ENTRIES_PER_TABLE;
	else if (name[i]>='A' && name[i]<='Z') 
		sum = (sum + name[i] - 'A') % MAX_ENTRIES_PER_TABLE;
	
    }
    
    return sum % MAX_ENTRIES_PER_TABLE;
}
void merge_insert(char *filename, int num) 
{
    struct merged_hash *it;

    for (it = head[hash(filename)];it!=NULL;it = it->next) {
	if (strcmp(it->filename, filename)==0) {
	    it->num_occur += num;
	    it->weight++;
	    return;
        }
    }

    it = (struct merged_hash *)malloc(sizeof(struct merged_hash));
    it->filename = (char *)malloc(strlen(filename));
    strcpy(it->filename, filename);
    it->num_occur = num;
    it->weight = 1;

    it->next  = head[hash(filename)];
    head[hash(filename)] = it;    
}
void merge_and_print_results()
{
    int i, max_rank = 0, count = 0;
    struct search_results *from;
    struct merged_hash *hash_iter;
    struct merged_list *list_iter = mlist, *result_iter = result_list, *max_rank_node = NULL;    

    for (from = list; from!=NULL; from = from->next) {
        for (i = 0;i < from->num_results; i++) {
	    merge_insert(from->filename[i], from->word_count[i]);
	}
    }    
    for (i = 0; i<MAX_ENTRIES_PER_TABLE; i++) {
        for (hash_iter = head[i]; hash_iter!=NULL; hash_iter = hash_iter->next) {
	    if (list_iter==NULL) {
	        mlist = (struct merged_list *)malloc(sizeof(struct merged_list));
		list_iter = mlist;		
	    } else {
		list_iter->next = (struct merged_list *)malloc(sizeof(struct merged_list));
		list_iter = list_iter->next;
	    }
            list_iter->filename = (char *)malloc(strlen(hash_iter->filename));
	    strcpy(list_iter->filename, hash_iter->filename);
	    list_iter->rank = (hash_iter->weight)*(hash_iter->num_occur);
	    list_iter->next = NULL;
	    list_iter->removed = 0;
	    count++;
	}
    }
    for (i = 0; i < count; i++) {
        for (list_iter = mlist; list_iter!=NULL; list_iter = list_iter->next) {
	    if (list_iter->rank > max_rank && !list_iter->removed) {
	        max_rank      = list_iter->rank;
	        max_rank_node = list_iter;		    
	    }
	}
	if (max_rank_node==NULL) 
	    continue;
                
	if (result_iter==NULL) {
	    result_list = (struct merged_list *)malloc(sizeof(struct merged_list));
	    result_iter = result_list;
	} else {
	    result_iter->next = (struct merged_list *)malloc(sizeof(struct merged_list));
	    result_iter = result_iter->next;
	}
        result_iter->filename = (char *)malloc(strlen(max_rank_node->filename));
	strcpy(result_iter->filename, max_rank_node->filename);
	result_iter->rank = max_rank_node->rank;
        printf("%d\n", max_rank_node->rank);
        result_iter->next = NULL;
        max_rank  = 0;
	max_rank_node->removed = 1;
	max_rank_node = NULL;
    }
    for (result_iter = result_list; result_iter!=NULL; result_iter = result_iter->next) {
        printf("File %s Rank %d\n", result_iter->filename, result_iter->rank);
    }    
}
void free_up_local_mem()
{
    struct search_results *start = list, *temp;
    struct merged_hash *st, *te;
    struct merged_list *stl = mlist, *ttl = mlist;
    int i;

    while(start!=NULL) {
        temp = start->next;
	for (i = 0; i < start->num_results; i++) {
	    free(start->filename[i]);
        }
        free(start);
	start = temp;
    }	        
    for (i = 0; i < MAX_ENTRIES_PER_TABLE; i++) {
	st = te = head[i];
	while (st!=NULL) {
	    te = st->next;
	    free(st->filename);
	    free(st);
	    st = te;
	}
    }
    stl = mlist;
    ttl = mlist;  
    while (stl!=NULL) {
        ttl = stl->next;
        free(stl->filename);
        free(stl);
        stl = ttl;
    }

    stl = result_list;
    ttl = result_list;
    while (stl!=NULL) {
        ttl = stl->next;
        free(stl->filename);
        free(stl);
        stl = ttl;
    }
}
void get_the_file_and_index(char *temp)
{
    char line[200], filename[200]; 
    FILE *fp;
    int count = 0;

    strcpy(filename, temp);
    strcat(filename, ".index");
    retrieve_index(temp);
    
    if ((fp = fopen(filename, "r"))==NULL) {
        return;
    }

    if (list==NULL) {
	list = (struct search_results *)malloc(sizeof(struct search_results));
	iter = list;
    } else {
	iter->next = (struct search_results *)malloc(sizeof(struct search_results));
	iter = iter->next;
    }

    iter->num_results = 0;
    iter->next = NULL; 
    
    while (fscanf(fp, "%s", line)>0) {
	printf("line is %s\n", line);
	switch(count) 
	{
	case 0:
	    iter->word_count[iter->num_results] = atoi(line);
	    break;

	case 1:
	    iter->filename[iter->num_results] = (char *)malloc(strlen(line)+1);
	    strcpy(iter->filename[iter->num_results], line);
	    iter->num_results++;
            break;		

	default:
	    printf("its impossible to get here\n");
	    break;		     	    
	    	    
	}
	count = (count + 1) % 2;
	if (iter->num_results==MAX_RESULTS_PER_WORD) {
	    break;
        }
    }
    fclose(fp);
}
void search_init()
{
    int i;

    for (i = 0; i < MAX_ENTRIES_PER_TABLE; i++) {
	head[i] = NULL;
    }
    list = NULL;
    result_list = NULL;
    mlist = NULL;
    iter = list;
}
void do_search(char *search_word)
{
    int i, temp_len = 0;
    char temp[100];

    search_init();

    for (i = 0;i<strlen(search_word); i++) {
        if (search_word[i]==' ' || search_word[i]=='\t' || search_word[i]=='\n') {
            temp[temp_len] = '\0';
            get_the_file_and_index(temp);
            temp_len = 0;
            continue;
        }
        temp[temp_len++] = search_word[i];
    }
    if (temp_len!=0) {
        temp[temp_len] = '\0';
        get_the_file_and_index(temp);
    }
    merge_and_print_results();
    cache_results();
    printf("just before freeing the mem\n");
    free_up_local_mem();
}
