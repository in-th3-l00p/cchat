#include "server.h"

int main() {
    Server server = create_server();
    destroy_server(&server);
    return 0;
}
