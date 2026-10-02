#include "demo.h"
#include "pubsub_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { PubSubClient client; int role, done, failed, temperatures, states, alarms; } Demo;
static void check(Demo *d, int result) { if (result < 0) d->failed = 1; }
static void onMessage(const char *topic, const PubSubValue *value, void *context) {
    Demo *d = context;
    printf("[P%d] %s = ", d->role, topic);
    if (value->type == TYPE_INT) printf("%d (INT)\n", value->data.integer);
    else if (value->type == TYPE_FLOAT) printf("%.9g (FLOAT)\n", (double)value->data.real);
    else printf("%s (STRING)\n", value->data.string);
    if (!strcmp(topic, "fin")) {
        if (value->type != TYPE_INT || value->data.integer != 1) d->failed = 1;
        d->done = 1;
    } else if (!strcmp(topic, "estado")) {
        d->states++;
        if (value->type != TYPE_INT || value->data.integer != 42) d->failed = 1;
        if (d->role == 2) check(d, publish(&d->client, "temperatura", (PubSubValue){.type = TYPE_FLOAT, .data.real = 23.5f}));
    } else if (!strcmp(topic, "temperatura")) {
        d->temperatures++;
        float expected = d->temperatures == 1 ? 23.5f : 24.5f;
        if (value->type != TYPE_FLOAT || value->data.real != expected) d->failed = 1;
        if (d->role == 3) {
            check(d, unsubscribe(&d->client, "temperatura"));
            puts("[P3] UNSUB temperatura; siguiente publicación solo debe llegar a P1");
            check(d, publish(&d->client, "temperatura", (PubSubValue){.type = TYPE_FLOAT, .data.real = 24.5f}));
            check(d, publish(&d->client, "alarma", (PubSubValue){.type = TYPE_STRING, .data.string = "Hola mundo desde P3"}));
        }
    } else if (!strcmp(topic, "alarma")) {
        d->alarms++;
        if (value->type != TYPE_STRING || strcmp(value->data.string, "Hola mundo desde P3")) d->failed = 1;
        if (d->role == 1) check(d, publish(&d->client, "fin", (PubSubValue){.type = TYPE_INT, .data.integer = 1}));
    }
}
int runDemo(int role, int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc != 2) { fprintf(stderr, "Este proceso debe ser lanzado por ./broker\n"); return 1; }
    char *end;
    long port = strtol(argv[1], &end, 10);
    if (*end || port < 1 || port > 65535) return 1;
    Demo d = {.role = role};
    if (pubsubConnect(&d.client, (unsigned short)port) < 0) { perror("pubsubConnect"); return 1; }
    check(&d, subscribe(&d.client, "fin", onMessage, &d));
    if (role == 1 || role == 2) check(&d, subscribe(&d.client, "estado", onMessage, &d));
    if (role == 1 || role == 3) check(&d, subscribe(&d.client, "temperatura", onMessage, &d));
    if (role == 1 || role == 2) check(&d, subscribe(&d.client, "alarma", onMessage, &d));
    /* Repeating a subscription updates its callback, without duplicate delivery. */
    if (role == 1) check(&d, subscribe(&d.client, "temperatura", onMessage, &d));
    if (!d.failed) check(&d, pubsubReady(&d.client));
    if (!d.failed && role == 1) check(&d, publish(&d.client, "estado", (PubSubValue){.type = TYPE_INT, .data.integer = 42}));
    while (!d.done && !d.failed) check(&d, pubsubDispatch(&d.client));
    if (role == 1 && (d.states != 1 || d.temperatures != 2 || d.alarms != 1)) d.failed = 1;
    if (role == 2 && (d.states != 1 || d.temperatures != 0 || d.alarms != 1)) d.failed = 1;
    if (role == 3 && (d.states != 0 || d.temperatures != 1 || d.alarms != 0)) d.failed = 1;
    pubsubClose(&d.client);
    printf("[P%d] %s\n", role, d.failed ? "ERROR" : "OK");
    return d.failed ? 1 : 0;
}
