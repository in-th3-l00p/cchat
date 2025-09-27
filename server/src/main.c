#include <stdio.h>
#include "networking.h"

int main() {
    Server server = create_server();
    destroy_server(&server);
    return 0;
}
