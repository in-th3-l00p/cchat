#include "server.h"

int main() {
    Server server = create_server();
    run_server(&server);
    destroy_server(&server);
    return 0;
}
