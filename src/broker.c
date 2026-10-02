#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include "communication.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static char subscriptions[CHILD_COUNT][MAX_SUBS][TOPIC_SIZE];
static int updateSubscription(int client, const Message *m) {
    int free_slot = -1;
    for (int i = 0; i < MAX_SUBS; i++) {
        if (!strcmp(subscriptions[client][i], m->topic)) {
            if (m->operation == OP_UNSUB) subscriptions[client][i][0] = '\0';
            return 0;
        }
        if (!subscriptions[client][i][0] && free_slot < 0) free_slot = i;
    }
    if (m->operation == OP_UNSUB) return 0;
    if (free_slot < 0) return -1;
    strcpy(subscriptions[client][free_slot], m->topic);
    return 0;
}
int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    Connection server = {-1}, clients[CHILD_COUNT];
    pid_t children[CHILD_COUNT] = {0};
    int ready[CHILD_COUNT] = {0}, ready_count = 0, status = EXIT_FAILURE;
    for (int i = 0; i < CHILD_COUNT; i++) clients[i].handle = -1;
    unsigned short port = 0; /* OS chooses a free loopback port. */
    if (communicationListen(&server, &port) < 0) { perror("listen"); goto cleanup; }
    printf("[Master] Escuchando en 127.0.0.1:%u\n", port);
    char port_text[16];
    snprintf(port_text, sizeof port_text, "%u", port);
    const char *programs[] = {"./p1", "./p2", "./p3"};
    for (int i = 0; i < CHILD_COUNT; i++) {
        children[i] = fork();
        if (children[i] < 0) { perror("fork"); goto cleanup; }
        if (!children[i]) {
            execl(programs[i], programs[i], port_text, (char *)NULL);
            perror("exec"); _exit(127);
        }
        printf("[Master] Lanzado P%d (PID %ld)\n", i + 1, (long)children[i]);
    }
    for (int i = 0; i < CHILD_COUNT; i++)
        if (communicationAccept(&server, &clients[i]) < 0) { perror("accept"); goto cleanup; }
    communicationClose(&server);
    int active = CHILD_COUNT;
    while (active) {
        int index = communicationWait(clients, CHILD_COUNT);
        if (index < 0) { perror("wait"); goto cleanup; }
        char line[MESSAGE_SIZE];
        int received = receiveMessage(&clients[index], line, sizeof line);
        if (!received) {
            communicationClose(&clients[index]);
            memset(subscriptions[index], 0, sizeof subscriptions[index]);
            active--; continue;
        }
        Message m;
        if (received < 0 || parseMessage(line, &m) < 0) { fprintf(stderr, "Mensaje inválido\n"); goto cleanup; }
        if (m.operation == OP_SUB || m.operation == OP_UNSUB) {
            if (updateSubscription(index, &m) < 0) goto cleanup;
            printf("[Broker] cliente %d: %s\n", index + 1, line);
        } else if (m.operation == OP_READY) {
            if (ready[index]) goto cleanup;
            ready[index] = 1;
            if (++ready_count == CHILD_COUNT) {
                for (int i = 0; i < CHILD_COUNT; i++)
                    if (sendMessage(&clients[i], "START") < 0) goto cleanup;
            }
        } else if (m.operation == OP_PUB) {
            m.operation = OP_MSG;
            if (serializeMessage(&m, line, sizeof line) < 0) goto cleanup;
            int recipients = 0;
            for (int i = 0; i < CHILD_COUNT; i++) {
                for (int j = 0; j < MAX_SUBS; j++) {
                    if (clients[i].handle >= 0 && !strcmp(subscriptions[i][j], m.topic)) {
                        if (sendMessage(&clients[i], line) < 0) goto cleanup;
                        recipients++; break;
                    }
                }
            }
            printf("[Broker] %s -> %d suscriptor(es)\n", line, recipients);
        } else goto cleanup;
    }
    status = EXIT_SUCCESS;
cleanup:
    communicationClose(&server);
    for (int i = 0; i < CHILD_COUNT; i++) {
        communicationClose(&clients[i]);
        if (status != EXIT_SUCCESS && children[i] > 0) kill(children[i], SIGTERM);
    }
    for (int i = 0; i < CHILD_COUNT; i++) {
        if (children[i] > 0) {
            int result;
            if (waitpid(children[i], &result, 0) < 0 || !WIFEXITED(result) || WEXITSTATUS(result)) status = EXIT_FAILURE;
        }
    }
    if (!status) puts("[Master] Demostración completada; tres hijos finalizados correctamente.");
    return status;
}
