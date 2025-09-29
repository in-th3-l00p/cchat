#include "client_processing.h"
#include "config.h"
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

typedef enum RecvStatus {
    RECV_OK = 0,
    RECV_WOULDBLOCK = 1,
    RECV_CLOSED = 2,
    RECV_ERROR = 3
} RecvStatus;

// unified handler for RecvStatus; returns 1 if caller should stop/return
static inline int handle_recv_status(
    const char* context,
    RecvStatus s,
    Server* server,
    Client* client
) {
    switch (s) {
        case RECV_OK:
        case RECV_WOULDBLOCK:
            return 0;
        case RECV_CLOSED:
            remove_client(&server->clients, client->sock);
            return 1;
        case RECV_ERROR:
        default:
            perror(context);
            remove_client(&server->clients, client->sock);
            return 1;
    }
}

// read as much of the 4-byte header as available; when complete, validate and
// ensure the payload buffer is allocated
// does not close the client.
static inline RecvStatus read_header_and_prepare_payload(Client* client) {
    // Read header
    while (client->header_bytes_read < 4) {
        ssize_t n = recv(
            client->sock,
            client->header_buf + client->header_bytes_read,
            4 - client->header_bytes_read,
            MSG_DONTWAIT
        );
        if (n > 0) {
            client->header_bytes_read += (uint32_t)n;
            continue;
        } else if (n == 0) {
            return RECV_CLOSED;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return RECV_WOULDBLOCK;
            return RECV_ERROR;
        }
    }

    // header complete → decode length, validate, and allocate payload buffer
    if (client->payload_length == 0) {
        uint32_t netlen;
        memcpy(&netlen, client->header_buf, 4);
        uint32_t len = ntohl(netlen);
        if (len == 0 || len > MAX_MESSAGE_LENGTH)
            return RECV_ERROR;
        client->payload_length = len;
        if (client->payload_capacity < len) {
            uint8_t* new_buf = realloc(client->payload_buf, len);
            if (new_buf == NULL)
                return RECV_ERROR;
            client->payload_buf = new_buf;
            client->payload_capacity = len;
        }
        client->payload_bytes_read = 0;
    }

    return RECV_OK;
}

// read as much of the payload as available
static inline RecvStatus read_payload(Client* client) {
    if (client->header_bytes_read < 4 || client->payload_length == 0)
        return RECV_OK;

    while (client->payload_bytes_read < client->payload_length) {
        ssize_t n = recv(
            client->sock,
            client->payload_buf + client->payload_bytes_read,
            client->payload_length - client->payload_bytes_read,
            MSG_DONTWAIT
        );
        if (n > 0) {
            client->payload_bytes_read += (uint32_t)n;
            continue;
        } else if (n == 0) {
            return RECV_CLOSED;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return RECV_WOULDBLOCK;
            return RECV_ERROR;
        }
    }

    return RECV_OK;
}

// process a fully-received message; here we echo it back with a length prefix
static inline void handle_complete_message(Client* client) {
    uint32_t netlen = htonl(client->payload_length);
    send(client->sock, &netlen, 4, 0);
    send(client->sock, client->payload_buf, client->payload_length, 0);
}

// reset receive state to accept next message
static inline void reset_receive_state(Client* client) {
    client->header_bytes_read = 0;
    client->payload_bytes_read = 0;
    client->payload_length = 0;
}

void process_client(
    fd_set* read_fds,
    Server* server, 
    int client_index
) {
    if (
        server->clients.connected[client_index] == NULL || 
        !FD_ISSET(server->clients.connected[client_index]->sock, read_fds)
    )
        return;
    Client* client = server->clients.connected[client_index];

    RecvStatus s = read_header_and_prepare_payload(client);
    if (handle_recv_status(
        "process_client: header read/prepare failed",
        s,
        server,
        client
    ))
        return;

    s = read_payload(client);
    if (handle_recv_status(
        "process_client: payload read failed",
        s,
        server,
        client
    )) 
        return;

    if (
        client->payload_length > 0 && 
        client->payload_bytes_read == client->payload_length
    ) {
        printf(
            "process_client: client %d complete message of %u bytes\n",
            client_index,
            client->payload_length
        );
        handle_complete_message(client);
        reset_receive_state(client);
    }
}