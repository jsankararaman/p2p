#include <math.h>
#include <fcntl.h>
#include <unistd.h>

char back_host[40];

long generate_id(char *str)
{
    long sum = 0, i;

    for (i = 0; str[i]!='\0'; i++) {
        sum = (sum + str[i])%32768;
    }
    return sum%32768;
}

int main()
{
    printf("%ld", generate_id("temp2.txt"));
    return 1;
}
