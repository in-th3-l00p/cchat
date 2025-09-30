#include "processing.h"
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

static inline int recv_header_and_prepare_payload(Client* client) {
    while (client->header_bytes_read < 4) {
        ssize_t n = recv(
            client->sock,
            client->header_buf + client->header_bytes_read,
            4 - client->header_bytes_read,
            0
        );
        if (n > 0) {
            client->header_bytes_read += (uint32_t)n;
            continue;
        } else if (n == 0) {
            return -1; // closed
        } else {
            if (errno == EINTR)
                continue;
            return -1;
        }
    }

    if (client->payload_length == 0) {
        uint32_t netlen;
        memcpy(&netlen, client->header_buf, 4);
        uint32_t len = ntohl(netlen);
        if (len == 0 || len > MAX_MESSAGE_LENGTH)
            return -1;
        client->payload_length = len;
        if (client->payload_capacity < len) {
            uint8_t* new_buf = realloc(client->payload_buf, len);
            if (new_buf == NULL)
                return -1;
            client->payload_buf = new_buf;
            client->payload_capacity = len;
        }
        client->payload_bytes_read = 0;
    }

    return 0;
}

static inline int recv_payload(Client* client) {
    if (client->header_bytes_read < 4 || client->payload_length == 0)
        return 0;

    while (client->payload_bytes_read < client->payload_length) {
        ssize_t n = recv(
            client->sock,
            client->payload_buf + client->payload_bytes_read,
            client->payload_length - client->payload_bytes_read,
            0
        );
        if (n > 0) {
            client->payload_bytes_read += (uint32_t)n;
            continue;
        } else if (n == 0) {
            return -1; // closed
        } else {
            if (errno == EINTR)
                continue;
            return -1;
        }
    }

    return 0;
}

static inline void reset_recv_state(Client* client) {
    client->header_bytes_read = 0;
    client->payload_bytes_read = 0;
    client->payload_length = 0;
}

static inline void handle_complete_message(const uint8_t* buf, uint32_t len) {
    fwrite(buf, 1, len, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

void run_client(Client* client) {
    char* line = NULL;
    size_t line_cap = 0;
    int stdin_fd = fileno(stdin);
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(stdin_fd, &read_fds);
        FD_SET(client->sock, &read_fds);
        int max_fd = client->sock > stdin_fd ? client->sock : stdin_fd;

        int rc = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (rc < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(stdin_fd, &read_fds)) {
            ssize_t line_len = getline(&line, &line_cap, stdin);
            if (line_len == -1) {
                // EOF on stdin: just exit
                break;
            }
            if (line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r'))
                line[--line_len] = '\0';
            if (line_len == 0)
                continue;
            if (strcmp(line, "/quit") == 0 || strcmp(line, "/exit") == 0)
                break;
            if (line_len > (ssize_t)MAX_MESSAGE_LENGTH) {
                fprintf(stderr, "input too long (>%d bytes)\n", MAX_MESSAGE_LENGTH);
                continue;
            }
            if (client_send_message(client, (const uint8_t*)line, (uint32_t)line_len) == -1) {
                fprintf(stderr, "send failed\n");
                break;
            }

            // clear current line, move up, clear echoed input
            fputs("\033[2K\r\033[1A\033[2K\r", stdout);
            fflush(stdout);
        }

        if (FD_ISSET(client->sock, &read_fds)) {
            if (recv_header_and_prepare_payload(client) == -1) {
                fprintf(stderr, "connection closed by server\n");
                break;
            }
            if (recv_payload(client) == -1) {
                fprintf(stderr, "connection closed by server\n");
                break;
            }
            if (client->payload_length > 0 && client->payload_bytes_read == client->payload_length) {
                handle_complete_message(client->payload_buf, client->payload_length);
                reset_recv_state(client);
            }
        }
    }

    free(line);
}


