#ifndef PUBSUB_API_H
#define PUBSUB_API_H
#include "common.h"
#include "communication.h"
typedef void (*MessageCallback)(const char *topic, const PubSubValue *value, void *context);
typedef struct {
    Connection connection;
    struct { char topic[TOPIC_SIZE]; MessageCallback callback; void *context; } subscriptions[MAX_SUBS];
} PubSubClient;
int pubsubConnect(PubSubClient *client, unsigned short port);
int subscribe(PubSubClient *client, const char *topic, MessageCallback callback, void *context);
int unsubscribe(PubSubClient *client, const char *topic);
int publish(PubSubClient *client, const char *topic, PubSubValue value);
/* Demo barrier: waits until every child has installed its subscriptions. */
int pubsubReady(PubSubClient *client);
/* Receives one publication and invokes its callback synchronously. */
int pubsubDispatch(PubSubClient *client);
void pubsubClose(PubSubClient *client);
#endif
