#include "processing.h"
#include "config.h"
#include "client_processing.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

static inline fd_set get_read_fds(Server* server) {
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

    struct timeval timeout = {0};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int activity = select(
        max_fd, 
        &read_fds, 
        NULL, 
        NULL, 
        &timeout
    );

    if (activity < 0)
        perror("get_read_fds: select failed");
    return read_fds;
}

static inline void process_server_socket(
    fd_set* read_fds, 
    Server* server
) {
    if (!FD_ISSET(server->sock, read_fds))
        return;
    struct sockaddr_in sock;
    socklen_t sock_len = sizeof(sock);
    int sock_fd = accept(server->sock, (struct sockaddr*)&sock, &sock_len);
    if (sock_fd == -1) {
        perror("process_server: accept failed");
        return;
    }
    printf("process_server: accepted connection from %s\n", inet_ntoa(sock.sin_addr));
    add_client(&server->clients, sock_fd, "");
}


static inline void process_clients_socket(
    fd_set* read_fds, 
    Server* server
) {
    for (int i = 0; i <= server->clients.max_fd; i++) {
        if (server->clients.connected[i] != NULL) {
            if (FD_ISSET(server->clients.connected[i]->sock, read_fds)) {
                process_client(read_fds, server, i);
            }
        }
    }
}

void process_server(Server* server) {
    fd_set read_fds = get_read_fds(server);
    process_server_socket(&read_fds, server);
    process_clients_socket(&read_fds, server);
}
