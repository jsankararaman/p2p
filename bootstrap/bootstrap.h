#ifndef BOOTSTRAP_H

#define BOOTSTRAP_H

#define MOTHERSHIP     "152.14.93.61"

struct bootstrap_message
{
    char your_ip[40];
    char your_neighbor[40];
    long your_id;
};

void boot_me_up();

#endif
