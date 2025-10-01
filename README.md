## cchat

tiny chat * one server, many clients * I/O multiplexing in C, and a gentle intro to network programming.
this project was made for writing an article on low level networking through multiplexing.

### general info

- **Tech**: C, POSIX sockets, `select(2)`, `getline(3)`, simple Makefiles
- **Protocol**: length‑prefixed frames (4‑byte big‑endian) up to 64 KiB
- **Shape**: a single‑process server that accepts multiple clients; a client that multiplexes stdin and the socket without threads
- **Goal**: show the essential moving parts clearly, without magic

### why it matters

I/O multiplexing lets one process wait on many file descriptors at once. No threads, no async frameworks—just `select(2)` and clear state machines

---

## architecture

- **server**
  - listens on TCP
  - tracks connected clients in an indexable container keyed by socket fd
  - uses `select(2)` to wake up for new connections and readable clients
  - deads messages incrementally: 4‑byte length header → payload buffer
  - first complete message from a client sets its nickname
  - subsequent messages are broadcast as `nickname: message` to all clients

- **client**
  - connects via `getaddrinfo(3)` to host/port (defaults `127.0.0.1:8080`)
  - uses `select(2)` to multiplex stdin and the socket
  - frames outbound messages with a 4‑byte big‑endian length
  - prints complete inbound frames to stdout
  - first line you send sets your nickname

---

## framing protocol

- `uint32_t length_be` (network byte order) + `length` bytes of payload
- max message size: `65536` bytes (see `MAX_MESSAGE_LENGTH`)
- server treats the client’s first message as the nickname (max 255 chars)

---

## xxecution flow

### server

1) accept new connections when the listening socket is readable.
2) for each readable client socket, progress its receive state machine:
   - read header until 4 bytes → decode length → ensure buffer
   - read payload until `length` bytes
   - on full frame: set nickname or broadcast

core multiplexing loop:

```c
// server/src/processing.c
fd_set read_fds;
FD_ZERO(&read_fds);
FD_SET(server->sock, &read_fds);
for (int i = 0; i < MAX_CLIENTS; i++) {
    if (server->clients.connected[i] != NULL) {
        FD_SET(server->clients.connected[i]->sock, &read_fds);
    }
}
int max_fd = server->clients.max_fd < 0 ? server->sock : server->clients.max_fd;
int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
if (FD_ISSET(server->sock, &read_fds)) { /* accept */ }
/* iterate clients and process */
```

incremental receive (header → payload) on the server uses non‑blocking `recv(MSG_DONTWAIT)` and explicit state to avoid partial‑read pitfalls.

### client

1) wait on both stdin and the socket using `select(2)`.
2) if stdin is ready, read a line, strip newline, frame and send.
3) if the socket is ready, incrementally read header and payload; on full frame, print to stdout.

core multiplexing loop:

```c
// client/src/processing.c
int stdin_fd = fileno(stdin);
while (1) {
    fd_set read_fds; FD_ZERO(&read_fds);
    FD_SET(stdin_fd, &read_fds);
    FD_SET(client->sock, &read_fds);
    int max_fd = client->sock > stdin_fd ? client->sock : stdin_fd;
    int rc = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
    if (FD_ISSET(stdin_fd, &read_fds)) { /* getline → send frame */ }
    if (FD_ISSET(client->sock, &read_fds)) { /* recv header/payload → print */ }
}
```

---

## build and run

requirements: a POSIX system with `gcc` or clang.

build:

```bash
cd server && make
cd ../client && make
```

run the server:

```bash
./server/bin/server
```

run a client (defaults: `127.0.0.1:8080`):

```bash
./client/bin/client
# or
./client/bin/client -h 127.0.0.1 -p 8080
```

## notes

- server uses a simple fd‑indexed container for connected clients; it keeps `max_fd` to bound the `select(2)` scan.
- reads are incremental and resilient to partial headers/payloads.
- no threads: one process, one `select(2)` loop on each side.

if you want to extend this, natural next steps are authentication, rooms, and swapping `select(2)` for `poll(2)`/`epoll(7)`/`kqueue(2)` depending on platform.


