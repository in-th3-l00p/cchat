#ifndef _CLIENT_PROCESSING_H_
#define _CLIENT_PROCESSING_H_

#include "server.h"
#include <sys/select.h>

void process_client(fd_set* read_fds, Server* server, int client_index);

#endif