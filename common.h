#ifndef COMMON_H
#define COMMON_H

#define TTWOBIT 2147483647 
long MYID;
char MYIP[200];
char bootstrap[200];
char booter_main[100];

long generate_id(char *);
int readline(int, char *);
void writeline(int, char *);
void writelinenum(int, long);

#endif
