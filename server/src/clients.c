#include "clients.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

Clients create_client_container() {
    Clients clients = {0};
    clients.max_fd = -1;
    return clients;
}

void destroy_client_container(Clients* clients) {
    if (clients->max_fd < 0)
        return;
    for (int i = 0; i <= clients->max_fd; i++) {
        if (clients->connected[i] == NULL) {
            continue;
        }
        close(clients->connected[i]->sock);
        if (clients->connected[i]->payload_buf != NULL) {
            free(clients->connected[i]->payload_buf);
            clients->connected[i]->payload_buf = NULL;
            clients->connected[i]->payload_capacity = 0;
        }
        free(clients->connected[i]);
        clients->connected[i] = NULL;
    }
    clients->count = 0;
    clients->max_fd = -1;
}

int add_client(
    Clients* clients, 
    int sock, 
    const char32_t name[MAX_NAME_LENGTH]
) {
    if (
        sock < 0 || sock >= MAX_CLIENTS ||
        clients->count >= MAX_CLIENTS || 
        clients->connected[sock] != NULL
    ) {
        perror("add_client: socket fd out of range");
        errno = EINVAL;
        return -1;
    }
    clients->connected[sock] = malloc(sizeof(Client));
    if (clients->connected[sock] == NULL) {
        perror("add_client: malloc failed");
        errno = ENOMEM;
        return -1;
    }
    clients->connected[sock]->sock = sock;
    memcpy(clients->connected[sock]->name, name, sizeof(char32_t) * MAX_NAME_LENGTH);
    clients->connected[sock]->name[MAX_NAME_LENGTH - 1] = U'\0';
    clients->connected[sock]->header_bytes_read = 0;
    clients->connected[sock]->payload_buf = NULL;
    clients->connected[sock]->payload_capacity = 0;
    clients->connected[sock]->payload_length = 0;
    clients->connected[sock]->payload_bytes_read = 0;
    clients->count++;
    if (sock > clients->max_fd)
        clients->max_fd = sock;
    return 0;
}

void update_nickname(
    Clients* clients, 
    int sock, 
    const char32_t name[MAX_NAME_LENGTH]
) {
    if (
        clients->connected[sock] == NULL ||
        sock < 0 || sock >= MAX_CLIENTS
    ) {
        perror("update_nickname: client not found");
        errno = EINVAL;
        return;
    }
    memcpy(clients->connected[sock]->name, name, sizeof(char32_t) * MAX_NAME_LENGTH);
}

void remove_client(Clients* clients, int sock) {
    if (
        clients->connected[sock] == NULL ||
        sock < 0 || sock >= MAX_CLIENTS
    ) {
        perror("remove_client: client not found");
        errno = EINVAL;
        return;
    }
    close(clients->connected[sock]->sock);
    if (clients->connected[sock]->payload_buf != NULL) {
        free(clients->connected[sock]->payload_buf);
        clients->connected[sock]->payload_buf = NULL;
        clients->connected[sock]->payload_capacity = 0;
    }
    free(clients->connected[sock]);
    clients->connected[sock] = NULL;
    clients->count--;
    if (sock == clients->max_fd) {
        int new_max_fd = clients->max_fd - 1;
        while (new_max_fd >= 0 && clients->connected[new_max_fd] == NULL)
            new_max_fd--;
        clients->max_fd = new_max_fd;
    }
}
