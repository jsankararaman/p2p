#ifndef INDEH_H

#define INDEH_H

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#define HASH_SIZE            100
#define MAX_WORD_LENGTH      100
#define MAX_FILE_LENGTH_SIZE 100
#define MAX_META_TABLE 100

char index_meta_file[100];
int mydirfd;

struct fileinfo
{
    char filename[MAX_FILE_LENGTH_SIZE];
    int count;
    struct fileinfo *next;
};
struct dir_list
{
    char word[MAX_WORD_LENGTH];
    int count;
    struct fileinfo *f;
    struct dir_list *next;
};
struct local_meta_data
{
    char *filename;
    struct local_meta_data *next;
};

struct local_meta_data *meta_table[MAX_META_TABLE];
struct dir_list *L[HASH_SIZE];

void index_init();

#endif
