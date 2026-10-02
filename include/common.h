#ifndef COMMON_H
#define COMMON_H
#define TOPIC_SIZE 128
#define VALUE_SIZE 512
#define MESSAGE_SIZE 768
#define MAX_SUBS 16
#define CHILD_COUNT 3

typedef enum { TYPE_INT, TYPE_FLOAT, TYPE_STRING } Type;
typedef struct {
    Type type;
    union { int integer; float real; char string[VALUE_SIZE]; } data;
} PubSubValue;
typedef enum { OP_SUB, OP_UNSUB, OP_PUB, OP_MSG, OP_READY, OP_START } Operation;
typedef struct { Operation operation; char topic[TOPIC_SIZE]; PubSubValue value; } Message;
int serializeMessage(const Message *message, char *line, unsigned long capacity);
int parseMessage(const char *line, Message *message);
#endif
