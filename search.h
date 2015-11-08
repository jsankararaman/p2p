#ifndef SEARCH_H
#define SEARCH_H

#define MAX_RESULTS_PER_WORD  100
#define MAX_ENTRIES_PER_TABLE 100

struct search_results
{
    // the following three variables are written in the index
    int word_count[MAX_RESULTS_PER_WORD];  // number of occurences in the file corresponding to the index
    char *filename[MAX_RESULTS_PER_WORD];  // filenames which have this file

    char marked[MAX_RESULTS_PER_WORD];

    int num_results;      // number of results ie., the number of files for this particular word
    struct search_results *next;
};

struct merged_hash
{
    char *filename;
    int num_occur;

    int weight;
    struct merged_hash *next;
};

struct merged_list
{
    char *filename;
    int rank;
    char removed;

    struct merged_list *next;
};

struct merged_hash *head[MAX_ENTRIES_PER_TABLE];
struct search_results *list, *iter;
struct merged_list *mlist, *result_list;

pthread_mutex_t search_mutex;

#endif
