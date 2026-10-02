CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g
CPPFLAGS += -Iinclude

# Use the SDK belonging to the selected Xcode toolchain on macOS.
ifeq ($(shell uname -s),Darwin)
CPPFLAGS += -isysroot $(shell xcrun --sdk macosx --show-sdk-path)
endif

TRANSPORT = src/communication.c src/protocol.c
CLIENT = src/demo.c src/pubsub_api.c $(TRANSPORT)
HEADERS = $(wildcard include/*.h)

all: build/broker build/p1 build/p2 build/p3

build:
	mkdir -p $@

build/broker: src/broker.c $(TRANSPORT) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ src/broker.c $(TRANSPORT) $(LDFLAGS) $(LDLIBS)

build/p1 build/p2 build/p3: build/%: src/%.c $(CLIENT) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(CLIENT) $(LDFLAGS) $(LDLIBS)

build/protocol_test: tests/protocol_test.c src/protocol.c include/common.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/protocol_test.c src/protocol.c $(LDFLAGS) $(LDLIBS)

run: all
	cd build && ./broker

check: all build/protocol_test
	./build/protocol_test
	python3 tests/integration_test.py

clean:
	rm -rf build

.PHONY: all run check clean
