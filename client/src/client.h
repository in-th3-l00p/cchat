#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <stdint.h>

typedef struct {
    int sock;
    // incremental receive state for length-prefixed frames
    uint8_t header_buf[4];
    uint32_t header_bytes_read;
    uint8_t* payload_buf;
    uint32_t payload_capacity;
    uint32_t payload_length;
    uint32_t payload_bytes_read;
} Client;

Client create_client(const char* host, const char* port);
void destroy_client(Client* client);
int client_send_message(Client* client, const uint8_t* payload, uint32_t length);
int client_recv_message(Client* client, uint8_t** out_buf, uint32_t* out_len);

#endif

