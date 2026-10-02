#include "common.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static Message roundtrip(PubSubValue value) {
    Message original = {.operation = OP_PUB, .topic = "prueba", .value = value}, decoded;
    char line[MESSAGE_SIZE];
    assert(serializeMessage(&original, line, sizeof line) == 0);
    assert(parseMessage(line, &decoded) == 0);
    assert(decoded.operation == OP_PUB && !strcmp(decoded.topic, "prueba") && decoded.value.type == value.type);
    return decoded;
}
int main(void) {
    assert(roundtrip((PubSubValue){.type = TYPE_INT, .data.integer = INT_MIN}).value.data.integer == INT_MIN);
    assert(roundtrip((PubSubValue){.type = TYPE_INT, .data.integer = INT_MAX}).value.data.integer == INT_MAX);
    assert(roundtrip((PubSubValue){.type = TYPE_FLOAT, .data.real = -23.5f}).value.data.real == -23.5f);
    assert(!strcmp(roundtrip((PubSubValue){.type = TYPE_STRING, .data.string = " Hola mundo  "}).value.data.string, " Hola mundo  "));
    assert(!strcmp(roundtrip((PubSubValue){.type = TYPE_STRING, .data.string = ""}).value.data.string, ""));
    PubSubValue long_string = {.type = TYPE_STRING};
    memset(long_string.data.string, 'x', VALUE_SIZE - 1);
    assert(strlen(roundtrip(long_string).value.data.string) == VALUE_SIZE - 1);
    const char *bad[] = {"SUB ", "SUB a b", "UNSUB a b", "PUB x INT 9999999999999999999999", "PUB x INT 1x", "PUB x FLOAT nan", "PUB x FLOAT inf", "PUB x FLOAT 2x", "PUB x BOOL 1", "PUB x INT ", "HELLO", "PUB x STRING a\nb"};
    Message m;
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) assert(parseMessage(bad[i], &m) < 0);
    puts("Protocolo: OK (tipos, límites, cadenas y mensajes inválidos)");
}
