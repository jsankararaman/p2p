#ifndef ROUTING_H

#define ROUTING_H

#define ROUTING_PORT 2003

int routing_started;
void *routing_protocol(void *);
char *who_has(char *);
char *who_has_id(long);
int lookup_id(long);
void print_route(void);

#endif
