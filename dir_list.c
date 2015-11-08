#include "common.h"
#include "index.h"
#include "infrastructure/p2pd.h"

int dir_hash(char *name, int NUM_SLOTS)
{
    int sum = 0, i;

    for (i = 0; name[i]!='\0'; i++) {
	if (name[i]>='a' && name[i]<='z') 
	        sum = (sum + name[i] - 'a') % NUM_SLOTS;
	else if (name[i]>='A' && name[i]<='Z') 
		sum = (sum + name[i] - 'A') % NUM_SLOTS;
	
    }
    return sum % NUM_SLOTS;
}
/*
 * Helper function for indexing
 * Updates the fileinfo datastructures in a particular node in the table
 * */
void manage_fileinfo(struct dir_list *node, char *filename)
{
    struct fileinfo *temp;

    for (temp = node->f; temp!=NULL; temp = temp->next) {
        if (strcmp(temp->filename, filename)==0) {
            temp->count++;
            return;
        }
    }

    temp = (struct fileinfo *)malloc(sizeof(struct fileinfo));
    strcpy(temp->filename, filename);
    temp->count = 1;
    temp->next  = node->f;
    node->f     = temp;
}
/*
 * This is where we update our per-word data structures.
 * */
void insert(char *word, char *filename)
{
    int index = dir_hash(word, HASH_SIZE);
    struct dir_list *temp;

    printf("inserting %s for %s into slot %d\n", word, filename, index);
    if (L[index]==NULL) {
        L[index] = (struct dir_list *)malloc(sizeof(struct dir_list));

        strcpy(L[index]->word, word);
        L[index]->count = 1;
        L[index]->next = NULL;

        L[index]->f = (struct fileinfo *)malloc(sizeof(struct fileinfo));
        strcpy(L[index]->f->filename, filename);
        L[index]->f->count = 1;

        L[index]->f->next = NULL;
    } else {
        for (temp = L[index]; temp!=NULL; temp = temp->next) {
            if (strcmp(temp->word, word)==0) {
                temp->count++;
                manage_fileinfo(temp, filename);
                return;
            }
        }
        temp = (struct dir_list *)malloc(sizeof(struct dir_list));

        strcpy(temp->word, word);
        temp->count = 1;
        temp->next = L[index];

        temp->f = (struct fileinfo *)malloc(sizeof(struct fileinfo));
        strcpy(temp->f->filename, filename);
        temp->f->count = 1;

        temp->f->next = NULL;
        L[index] = temp;
    }
}
/*
 * We have encountered a file that has not been indexed before.
 * Read the file and update our local datastructures for each word in the file.
 * */
void add_to_index(char *filename)
{
    int filed, n, len = 0, testfd;
    char c;
    char buffer[100];

    strcpy(buffer, filename);
    strcat(buffer, ".meta");

    retrieve_index(buffer);
    if ((testfd = open(buffer, O_RDWR | O_APPEND))>0) {
	writeline(testfd, MYIP);
	close(testfd);
	store_index(buffer);
	return;
    } 
    printf("creating the file %s\n", buffer);
    if ((testfd = creat(buffer, 0666))<0) {
	return;
    }
    writeline(testfd, MYIP);
    close(testfd);	
    store_index(buffer);
	
    filed = open(filename, O_RDONLY);
    if (filed<0) {
        perror("error opening file : ");
        return;
    }
    printf("going through the file\n");
    while ((n = read(filed, &c, 1)) > 0) {
        if ((c>='a' && c<='z') || (c>='A' && c<='Z')) {
            buffer[len++] = c;
        } else {
            buffer[len] = '\0';
            len = 0;
            insert(buffer, filename);
            continue;
        }
    }
}
/*
 * Returns 1 if filename is in the hash table and 0 otherwise
 * */
int ispresent(char *filename)
{
    struct local_meta_data *temp = meta_table[dir_hash(filename, MAX_META_TABLE)];

    while (temp!=NULL && strcmp(temp->filename, filename)!=0) {
        temp = temp->next;
    }
    return (temp!=NULL);
}
/*
 * Inserts the string "filename" into a hash table
 * Each file already indexed is inserted into this data structure by 
 * the meta-indexer. Then we iterate through the current directory  
 * and check whether a file in our directory has been indexed or not.
 * */
void meta_data_insert(char *filename)
{
    struct local_meta_data *temp;

    if (ispresent(filename)) {
	return;
    }
    temp = (struct local_meta_data *)malloc(sizeof(struct local_meta_data));

    temp->filename = (char *)malloc(strlen(filename));
    strcpy(temp->filename, filename);
    temp->next = meta_table[dir_hash(filename, MAX_META_TABLE)];
   
    meta_table[dir_hash(filename, MAX_META_TABLE)] = temp;
    if (mydirfd>0) {
	writeline(mydirfd, filename);
    }
}
/*
 * Reads the meta index file and updates the data structures on
 * init. At the end of this function, we know what files we have
 * already indexed. 
 *
 * We use this data structure to check whether any new file has
 * been added to our directory; which we need to index.
 * */
void create_local_meta_index()
{
    char buffer[100], c;
    int n, i = 0;

    if (mydirfd<0) {
        return;
    }
    while ((n = read(mydirfd, &c, 1))>0)
    {
	if (c=='\n') {
	    buffer[i] = '\0';
	    i = 0;
            meta_data_insert(buffer);   // insert filename 
	    continue;
	}
	buffer[i++] = c;
    }
}
void iter_dir(char *dirname)
{
    DIR *dp;
    struct dirent *dirp;

    if ((dp = opendir(dirname))==NULL) {
        perror("cant open directory :");
	return;
    }

    create_local_meta_index();
    lseek(mydirfd, 0, SEEK_END);
    while ((dirp = readdir(dp))!=NULL) {
        if (strstr(dirp->d_name, ".txt")!=NULL && !ispresent(dirp->d_name) \
            && (strstr(dirp->d_name, ".index")==NULL) && (strstr(dirp->d_name, ".meta")==NULL)) {
	    printf("adding file %s to index\n", dirp->d_name);
	    meta_data_insert(dirp->d_name);
            add_to_index(dirp->d_name);
	}
    }
}
void append_index(char *name, struct fileinfo *word_entry)
{
    char buffer[1000], filename[100];
    int i, fd;
    struct fileinfo *temp = word_entry;

    strcpy(filename, name);
    strcat(filename, ".index");

    if ((fd = open(filename, O_RDWR | O_APPEND))<0) {
        fd = creat(filename, 0666);
    }

    buffer[0] = '\0';
    for (temp = word_entry; temp!=NULL; temp=temp->next) {
        sprintf(buffer, "%d\n%s\n", temp->count, temp->filename);
        printf("word %s\n", buffer);
        write(fd, buffer, strlen(buffer));
    }
    close(fd);
}
void update_index()
{
    int i;
    struct dir_list     *temp;
    struct fileinfo *filetemp;
    char filename[100];

    for (i = 0; i < HASH_SIZE; i++) {
        for (temp = L[i]; temp!=NULL; temp = temp->next) {
            strcpy(filename, temp->word);
	    strcat(filename, ".index");
	    retrieve_index(filename);
            for (filetemp = temp->f; filetemp!=NULL; filetemp = filetemp->next) {
                append_index(temp->word, filetemp);     
            }
	    store_index(filename);
        }
    } 
}
void index_init()
{
    int i;

    mydirfd = -1;
    printf("indexing started\n");
    for (i = 0; i < MAX_META_TABLE; i++) {
        meta_table[i] = NULL;
    }
    for (i = 0; i < HASH_SIZE; i++) {
        L[i] = NULL;
    }
    if ((mydirfd = open(index_meta_file, O_RDWR))<0) {
        mydirfd = creat(index_meta_file, 0666);
    }
    printf("going through the directory\n");
    iter_dir(".");
    
    // update_index comes here
    update_index();
    printf("indexing done\n");
    close(mydirfd);
}
