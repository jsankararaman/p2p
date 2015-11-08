#include "common.h"
#include <string.h>
#include <stdio.h>

long generate_id(char *str)
{
    long sum = 0, i;
    
    for (i = 0; str[i]!='\0'; i++) {
	sum = (sum + str[i])%TTWOBIT;
    }
    return sum%TTWOBIT;
}
int readline(int fd, char *str)
{
    int i = 0;
    char c;

    while (read(fd, &c, 1)>0 && c!='\n') {
        str[i++] = c;
    }
    str[i] = '\0';

    return i;
}
void writeline(int fd, char *str)
{
    if (write(fd, str, strlen(str))<0) {
	printf("write error %s\n", str);
    }
    write(fd, "\n", 1);
}
void writelinenum(int fd, long num)
{
    char buffer[40];

    sprintf(buffer, "%ld", num);
    write(fd, buffer, strlen(buffer));
    write(fd, "\n", 1);
}
