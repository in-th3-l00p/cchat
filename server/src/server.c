#include "server.h"
#include "clients.h"
#include "config.h"
#include "processing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

Server create_server() {
    Server server = {0};
    int yes = 1;
    if ((server.sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("create_server: socket failed");
        exit(1);
    }
    setsockopt(server.sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in sa;
    memset(&sa,0,sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(PORT);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if (
        bind(server.sock, (struct sockaddr*)&sa, sizeof(sa)) == -1 ||
        listen(server.sock, BACKLOG) == -1
    ) {
        close(server.sock);
        perror("create_server: bind or listen failed");
        exit(1);
    }

    server.clients = create_client_container();

    return server;
}

void destroy_server(Server* server) {
    close(server->sock);
    destroy_client_container(&server->clients);
}

void run_server(Server* server) {
    printf("run_server: running server on port %d\n", PORT);
    while (1) 
        process_server(server);
}