ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

CC ?= cc
AR ?= ar
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(ROOT)/include -I$(ROOT)/third_party/cJSON
LDFLAGS ?=

BUILD_DIR := $(ROOT)/build
OBJ_DIR := $(BUILD_DIR)/obj

LIB_SRCS := \
	src/result.c \
	src/framing.c \
	src/envelope.c \
	src/signal_tcp.c \
	third_party/cJSON/cJSON.c

EXAMPLE_SRCS := \
	examples/hexagora_server.c \
	examples/hexagora_client.c

TEST_SRCS := \
	tests/test_framing.c \
	tests/test_envelope.c \
	tests/test_samples.c \
	tests/test_signal_tcp.c

LIB_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(LIB_SRCS))

LIB := $(BUILD_DIR)/libagorahex.a
EXAMPLES := $(BUILD_DIR)/hexagora-server $(BUILD_DIR)/hexagora-client
TEST_BINS := $(BUILD_DIR)/test_framing $(BUILD_DIR)/test_envelope $(BUILD_DIR)/test_samples
TEST_BINS += $(BUILD_DIR)/test_signal_tcp

.PHONY: all clean test

all: $(LIB) $(EXAMPLES)

$(OBJ_DIR)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(BUILD_DIR)/hexagora-server: $(OBJ_DIR)/examples/hexagora_server.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/hexagora-client: $(OBJ_DIR)/examples/hexagora_client.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_framing: $(OBJ_DIR)/tests/test_framing.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_envelope: $(OBJ_DIR)/tests/test_envelope.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_samples: $(OBJ_DIR)/tests/test_samples.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_signal_tcp: $(OBJ_DIR)/tests/test_signal_tcp.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TEST_BINS)
	$(BUILD_DIR)/test_framing
	$(BUILD_DIR)/test_envelope
	cd "$(ROOT)/docs" && ../build/test_samples
	$(BUILD_DIR)/test_signal_tcp

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(ROOT)/src/*.o $(ROOT)/examples/*.o $(ROOT)/tests/*.o $(ROOT)/third_party/cJSON/*.o
