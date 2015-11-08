#include <stdio.h>
#include <string.h>

#include "../common.h"
#include "bootstrap.h"

int main()
{
    strcpy(booter_main, "152.14.93.61");
    printf("%s\n", booter_main);
    boot_me_up();
}
