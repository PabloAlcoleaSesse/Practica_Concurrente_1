#ifndef COMMUNICATION_H
#define COMMUNICATION_H
#include <stddef.h>
typedef struct { int handle; } Connection;
/* All socket operations, framing and readiness checks belong to this layer. */
int communicationListen(Connection *server, unsigned short *port);
int communicationAccept(Connection *server, Connection *client);
int communicationConnect(Connection *connection, unsigned short port);
int sendMessage(Connection *connection, const char *line);
/* 1: line, 0: EOF, -1: error. Lines exclude the terminating newline. */
int receiveMessage(Connection *connection, char *line, size_t capacity);
/* Returns ready index, -1 on error or timeout (10 seconds). */
int communicationWait(Connection *connections, size_t count);
void communicationClose(Connection *connection);
#endif
