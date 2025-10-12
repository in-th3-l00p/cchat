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
static inline void handle_complete_message(
    Server* server,
    Client* current_client
) {
    printf(
        "process_client: client %d complete message of %u bytes\n",
        current_client->sock,
        current_client->payload_length
    );

    if (strlen(current_client->name) <= 0) {
        if (strlen((char*)current_client->payload_buf) > MAX_NAME_LENGTH) {
            return;
        }

        printf(
            "process_client: client %d is setting its name to %s\n", 
            current_client->sock, 
            current_client->payload_buf
        );

        update_nickname(
            &server->clients, 
            current_client->sock, 
            (char*)current_client->payload_buf
        );
        return;
    }

    for (int i = 0; i <= server->clients.max_fd; i++) {
        Client* client = server->clients.connected[i];
        if (client == NULL)
            continue;

        const char* delim = ": ";
        uint32_t name_len = (uint32_t)strlen(current_client->name);
        uint32_t delim_len = 2;
        uint32_t msg_len = current_client->payload_length;

        uint32_t max_msg_len = MAX_MESSAGE_LENGTH;
        uint32_t available_for_msg = (name_len + delim_len < max_msg_len)
            ? (max_msg_len - name_len - delim_len)
            : 0;
        uint32_t msg_to_copy = msg_len > available_for_msg ? available_for_msg : msg_len;
        uint32_t out_len = name_len + delim_len + msg_to_copy;

        uint8_t* out = malloc(out_len);
        if (out == NULL) {
            perror("process_client: malloc failed");
            continue;
        }

        memcpy(out, current_client->name, name_len);
        memcpy(out + name_len, delim, delim_len);
        memcpy(out + name_len + delim_len, current_client->payload_buf, msg_to_copy);

        uint32_t netlen = htonl(out_len);
        send(client->sock, &netlen, 4, 0);
        send(client->sock, out, out_len, 0);

        free(out);

        printf(
            "process_client: client %d sent message to client %d of %u bytes (prefixed)\n",
            current_client->sock,
            client->sock,
            out_len
        );
    }
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
        handle_complete_message(
            server,
            client
        );
        reset_receive_state(client);
    }
}