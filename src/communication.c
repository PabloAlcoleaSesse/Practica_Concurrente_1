#include "communication.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int prepare(int fd) {
    if (fd < 0) return -1;
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) { close(fd); return -1; }
#ifdef SO_NOSIGPIPE
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof yes) < 0) {
        close(fd); return -1;
    }
#endif
    return fd;
}
void communicationClose(Connection *c) {
    if (c->handle >= 0) close(c->handle);
    c->handle = -1;
}
int communicationListen(Connection *c, unsigned short *port) {
    c->handle = prepare(socket(AF_INET, SOCK_STREAM, 0));
    if (c->handle < 0) return -1;
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(*port);
    socklen_t length = sizeof address;
    if (bind(c->handle, (struct sockaddr *)&address, length) < 0 ||
        listen(c->handle, 8) < 0 ||
        getsockname(c->handle, (struct sockaddr *)&address, &length) < 0) {
        communicationClose(c); return -1;
    }
    *port = ntohs(address.sin_port);
    return 0;
}
int communicationWait(Connection *connections, size_t count) {
    if (!count || count > 64) { errno = EINVAL; return -1; }
    struct pollfd descriptors[64];
    for (size_t i = 0; i < count; i++)
        descriptors[i] = (struct pollfd){connections[i].handle, POLLIN, 0};
    int result;
    do { result = poll(descriptors, count, 10000); } while (result < 0 && errno == EINTR);
    if (result <= 0) { if (!result) errno = ETIMEDOUT; return -1; }
    for (size_t i = 0; i < count; i++) if (descriptors[i].revents) return (int)i;
    return -1;
}
int communicationAccept(Connection *server, Connection *client) {
    if (communicationWait(server, 1) < 0) return -1;
    int fd;
    do { fd = accept(server->handle, NULL, NULL); } while (fd < 0 && errno == EINTR);
    client->handle = prepare(fd);
    return client->handle < 0 ? -1 : 0;
}
int communicationConnect(Connection *c, unsigned short port) {
    c->handle = prepare(socket(AF_INET, SOCK_STREAM, 0));
    if (c->handle < 0) return -1;
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(c->handle, (struct sockaddr *)&address, sizeof address) < 0) {
        communicationClose(c); return -1;
    }
    return 0;
}
static int sendAll(Connection *c, const char *data, size_t size) {
    while (size) {
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags = MSG_NOSIGNAL;
#endif
        ssize_t sent = send(c->handle, data, size, flags);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) return -1;
        data += sent; size -= (size_t)sent;
    }
    return 0;
}
int sendMessage(Connection *c, const char *line) {
    if (strchr(line, '\n') || strchr(line, '\r')) { errno = EINVAL; return -1; }
    if (sendAll(c, line, strlen(line)) < 0) return -1;
    return sendAll(c, "\n", 1);
}
int receiveMessage(Connection *c, char *line, size_t capacity) {
    size_t used = 0;
    for (;;) {
        char character;
        if (communicationWait(c, 1) < 0) return -1;
        ssize_t received = recv(c->handle, &character, 1, 0);
        if (received < 0 && errno == EINTR) continue;
        if (received <= 0) return received == 0 && used == 0 ? 0 : -1;
        if (character == '\n') { if (!capacity) return -1; line[used] = '\0'; return 1; }
        if (character == '\0' || character == '\r' || used + 1 >= capacity) {
            errno = EMSGSIZE; return -1;
        }
        line[used++] = character;
    }
}
