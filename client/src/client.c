#include "client.h"
#include "config.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int connect_to_server(const char* host, const char* port) {
    struct addrinfo hints;
    struct addrinfo* res = NULL;
    struct addrinfo* it = NULL;
    int sock = -1;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rc = getaddrinfo(host, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    for (it = res; it != NULL; it = it->ai_next) {
        sock = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (sock == -1) 
            continue;
        if (connect(sock, it->ai_addr, it->ai_addrlen) == 0) 
            break;
        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);
    return sock;
}

static int send_all(int sock, const void* buffer, size_t length) {
    const uint8_t* ptr = (const uint8_t*)buffer;
    size_t sent_total = 0;
    while (sent_total < length) {
        ssize_t n = send(sock, ptr + sent_total, length - sent_total, 0);
        if (n > 0) {
            sent_total += (size_t)n;
            continue;
        }
        if (n == -1 && errno == EINTR) 
            continue;
        return -1;
    }
    return 0;
}

static int recv_all(int sock, void* buffer, size_t length) {
    uint8_t* ptr = (uint8_t*)buffer;
    size_t received_total = 0;
    while (received_total < length) {
        ssize_t n = recv(
            sock, 
            ptr + received_total, 
            length - received_total, 
            0
        );
        if (n > 0) {
            received_total += (size_t)n;
            continue;
        }
        if (n == 0) return -1; // connection closed
        if (n == -1 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}

Client create_client(const char* host, const char* port) {
    Client client = {0};
    client.sock = connect_to_server(host, port);
    if (client.sock == -1) {
        fprintf(stderr, "failed to connect to %s:%s\n", host, port);
        exit(1);
    }
    client.header_bytes_read = 0;
    client.payload_buf = NULL;
    client.payload_capacity = 0;
    client.payload_length = 0;
    client.payload_bytes_read = 0;
    return client;
}

void destroy_client(Client* client) {
    if (client == NULL) return;
    if (client->sock >= 0) {
        close(client->sock);
        client->sock = -1;
    }
    if (client->payload_buf != NULL) {
        free(client->payload_buf);
        client->payload_buf = NULL;
        client->payload_capacity = 0;
    }
}

int client_send_message(Client* client, const uint8_t* payload, uint32_t length) {
    if (length == 0 || length > MAX_MESSAGE_LENGTH) {
        fprintf(stderr, "invalid message length: %u\n", length);
        return -1;
    }
    uint32_t netlen = htonl(length);
    if (send_all(client->sock, &netlen, 4) == -1) 
        return -1;
    if (send_all(client->sock, payload, length) == -1) 
        return -1;
    return 0;
}

int client_recv_message(Client* client, uint8_t** out_buf, uint32_t* out_len) {
    uint32_t netlen = 0;
    if (recv_all(client->sock, &netlen, 4) == -1) 
        return -1;
    uint32_t length = ntohl(netlen);
    if (length == 0 || length > MAX_MESSAGE_LENGTH) 
        return -1;
    uint8_t* buf = (uint8_t*)malloc(length + 1);
    if (buf == NULL) 
        return -1;
    if (recv_all(client->sock, buf, length) == -1) { 
        free(buf); 
        return -1; 
    }
    buf[length] = '\0';
    *out_buf = buf;
    *out_len = length;
    return 0;
}


