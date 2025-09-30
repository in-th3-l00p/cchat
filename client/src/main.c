#include "client.h"
#include "processing.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char* prog) {
    fprintf(stderr, "usage: %s [-h host] [-p port]\n", prog);
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

    Client client = create_client(host, port);
    fprintf(stdout, "connected to %s:%s, type messages and press Enter, Ctrl-D to quit.\n", host, port);
    fflush(stdout);
    run_client(&client);
    destroy_client(&client);
    return 0;
}
