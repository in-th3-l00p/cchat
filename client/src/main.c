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

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT "8080"
#define MAX_MESSAGE_LENGTH 65536

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
        if (sock == -1) continue;
        if (connect(sock, it->ai_addr, it->ai_addrlen) == 0) break;
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
        if (n == -1 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}

static int recv_all(int sock, void* buffer, size_t length) {
    uint8_t* ptr = (uint8_t*)buffer;
    size_t received_total = 0;
    while (received_total < length) {
        ssize_t n = recv(sock, ptr + received_total, length - received_total, 0);
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

static int send_message(int sock, const uint8_t* payload, uint32_t length) {
    if (length == 0 || length > MAX_MESSAGE_LENGTH) {
        fprintf(stderr, "invalid message length: %u\n", length);
        return -1;
    }
    uint32_t netlen = htonl(length);
    if (send_all(sock, &netlen, 4) == -1) return -1;
    if (send_all(sock, payload, length) == -1) return -1;
    return 0;
}

static int recv_message(int sock, uint8_t** out_buf, uint32_t* out_len) {
    uint32_t netlen = 0;
    if (recv_all(sock, &netlen, 4) == -1) return -1;
    uint32_t length = ntohl(netlen);
    if (length == 0 || length > MAX_MESSAGE_LENGTH) return -1;
    uint8_t* buf = (uint8_t*)malloc(length + 1);
    if (buf == NULL) return -1;
    if (recv_all(sock, buf, length) == -1) { free(buf); return -1; }
    buf[length] = '\0';
    *out_buf = buf;
    *out_len = length;
    return 0;
}

static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s [-h host] [-p port]\n", prog);
}

int main(int argc, char* argv[]) {
    const char* host = DEFAULT_HOST;
    const char* port = DEFAULT_PORT;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--host") == 0) && i + 1 < argc) {
            host = argv[++i];
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i + 1 < argc) {
            port = argv[++i];
        } else if (strcmp(argv[i], "-?" ) == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    int sock = connect_to_server(host, port);
    if (sock == -1) {
        fprintf(stderr, "failed to connect to %s:%s\n", host, port);
        return 1;
    }

    fprintf(stdout, "connected to %s:%s. Type messages and press Enter. Ctrl-D to quit.\n", host, port);
    fflush(stdout);

    char* line = NULL;
    size_t line_cap = 0;
    ssize_t line_len;
    while ((line_len = getline(&line, &line_cap, stdin)) != -1) {
        // strip trailing newline
        if (line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
            line[--line_len] = '\0';
        }

        if (line_len == 0) continue;

        if (strcmp(line, "/quit") == 0 || strcmp(line, "/exit") == 0) {
            break;
        }

        if (line_len > (ssize_t)MAX_MESSAGE_LENGTH) {
            fprintf(stderr, "input too long (>%d bytes)\n", MAX_MESSAGE_LENGTH);
            continue;
        }

        if (send_message(sock, (const uint8_t*)line, (uint32_t)line_len) == -1) {
            fprintf(stderr, "send failed\n");
            break;
        }

        uint8_t* resp = NULL;
        uint32_t resp_len = 0;
        if (recv_message(sock, &resp, &resp_len) == -1) {
            fprintf(stderr, "receive failed (server closed?)\n");
            break;
        }

        fwrite(resp, 1, resp_len, stdout);
        fputc('\n', stdout);
        fflush(stdout);
        free(resp);
    }

    free(line);
    close(sock);
    return 0;
}
