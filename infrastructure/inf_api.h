void read_write_start();

void *read_file_for_remote_peer(void *);
void *write_file_for_remote_peer(void *);
int write_file_to_peer_api(char *, char *, int, int);
int read_file_from_peer_api(char *, char *);
