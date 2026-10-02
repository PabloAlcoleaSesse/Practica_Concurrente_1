#include "pubsub_api.h"
#include <string.h>
static int transmit(PubSubClient *c, Operation op, const char *topic, PubSubValue value) {
    Message m = {.operation = op, .value = value};
    char line[MESSAGE_SIZE];
    if (topic) {
        if (strlen(topic) >= TOPIC_SIZE) return -1;
        strcpy(m.topic, topic);
    }
    if (serializeMessage(&m, line, sizeof line) < 0) return -1;
    return sendMessage(&c->connection, line);
}
int pubsubConnect(PubSubClient *c, unsigned short port) {
    memset(c, 0, sizeof *c);
    return communicationConnect(&c->connection, port);
}
int subscribe(PubSubClient *c, const char *topic, MessageCallback callback, void *context) {
    if (!topic || !callback) return -1;
    int slot = -1;
    for (int i = 0; i < MAX_SUBS; i++) {
        if (c->subscriptions[i].callback && !strcmp(c->subscriptions[i].topic, topic)) { slot = i; break; }
        if (!c->subscriptions[i].callback && slot < 0) slot = i;
    }
    if (slot < 0 || transmit(c, OP_SUB, topic, (PubSubValue){0}) < 0) return -1;
    strcpy(c->subscriptions[slot].topic, topic);
    c->subscriptions[slot].callback = callback;
    c->subscriptions[slot].context = context;
    return 0;
}
int unsubscribe(PubSubClient *c, const char *topic) {
    if (!topic || transmit(c, OP_UNSUB, topic, (PubSubValue){0}) < 0) return -1;
    for (int i = 0; i < MAX_SUBS; i++)
        if (c->subscriptions[i].callback && !strcmp(c->subscriptions[i].topic, topic)) c->subscriptions[i].callback = NULL;
    return 0;
}
int publish(PubSubClient *c, const char *topic, PubSubValue value) {
    return topic ? transmit(c, OP_PUB, topic, value) : -1;
}
static int receive(PubSubClient *c, Message *m) {
    char line[MESSAGE_SIZE];
    return receiveMessage(&c->connection, line, sizeof line) == 1 ? parseMessage(line, m) : -1;
}
int pubsubReady(PubSubClient *c) {
    Message m;
    if (transmit(c, OP_READY, NULL, (PubSubValue){0}) < 0 || receive(c, &m) < 0) return -1;
    return m.operation == OP_START ? 0 : -1;
}
int pubsubDispatch(PubSubClient *c) {
    Message m;
    if (receive(c, &m) < 0 || m.operation != OP_MSG) return -1;
    for (int i = 0; i < MAX_SUBS; i++) {
        if (c->subscriptions[i].callback && !strcmp(c->subscriptions[i].topic, m.topic)) {
            c->subscriptions[i].callback(m.topic, &m.value, c->subscriptions[i].context);
            break;
        }
    }
    return 0;
}
void pubsubClose(PubSubClient *c) { communicationClose(&c->connection); }
