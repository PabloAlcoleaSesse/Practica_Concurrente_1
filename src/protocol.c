#include "common.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int validTopic(const char *topic) {
    size_t length = strlen(topic);
    if (!length || length >= TOPIC_SIZE) return 0;
    for (size_t i = 0; i < length; i++) if (isspace((unsigned char)topic[i])) return 0;
    return 1;
}
int serializeMessage(const Message *m, char *line, unsigned long capacity) {
    int count;
    if (m->operation == OP_READY || m->operation == OP_START)
        count = snprintf(line, capacity, "%s", m->operation == OP_READY ? "READY" : "START");
    else {
        if (!validTopic(m->topic)) return -1;
        if (m->operation == OP_SUB || m->operation == OP_UNSUB)
            count = snprintf(line, capacity, "%s %s", m->operation == OP_SUB ? "SUB" : "UNSUB", m->topic);
        else {
            if (m->operation != OP_PUB && m->operation != OP_MSG) return -1;
            const char *op = m->operation == OP_PUB ? "PUB" : "MSG";
            switch (m->value.type) {
            case TYPE_INT: count = snprintf(line, capacity, "%s %s INT %d", op, m->topic, m->value.data.integer); break;
            case TYPE_FLOAT:
                if (!isfinite(m->value.data.real)) return -1;
                count = snprintf(line, capacity, "%s %s FLOAT %.9g", op, m->topic, (double)m->value.data.real); break;
            case TYPE_STRING:
                if (!memchr(m->value.data.string, 0, VALUE_SIZE) || strchr(m->value.data.string, '\n') || strchr(m->value.data.string, '\r')) return -1;
                count = snprintf(line, capacity, "%s %s STRING %s", op, m->topic, m->value.data.string); break;
            default: return -1;
            }
        }
    }
    return count >= 0 && (unsigned long)count < capacity ? 0 : -1;
}
int parseMessage(const char *line, Message *m) {
    memset(m, 0, sizeof *m);
    if (!strcmp(line, "READY")) { m->operation = OP_READY; return 0; }
    if (!strcmp(line, "START")) { m->operation = OP_START; return 0; }
    const char *topic;
    if (!strncmp(line, "SUB ", 4)) { m->operation = OP_SUB; topic = line + 4; }
    else if (!strncmp(line, "UNSUB ", 6)) { m->operation = OP_UNSUB; topic = line + 6; }
    else if (!strncmp(line, "PUB ", 4)) { m->operation = OP_PUB; topic = line + 4; }
    else if (!strncmp(line, "MSG ", 4)) { m->operation = OP_MSG; topic = line + 4; }
    else return -1;
    const char *space = strchr(topic, ' ');
    size_t length = space ? (size_t)(space - topic) : strlen(topic);
    if (!length || length >= TOPIC_SIZE) return -1;
    memcpy(m->topic, topic, length);
    if (!validTopic(m->topic)) return -1;
    if (m->operation == OP_SUB || m->operation == OP_UNSUB) return space ? -1 : 0;
    if (!space) return -1;
    const char *type = space + 1;
    const char *value = strchr(type, ' ');
    if (!value) return -1;
    value++;
    char *end;
    errno = 0;
    if (!strncmp(type, "INT ", 4)) {
        long integer = strtol(value, &end, 10);
        if (end == value || *end || errno || integer < INT_MIN || integer > INT_MAX) return -1;
        m->value.type = TYPE_INT; m->value.data.integer = (int)integer;
    } else if (!strncmp(type, "FLOAT ", 6)) {
        float real = strtof(value, &end);
        if (end == value || *end || errno || !isfinite(real)) return -1;
        m->value.type = TYPE_FLOAT; m->value.data.real = real;
    } else if (!strncmp(type, "STRING ", 7)) {
        if (strlen(value) >= VALUE_SIZE || strchr(value, '\n') || strchr(value, '\r')) return -1;
        m->value.type = TYPE_STRING; strcpy(m->value.data.string, value);
    } else return -1;
    return 0;
}
