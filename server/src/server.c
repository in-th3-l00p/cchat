#include "server.h"
#include "clients.h"
#include "config.h"
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
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server->sock, &read_fds);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (server->clients.connected[i] != NULL) {
                FD_SET(server->clients.connected[i]->sock, &read_fds);
            }
        }

        int max_fd = server->clients.max_fd;
        if (server->clients.max_fd < 0)
            max_fd = server->sock;
        max_fd++;

        int activity = select(
            max_fd, 
            &read_fds, 
            NULL, 
            NULL, 
            NULL
        );

        if (activity < 0) {
            perror("run_server: select failed");
            continue;
        } else if (activity == 0) {
            printf("run_server: select timed out\n");
            continue;
        } else {
            if (FD_ISSET(server->sock, &read_fds)) {
                struct sockaddr_in sock;
                socklen_t sock_len = sizeof(sock);
                int sock_fd = accept(server->sock, (struct sockaddr*)&sock, &sock_len);
                if (sock_fd == -1) {
                    perror("run_server: accept failed");
                    continue;
                }
                
                printf("run_server: accepted connection from %s\n", inet_ntoa(sock.sin_addr));
                add_client(&server->clients, sock_fd, U"");
            }
        }
    }
}