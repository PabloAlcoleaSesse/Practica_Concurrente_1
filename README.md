# Práctica 1 — Infraestructura de comunicación Pub/Sub

Sistema en C con un Master/Broker y tres procesos hijos independientes. El Master
abre un servidor TCP en loopback, lanza `p1`, `p2` y `p3` mediante `fork`/`exec`,
gestiona sus suscripciones y reenvía cada publicación a todos sus suscriptores,
incluido el emisor si está suscrito. Al terminar la demostración, recoge a los tres
hijos mediante `waitpid` y devuelve un código de salida que indica el resultado.

## Compilar y ejecutar

Requisitos: Linux o macOS, compilador C11 y Make. Las pruebas de integración
requieren Python 3. Ejecutar desde la raíz del proyecto:

```sh
make
make run
make check
make clean
```

No hay que arrancar los hijos a mano. El sistema operativo asigna un puerto libre,
que el Master pasa a los hijos como argumento. En macOS, Make selecciona el SDK
con `xcrun`. La ejecución requiere permiso para abrir sockets TCP locales.

## Estructura del proyecto

```text
.
├── include/       # Cabeceras de comunicación, protocolo, Pub/Sub y demo
├── src/           # Implementación del broker, las capas y los tres hijos
├── tests/         # Pruebas de protocolo e integración
├── build/         # Ejecutables y símbolos generados (ignorado por Git)
├── Makefile
└── README.md
```

`make` crea `build/`; `make run` ejecuta el broker desde esa carpeta para que
encuentre a los hijos. `make clean` elimina todos los archivos generados.
Para ejecutar manualmente: `cd build && ./broker`.

## Capas

| Capa | Archivos | Responsabilidad |
| --- | --- | --- |
| Aplicación | `src/p1.c`, `src/p2.c`, `src/p3.c`, `src/demo.c` | Roles de los hijos y callbacks; solo usa la API Pub/Sub |
| Pub/Sub | `include/pubsub_api.h`, `src/pubsub_api.c` | `subscribe`, `unsubscribe`, `publish` y despacho de callbacks |
| Protocolo | `include/common.h`, `src/protocol.c` | Valores tipados, serialización y validación de texto |
| Comunicación | `include/communication.h`, `src/communication.c` | Conexión, aceptación, envío completo, recepción por líneas, espera y cierre |
| Master/Broker | `src/broker.c` | Lanzamiento de hijos, tabla de suscripciones y distribución por tópico |

Las llamadas `socket`, `connect`, `bind`, `listen`, `accept`, `send` y `recv`
solo aparecen en `src/communication.c`. No se transmiten punteros ni estructuras C
binarias. Se utiliza un bucle de eventos con `poll` encapsulado en comunicación;
no hacen falta hilos ni mutex en esta parte.

## API de aplicación

```c
static void onTemperature(const char *topic, const PubSubValue *value, void *context) {
    (void)topic;
    (void)context;
    if (value->type == TYPE_FLOAT)
        printf("Temperatura: %.1f\n", (double)value->data.real);
}

PubSubClient client;
/* Comprobar los códigos de retorno, como hace demo.c. */
pubsubConnect(&client, port);
subscribe(&client, "temperatura", onTemperature, NULL);
publish(&client, "temperatura",
        (PubSubValue){.type = TYPE_FLOAT, .data.real = 23.5f});
pubsubDispatch(&client);
unsubscribe(&client, "temperatura");
pubsubClose(&client);
```

Las operaciones devuelven `0` si tienen éxito y `-1` si fallan. `publish` recibe
el tópico y su valor tipado; la aplicación nunca construye mensajes del protocolo.
Los otros tipos son `TYPE_INT` (`data.integer`) y `TYPE_STRING` (`data.string`).
`subscribe` permite varios tópicos; repetir un tópico actualiza el callback sin
crear entregas duplicadas. Cancelar un tópico inexistente es inocuo.

`pubsubDispatch` espera una publicación y ejecuta el callback en el mismo hilo.
El valor recibido solo es válido durante el callback; se debe copiar si se desea
conservarlo. Un callback puede publicar o cancelar una suscripción. El envío
correcto significa que se ha escrito el mensaje en la conexión, no que todos los
suscriptores ya lo hayan procesado. No hay confirmaciones de entrega.

## Protocolo

Una línea por mensaje, terminada en `\n`:

```text
SUB temperatura
UNSUB temperatura
PUB estado INT 42
PUB temperatura FLOAT 23.5
PUB alarma STRING Hola mundo desde P3
MSG temperatura FLOAT 23.5
```

`MSG` es la publicación reenviada por el broker y conserva tópico, tipo y valor.
Los tópicos admiten hasta 127 bytes sin espacios en blanco. Las cadenas admiten
hasta 511 bytes, pueden estar vacías o contener espacios y no pueden contener
saltos de línea, retornos de carro ni bytes NUL. Hay un máximo de 16 suscripciones
por cliente. Los enteros deben caber en `int`; los valores FLOAT deben ser finitos
y representables por `float`. Se rechazan comandos y valores mal formados.

La comunicación maneja envíos parciales y delimita las líneas aunque TCP fragmente
o agrupe los datos. La espera de entrada tiene un límite de 10 segundos para que
la demostración falle en vez de quedar esperando indefinidamente.

## Demostración y comprobaciones

1. P1 se suscribe a `estado`, `temperatura`, `alarma` y `fin`; P2 a `estado`,
   `alarma` y `fin`; P3 a `temperatura` y `fin`.
2. Cada hijo llama a `pubsubReady`. Esta barrera auxiliar envía `READY`; cuando el
   broker ha procesado las suscripciones de los tres, responde `START`. Evita
   depender de `sleep` para coordinar el ejemplo.
3. P1 publica `estado INT 42`: lo reciben P1 y P2.
4. P2 responde con `temperatura FLOAT 23.5`: la reciben P1 y P3.
5. P3 cancela `temperatura` y publica `temperatura FLOAT 24.5`: solo la recibe P1.
6. P3 publica `alarma STRING Hola mundo desde P3`: la reciben P1 y P2.
7. P1 publica `fin INT 1`: los tres comprueban sus resultados y terminan.

`make check` verifica el protocolo (límites, cadenas con espacios y vacías, tipos
y entradas inválidas) y ejecuta cinco veces la demostración. Comprueba las
cantidades de destinatarios en el broker, los valores y recuentos recibidos,
la autoentrega, las suscripciones duplicadas y la cancelación. Los tres procesos
deben mostrar `OK` y el Master debe finalizar con éxito.

## Alcance

Esta parte no implementa reconexión, persistencia, QoS, tolerancia a fallos,
concurrencia avanzada ni gestión sofisticada de desconexiones. Los límites y la
barrera están pensados para una demostración de tres procesos.
