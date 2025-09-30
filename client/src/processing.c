#include "processing.h"
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void run_client(Client* client) {
    char* line = NULL;
    size_t line_cap = 0;
    ssize_t line_len;
    while ((line_len = getline(&line, &line_cap, stdin)) != -1) {
        if (line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
            line[--line_len] = '\0';
        }

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

        uint8_t* resp = NULL;
        uint32_t resp_len = 0;
        if (client_recv_message(client, &resp, &resp_len) == -1) {
            fprintf(stderr, "receive failed (server closed?)\n");
            break;
        }

        fwrite(resp, 1, resp_len, stdout);
        fputc('\n', stdout);
        fflush(stdout);
        free(resp);
    }

    free(line);
}


