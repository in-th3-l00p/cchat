#ifndef _NETWORKING_H_
#define _NETWORKING_H_

#include "clients.h"

typedef struct {
    int sock;
    Clients clients;
} Server;

Server create_server();
void destroy_server(Server* server);

#endif