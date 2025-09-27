/* clients.h 
 * data structure for identifying currently connected users
 * a linked list of usernames and file descriptors
 * */

#ifndef _CLIENTS_H_
#define _CLIENTS_H_

#include <stdint.h>
#if defined(__has_include) // mainly bcs of linting issues
    #if __has_include(<uchar.h>)
        #include <uchar.h>
    #else
        typedef uint32_t char32_t;
    #endif
#else
    typedef uint32_t char32_t;
#endif

#define MAX_NAME_LENGTH 256
#define MAX_CLIENTS 1000

typedef struct Client {
    int sock;
    char32_t name[MAX_NAME_LENGTH];
} Client;

// key -> value store of the connected
// key is the socket fd
typedef struct Clients {
    Client* connected[MAX_CLIENTS];
    uint32_t count;
    int max_fd; // highest used  socket fd
} Clients;

// function prototypes
Clients create_client_container();
void destroy_client_container(Clients* clients);
int add_client(Clients* clients, int sock, char32_t name[MAX_NAME_LENGTH]);
void update_nickname(Clients* clients, int sock, char32_t name[MAX_NAME_LENGTH]);
void remove_client(Clients* clients, int sock);

#endif
