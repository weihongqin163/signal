# AVC Capacity Indication Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add strict envelope support for `AVCCapacityIndication`, enrich its peer endpoint on the TCP server, and notify server applications when registered clients disconnect.

**Architecture:** Keep JSON ownership and validation in `envelope.c`. Add transport-only peer enrichment and disconnect lifecycle handling inside `signal_tcp.c`, using the existing message callback plus one new nullable disconnect callback argument. Centralize registered-client teardown in one helper so socket close, connection count updates, and exactly-once notification cannot diverge across read, poll, and send paths.

**Tech Stack:** C11, cJSON, POSIX IPv4 sockets, existing framing decoder, Make.

---

## File Map

- Modify `include/agorahex/types.h`: public AVC capacity payload structure.
- Modify `include/agorahex/envelope.h`: append the new message kind and union member.
- Modify `src/envelope.c`: kind mapping, strict required-field parsing, optional endpoint parsing, serialization, and cleanup.
- Modify `include/agorahex/signal_tcp.h`: disconnect reason enum, callback type, and source-incompatible `agorahex_signal_start()` signature.
- Modify `src/signal_tcp.c`: nullable disconnect callback storage, centralized client teardown, reason classification, and capacity peer enrichment.
- Modify `tests/test_envelope.c`: capacity parser, serializer, validation, and no-business-constraint tests.
- Modify `tests/test_signal_tcp.c`: capacity peer enrichment, original JSON preservation, and orderly close notification.
- Create `tests/test_signal_disconnect.c`: protocol-error, send-error, exactly-once, nullable-callback, and explicit-close behavior.
- Modify `examples/hexagora_client.c` and `examples/hexagora_server.c`: pass the new callback argument and demonstrate server disconnect logging.
- Modify `Makefile`: build and run the new disconnect test.
- Modify `docs/json_msg/AVCCapacityIndication.txt`: use a valid identifier no longer than 64 UTF-8 bytes and track the sample.
- Modify `CHANGELOG.md`: replace the empty `Unreleased` entry with the new protocol and disconnect callback summary.

### Task 1: Add The Capacity Envelope Model And Validation

**Files:**
- Modify: `tests/test_envelope.c`
- Modify: `include/agorahex/types.h`
- Modify: `include/agorahex/envelope.h`
- Modify: `src/envelope.c`

- [ ] **Step 1: Write failing envelope tests**

Add a focused test function and call it from `main()`:

```c
static int test_avc_capacity_indication(void) {
    const char *complete =
        "{\"AVCCapacityIndication\":{\"maxCapacity\":10,\"curCapacity\":1,"
        "\"addr\":\"192.0.2.10\",\"port\":8080,\"identifier\":\"avc-process-1\"}}";
    agorahex_message_t m;
    char *json = NULL;
    size_t json_len = 0;

    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(complete, strlen(complete), &m) != AGORAHEX_OK ||
        m.kind != AGORAHEX_KIND_AVC_CAPACITY_INDICATION ||
        m.u.avc_capacity_indication.max_capacity != 10 ||
        m.u.avc_capacity_indication.cur_capacity != 1 ||
        !m.u.avc_capacity_indication.addr ||
        strcmp(m.u.avc_capacity_indication.addr, "192.0.2.10") != 0 ||
        m.u.avc_capacity_indication.port != 8080 ||
        !m.u.avc_capacity_indication.identifier ||
        strcmp(m.u.avc_capacity_indication.identifier, "avc-process-1") != 0) {
        agorahex_message_free(&m);
        return fail("parse AVC capacity indication");
    }
    if (strcmp(agorahex_kind_cstr(m.kind), "AVCCapacityIndication") != 0 ||
        agorahex_marshal_envelope(&m, &json, &json_len) != AGORAHEX_OK || !json) {
        agorahex_message_free(&m);
        free(json);
        return fail("marshal AVC capacity indication");
    }
    agorahex_message_free(&m);
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(json, json_len, &m) != AGORAHEX_OK ||
        m.u.avc_capacity_indication.max_capacity != 10 ||
        m.u.avc_capacity_indication.cur_capacity != 1) {
        free(json);
        agorahex_message_free(&m);
        return fail("roundtrip AVC capacity indication");
    }
    free(json);
    agorahex_message_free(&m);

    const char *minimal =
        "{\"AVCCapacityIndication\":{\"maxCapacity\":-1,\"curCapacity\":20,"
        "\"identifier\":\"avc-2\"}}";
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(minimal, strlen(minimal), &m) != AGORAHEX_OK ||
        m.u.avc_capacity_indication.max_capacity != -1 ||
        m.u.avc_capacity_indication.cur_capacity != 20 ||
        m.u.avc_capacity_indication.addr != NULL || m.u.avc_capacity_indication.port != 0) {
        agorahex_message_free(&m);
        return fail("minimal AVC capacity indication");
    }
    if (agorahex_marshal_envelope(&m, &json, &json_len) != AGORAHEX_OK ||
        strstr(json, "\"addr\"") || strstr(json, "\"port\"")) {
        agorahex_message_free(&m);
        free(json);
        return fail("omit missing AVC capacity endpoint");
    }
    agorahex_message_free(&m);
    free(json);

    const char *ignored_optional =
        "{\"AVCCapacityIndication\":{\"maxCapacity\":3,\"curCapacity\":2,"
        "\"addr\":42,\"port\":1.5,\"identifier\":\"avc-3\"}}";
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(ignored_optional, strlen(ignored_optional), &m) != AGORAHEX_OK ||
        m.u.avc_capacity_indication.addr != NULL || m.u.avc_capacity_indication.port != 0) {
        agorahex_message_free(&m);
        return fail("ignore invalid optional AVC capacity endpoint");
    }
    agorahex_message_free(&m);

    memset(&m, 0, sizeof m);
    m.kind = AGORAHEX_KIND_AVC_CAPACITY_INDICATION;
    m.u.avc_capacity_indication.identifier = "";
    if (agorahex_marshal_envelope(&m, &json, &json_len) != AGORAHEX_ERR_INVALID_ARG) {
        free(json);
        return fail("marshal invalid AVC capacity identifier");
    }
    return 0;
}
```

Add a table-driven validation test. Every string below must return `AGORAHEX_ERR_JSON_PARSE`; the last entry uses 65 ASCII bytes, which are also 65 UTF-8 bytes:

```c
static int test_avc_capacity_required_field_validation(void) {
    static const char *invalid[] = {
        "{\"AVCCapacityIndication\":{\"curCapacity\":1,\"identifier\":\"avc\"}}",
        "{\"AVCCapacityIndication\":{\"maxCapacity\":10,\"identifier\":\"avc\"}}",
        "{\"AVCCapacityIndication\":{\"maxCapacity\":1.5,\"curCapacity\":1,\"identifier\":\"avc\"}}",
        "{\"AVCCapacityIndication\":{\"maxCapacity\":10,\"curCapacity\":2147483648,\"identifier\":\"avc\"}}",
        "{\"AVCCapacityIndication\":{\"maxCapacity\":10,\"curCapacity\":1}}",
        "{\"AVCCapacityIndication\":{\"maxCapacity\":10,\"curCapacity\":1,\"identifier\":\"\"}}",
        "{\"AVCCapacityIndication\":{\"maxCapacity\":10,\"curCapacity\":1,"
        "\"identifier\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}}",
    };
    agorahex_message_t m;
    size_t i;

    for (i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        memset(&m, 0, sizeof m);
        if (agorahex_parse_envelope(invalid[i], strlen(invalid[i]), &m) != AGORAHEX_ERR_JSON_PARSE) {
            agorahex_message_free(&m);
            return fail("accept invalid AVC capacity indication");
        }
    }
    return 0;
}
```

- [ ] **Step 2: Run the tests to verify RED**

Run: `make test`

Expected: compilation fails because `AGORAHEX_KIND_AVC_CAPACITY_INDICATION` and `avc_capacity_indication` do not exist.

- [ ] **Step 3: Add the public payload and append-only kind**

Add to `include/agorahex/types.h`:

```c
typedef struct agorahex_avc_capacity_indication {
    int max_capacity;
    int cur_capacity;
    char *addr;
    int port;
    char *identifier;
} agorahex_avc_capacity_indication_t;
```

Append, without reordering existing values, in `include/agorahex/envelope.h`:

```c
AGORAHEX_KIND_AGORA_DIAL_OUT_REPLY,
AGORAHEX_KIND_AVC_CAPACITY_INDICATION,
```

Add the union member:

```c
agorahex_avc_capacity_indication_t avc_capacity_indication;
```

- [ ] **Step 4: Implement strict integer parsing and capacity envelope handling**

Add one reusable integer helper near `dup_or_null()` in `src/envelope.c`:

```c
static int parse_json_int(const cJSON *item, int *out) {
    double value;
    int parsed;

    if (!cJSON_IsNumber(item) || !out) {
        return 0;
    }
    value = cJSON_GetNumberValue(item);
    if (value < (double)INT_MIN || value > (double)INT_MAX) {
        return 0;
    }
    parsed = (int)value;
    if (value != (double)parsed) {
        return 0;
    }
    *out = parsed;
    return 1;
}
```

Add cleanup and kind mappings:

```c
case AGORAHEX_KIND_AVC_CAPACITY_INDICATION:
    free(m->u.avc_capacity_indication.addr);
    free(m->u.avc_capacity_indication.identifier);
    memset(&m->u.avc_capacity_indication, 0, sizeof(m->u.avc_capacity_indication));
    break;
```

```c
case AGORAHEX_KIND_AVC_CAPACITY_INDICATION:
    return "AVCCapacityIndication";
```

```c
if (strcmp(k, "AVCCapacityIndication") == 0) {
    return AGORAHEX_KIND_AVC_CAPACITY_INDICATION;
}
```

Add this `parse_kind_body()` case:

```c
case AGORAHEX_KIND_AVC_CAPACITY_INDICATION: {
    agorahex_avc_capacity_indication_t *x = &out->u.avc_capacity_indication;
    const cJSON *max_capacity = cJSON_GetObjectItemCaseSensitive(body, "maxCapacity");
    const cJSON *cur_capacity = cJSON_GetObjectItemCaseSensitive(body, "curCapacity");
    const cJSON *identifier = cJSON_GetObjectItemCaseSensitive(body, "identifier");
    const cJSON *addr = cJSON_GetObjectItemCaseSensitive(body, "addr");
    const cJSON *port = cJSON_GetObjectItemCaseSensitive(body, "port");
    const char *identifier_value;
    size_t identifier_len;

    if (!parse_json_int(max_capacity, &x->max_capacity) ||
        !parse_json_int(cur_capacity, &x->cur_capacity) || !cJSON_IsString(identifier)) {
        return AGORAHEX_ERR_JSON_PARSE;
    }
    identifier_value = cJSON_GetStringValue(identifier);
    identifier_len = identifier_value ? strlen(identifier_value) : 0u;
    if (identifier_len == 0u || identifier_len > 64u) {
        return AGORAHEX_ERR_JSON_PARSE;
    }
    x->identifier = dup_or_null(identifier_value);
    if (!x->identifier) {
        return AGORAHEX_ERR_NO_MEMORY;
    }
    if (cJSON_IsString(addr)) {
        x->addr = dup_or_null(cJSON_GetStringValue(addr));
        if (!x->addr) {
            return AGORAHEX_ERR_NO_MEMORY;
        }
    }
    (void)parse_json_int(port, &x->port);
    return AGORAHEX_OK;
}
```

Add this serializer case. Returning `AGORAHEX_ERR_INVALID_ARG` for an invalid in-memory identifier prevents emitting a wire message the parser would reject:

```c
case AGORAHEX_KIND_AVC_CAPACITY_INDICATION: {
    const agorahex_avc_capacity_indication_t *x = &msg->u.avc_capacity_indication;
    size_t identifier_len = x->identifier ? strlen(x->identifier) : 0u;

    if (identifier_len == 0u || identifier_len > 64u) {
        cJSON_Delete(root);
        return AGORAHEX_ERR_INVALID_ARG;
    }
    body = cJSON_CreateObject();
    if (!body || !cJSON_AddNumberToObject(body, "maxCapacity", (double)x->max_capacity) ||
        !cJSON_AddNumberToObject(body, "curCapacity", (double)x->cur_capacity) ||
        !cJSON_AddStringToObject(body, "identifier", x->identifier)) {
        cJSON_Delete(body);
        body = NULL;
        break;
    }
    if (x->addr && !cJSON_AddStringToObject(body, "addr", x->addr)) {
        cJSON_Delete(body);
        body = NULL;
        break;
    }
    if (x->port != 0 && !cJSON_AddNumberToObject(body, "port", (double)x->port)) {
        cJSON_Delete(body);
        body = NULL;
    }
    break;
}
```

- [ ] **Step 5: Run the complete test suite to verify GREEN**

Run: `make test`

Expected: all existing tests and the new envelope tests pass.

- [ ] **Step 6: Commit the envelope layer**

```bash
git add include/agorahex/types.h include/agorahex/envelope.h src/envelope.c tests/test_envelope.c
git commit -m "feat: add AVC capacity envelope"
```

### Task 2: Add The Server Disconnect Callback API And Orderly-Close Behavior

**Files:**
- Modify: `include/agorahex/signal_tcp.h`
- Modify: `src/signal_tcp.c`
- Modify: `tests/test_signal_tcp.c`
- Modify: `examples/hexagora_client.c`
- Modify: `examples/hexagora_server.c`

- [ ] **Step 1: Extend the TCP test with an orderly disconnect assertion**

Add state and a server callback in `tests/test_signal_tcp.c`:

```c
static int g_server_disconnected;
static int g_server_disconnected_fd = AGORAHEX_SIGNAL_BROADCAST_FD;
static agorahex_signal_disconnect_reason_t g_server_disconnect_reason;

static void server_disconnect_cb(int fd, agorahex_signal_disconnect_reason_t reason) {
    g_server_disconnected++;
    g_server_disconnected_fd = fd;
    g_server_disconnect_reason = reason;
}
```

Pass `server_disconnect_cb` to the server start call and `NULL` to client/invalid-argument calls. Change `run_server()` so it returns success only after the reply has been sent and exactly one disconnect arrives:

```c
if (g_server_reply_sent && g_server_disconnected == 1) {
    if (g_server_disconnected_fd != g_server_peer_fd ||
        g_server_disconnect_reason != AGORAHEX_SIGNAL_DISCONNECT_PEER_CLOSED) {
        agorahex_signal_close();
        return 1;
    }
    agorahex_signal_close();
    return 0;
}
```

The parent already calls `agorahex_signal_close()` after receiving the reply, causing an orderly FIN for the child server to observe.

- [ ] **Step 2: Run the tests to verify RED**

Run: `make test`

Expected: compilation fails because the disconnect enum, callback type, and fifth `agorahex_signal_start()` argument do not exist.

- [ ] **Step 3: Add the public disconnect API**

Add to `include/agorahex/signal_tcp.h`:

```c
typedef enum agorahex_signal_disconnect_reason {
    AGORAHEX_SIGNAL_DISCONNECT_PEER_CLOSED = 0,
    AGORAHEX_SIGNAL_DISCONNECT_IO_ERROR,
    AGORAHEX_SIGNAL_DISCONNECT_PROTOCOL_ERROR,
} agorahex_signal_disconnect_reason_t;

typedef void (*agorahex_signal_disconnect_cb_t)(int fd, agorahex_signal_disconnect_reason_t reason);
```

Change the declaration and documentation to:

```c
int agorahex_signal_start(int server_mode, char *server_ipv4_addr, int tcp_port,
                          agorahex_signal_cb_t cb, agorahex_signal_disconnect_cb_t disconnect_cb);
```

Document that the callback is server-only, nullable, synchronous, called after socket close/removal, and receives an invalidated fd only as a mapping key.

- [ ] **Step 4: Centralize registered-client teardown**

Add `disconnect_cb` to `agorahex_signal_server_t`, clear it in `runtime_reset()` and `close_server_mode()`, and pass it through `start_server()` and `agorahex_signal_start()`.

Add this helper beside `conn_reset()`:

```c
static void server_disconnect_client(agorahex_signal_conn_t *conn,
                                     agorahex_signal_disconnect_reason_t reason) {
    int fd;

    if (!conn || conn->fd < 0 || conn->state != AGORAHEX_SIGNAL_CONN_CONNECTED) {
        return;
    }
    fd = conn->fd;
    conn_reset(conn);
    if (g_runtime.server.client_count > 0) {
        g_runtime.server.client_count--;
    }
    if (g_runtime.server.disconnect_cb) {
        g_runtime.server.disconnect_cb(fd, reason);
    }
}
```

Do not use this helper from `close_server_mode()` or max-client rejection; those paths deliberately produce no callback.

Change `handle_server_readable()` to return a disconnect reason separately:

```c
static int handle_server_readable(agorahex_signal_conn_t *conn,
                                  agorahex_signal_disconnect_reason_t *reason) {
    uint8_t buf[32u << 10u];
    agorahex_signal_dispatch_ctx_t ctx;

    ctx.fd = conn->fd;
    ctx.cb = g_runtime.server.cb;
    ctx.server_mode = 1;
    for (;;) {
        ssize_t n = recv(conn->fd, buf, sizeof buf, 0);
        if (n > 0) {
            agorahex_result_t r = agorahex_frame_decoder_append(&conn->decoder, buf, (size_t)n, &ctx,
                                                                 on_signal_frame);
            if (r != AGORAHEX_OK) {
                *reason = (r == AGORAHEX_ERR_NO_MEMORY || r == AGORAHEX_ERR_IO)
                              ? AGORAHEX_SIGNAL_DISCONNECT_IO_ERROR
                              : AGORAHEX_SIGNAL_DISCONNECT_PROTOCOL_ERROR;
                return (int)r;
            }
            continue;
        }
        if (n == 0) {
            *reason = AGORAHEX_SIGNAL_DISCONNECT_PEER_CLOSED;
            return AGORAHEX_ERR_IO;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return AGORAHEX_OK;
        }
        *reason = AGORAHEX_SIGNAL_DISCONNECT_IO_ERROR;
        return AGORAHEX_ERR_IO;
    }
}
```

Add `int server_mode;` to `agorahex_signal_dispatch_ctx_t`; set it to `1` in the server read path and `0` in the client read path. Task 4 will use this flag for capacity peer enrichment.

In `handle_server_poll()`, replace direct client `conn_reset()` and count updates with `server_disconnect_client()`. When readable handling fails, use its returned reason. For client `POLLERR | POLLHUP | POLLNVAL`, call the helper with `IO_ERROR` only if the connection is still registered; this guard provides exactly-once notification when `POLLIN` and `POLLHUP` arrive together.

- [ ] **Step 5: Update all start call sites**

Use these arguments:

```c
agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, server_ipv4_addr, port, server_cb,
                      server_disconnect_cb);
agorahex_signal_start(AGORAHEX_SIGNAL_CLIENT_MODE, server_ipv4_addr, port, client_cb, NULL);
```

Add a concise server example callback:

```c
static void server_disconnect_cb(int fd, agorahex_signal_disconnect_reason_t reason) {
    fprintf(stderr, "client disconnected fd=%d reason=%d\n", fd, (int)reason);
}
```

- [ ] **Step 6: Run the complete test suite to verify GREEN**

Run: `make test`

Expected: all tests pass; the TCP test receives one `PEER_CLOSED` callback after the client closes.

- [ ] **Step 7: Commit the callback API and orderly close behavior**

```bash
git add include/agorahex/signal_tcp.h src/signal_tcp.c tests/test_signal_tcp.c examples/hexagora_client.c examples/hexagora_server.c
git commit -m "feat: notify server of client disconnects"
```

### Task 3: Cover Protocol And Send Failure Disconnect Paths

**Files:**
- Create: `tests/test_signal_disconnect.c`
- Modify: `Makefile`
- Modify: `src/signal_tcp.c`

- [ ] **Step 1: Add a standalone raw-client disconnect regression binary**

Create `tests/test_signal_disconnect.c` with three local-server scenarios. Use a blocking raw IPv4 client socket so the test controls FIN, malformed frames, and RST independently of the library client runtime. The shared callbacks and connection helper are:

```c
#define _POSIX_C_SOURCE 200809L

#include "agorahex/framing.h"
#include "agorahex/result.h"
#include "agorahex/signal_tcp.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int g_message_fd = -1;
static int g_disconnect_count;
static int g_disconnect_fd = -1;
static agorahex_signal_disconnect_reason_t g_disconnect_reason;

static void on_message(int fd, const void *json, int len, agorahex_message_t *msg) {
    (void)json;
    (void)len;
    (void)msg;
    g_message_fd = fd;
}

static void on_disconnect(int fd, agorahex_signal_disconnect_reason_t reason) {
    g_disconnect_count++;
    g_disconnect_fd = fd;
    g_disconnect_reason = reason;
}

static void reset_observations(void) {
    g_message_fd = -1;
    g_disconnect_count = 0;
    g_disconnect_fd = -1;
    g_disconnect_reason = AGORAHEX_SIGNAL_DISCONNECT_PEER_CLOSED;
}

static int connect_raw_client(int port) {
    struct sockaddr_in addr;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int poll_until(int *value, int expected) {
    int attempts;
    for (attempts = 0; attempts < 100; attempts++) {
        if (*value == expected) {
            return 0;
        }
        if (agorahex_signal_poll(20) < 0) {
            return -1;
        }
    }
    return (*value == expected) ? 0 : -1;
}

static int poll_until_message(void) {
    int attempts;
    for (attempts = 0; attempts < 100; attempts++) {
        if (g_message_fd >= 0) {
            return 0;
        }
        if (agorahex_signal_poll(20) < 0) {
            return -1;
        }
    }
    return g_message_fd >= 0 ? 0 : -1;
}
```

Scenario A sends a framed invalid JSON payload and expects one `PROTOCOL_ERROR`:

```c
static int test_protocol_disconnect(int port) {
    static const uint8_t invalid_json[] = "not-json";
    uint8_t frame[AGORAHEX_FRAME_HEADER_SIZE + sizeof invalid_json - 1u];
    int client_fd;

    reset_observations();
    if (agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, "127.0.0.1", port,
                              on_message, on_disconnect) != AGORAHEX_OK) {
        return -1;
    }
    client_fd = connect_raw_client(port);
    if (client_fd < 0) {
        agorahex_signal_close();
        return -1;
    }
    agorahex_frame_encode(invalid_json, sizeof invalid_json - 1u, frame);
    if (send(client_fd, frame, sizeof frame, 0) != (ssize_t)sizeof frame ||
        poll_until(&g_disconnect_count, 1) != 0 ||
        g_disconnect_reason != AGORAHEX_SIGNAL_DISCONNECT_PROTOCOL_ERROR) {
        close(client_fd);
        agorahex_signal_close();
        return -1;
    }
    close(client_fd);
    agorahex_signal_poll(0);
    agorahex_signal_close();
    return g_disconnect_count == 1 ? 0 : -1;
}
```

Scenario B sends a valid envelope to capture the server fd, closes the raw client with an RST, then retries server send until the kernel reports failure. It expects `IO_ERROR` exactly once:

```c
static int test_send_disconnect(int port) {
    static const char ping[] = "{\"HangupIndication\":{\"callId\":\"x\",\"dropCode\":0}}";
    static const char pong[] = "{\"MutedIndication\":{\"callId\":\"x\",\"muted\":true}}";
    uint8_t frame[AGORAHEX_FRAME_HEADER_SIZE + sizeof ping - 1u];
    struct linger rst = {1, 0};
    struct timespec wait = {0, 10000000L};
    int client_fd;
    int attempts;

    reset_observations();
    if (agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, "127.0.0.1", port,
                              on_message, on_disconnect) != AGORAHEX_OK) {
        return -1;
    }
    client_fd = connect_raw_client(port);
    if (client_fd < 0) {
        agorahex_signal_close();
        return -1;
    }
    agorahex_frame_encode((const uint8_t *)ping, sizeof ping - 1u, frame);
    if (send(client_fd, frame, sizeof frame, 0) != (ssize_t)sizeof frame ||
        poll_until_message() != 0) {
        close(client_fd);
        agorahex_signal_close();
        return -1;
    }
    setsockopt(client_fd, SOL_SOCKET, SO_LINGER, &rst, sizeof rst);
    close(client_fd);
    for (attempts = 0; attempts < 100 && g_disconnect_count == 0; attempts++) {
        if (agorahex_signal_send(g_message_fd, pong, (int)(sizeof pong - 1u)) == AGORAHEX_ERR_IO) {
            break;
        }
        nanosleep(&wait, NULL);
    }
    if (g_disconnect_count != 1 || g_disconnect_fd != g_message_fd ||
        g_disconnect_reason != AGORAHEX_SIGNAL_DISCONNECT_IO_ERROR) {
        agorahex_signal_close();
        return -1;
    }
    agorahex_signal_poll(0);
    agorahex_signal_close();
    return g_disconnect_count == 1 ? 0 : -1;
}
```

Scenario C verifies a nullable callback and explicit-close suppression:

```c
static int test_null_callback_and_explicit_close(int port) {
    int client_fd;
    int attempts;

    reset_observations();
    if (agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, "127.0.0.1", port,
                              on_message, NULL) != AGORAHEX_OK) {
        return -1;
    }
    client_fd = connect_raw_client(port);
    if (client_fd < 0) {
        agorahex_signal_close();
        return -1;
    }
    for (attempts = 0; attempts < 5; attempts++) {
        agorahex_signal_poll(20);
    }
    close(client_fd);
    for (attempts = 0; attempts < 5; attempts++) {
        agorahex_signal_poll(20);
    }
    agorahex_signal_close();

    reset_observations();
    if (agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, "127.0.0.1", port + 1,
                              on_message, on_disconnect) != AGORAHEX_OK) {
        return -1;
    }
    client_fd = connect_raw_client(port + 1);
    if (client_fd < 0) {
        agorahex_signal_close();
        return -1;
    }
    for (attempts = 0; attempts < 5; attempts++) {
        agorahex_signal_poll(20);
    }
    agorahex_signal_close();
    close(client_fd);
    return g_disconnect_count == 0 ? 0 : -1;
}

int main(void) {
    int base_port = 30000 + (int)(getpid() % 10000);

    signal(SIGPIPE, SIG_IGN);
    if (test_protocol_disconnect(base_port) != 0) {
        fprintf(stderr, "FAIL: protocol disconnect callback\n");
        return 1;
    }
    if (test_send_disconnect(base_port + 1) != 0) {
        fprintf(stderr, "FAIL: send disconnect callback\n");
        return 1;
    }
    if (test_null_callback_and_explicit_close(base_port + 2) != 0) {
        fprintf(stderr, "FAIL: null callback or explicit close\n");
        return 1;
    }
    return 0;
}
```

- [ ] **Step 2: Add the test target and verify RED**

In `Makefile`, append the source and binary, add its link rule, and execute it from `test`:

```make
TEST_SRCS := \
	tests/test_framing.c \
	tests/test_envelope.c \
	tests/test_samples.c \
	tests/test_signal_tcp.c \
	tests/test_signal_disconnect.c

TEST_BINS := $(BUILD_DIR)/test_framing $(BUILD_DIR)/test_envelope $(BUILD_DIR)/test_samples
TEST_BINS += $(BUILD_DIR)/test_signal_tcp $(BUILD_DIR)/test_signal_disconnect

$(BUILD_DIR)/test_signal_disconnect: $(OBJ_DIR)/tests/test_signal_disconnect.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
```

Add this final command to the `test` recipe:

```make
	$(BUILD_DIR)/test_signal_disconnect
```

Run: `make test`

Expected: `test_signal_disconnect` fails because server send currently resets a connection without invoking the callback, and protocol/read paths do not classify disconnect reasons.

- [ ] **Step 3: Make send failure notification exact and non-duplicating**

Rename `write_all_or_close()` to `write_all()` and remove its `conn_reset(conn)` side effect:

```c
static int write_all(agorahex_signal_conn_t *conn, const uint8_t *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
#ifdef MSG_NOSIGNAL
        ssize_t n = send(conn->fd, buf + off, len - off, MSG_NOSIGNAL);
#else
        ssize_t n = send(conn->fd, buf + off, len - off, 0);
#endif
        if (n > 0) {
            off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return AGORAHEX_ERR_IO;
    }
    return AGORAHEX_OK;
}
```

In `server_send_one()` and `server_send_all()`, call `server_disconnect_client(conn, AGORAHEX_SIGNAL_DISCONNECT_IO_ERROR)` after `write_all()` fails. Do not decrement `client_count` separately. In client mode, retain existing behavior by calling `conn_reset()` after `write_all()` fails, without invoking a callback.

- [ ] **Step 4: Verify protocol and poll reason mapping**

Ensure `handle_server_poll()` uses the reason returned by `handle_server_readable()`. A poll error on a still-registered client uses `AGORAHEX_SIGNAL_DISCONNECT_IO_ERROR`. A listening-socket error still calls `close_server_mode()` directly and therefore emits no client callbacks.

- [ ] **Step 5: Run the disconnect binary and full suite to verify GREEN**

Run: `make test`

Expected: framing, envelope, samples, TCP round-trip, and all disconnect scenarios pass with no duplicate notifications.

- [ ] **Step 6: Commit the remaining disconnect paths**

```bash
git add Makefile src/signal_tcp.c tests/test_signal_disconnect.c
git commit -m "test: cover signal disconnect reasons"
```

### Task 4: Enrich Capacity Messages With The Actual TCP Peer

**Files:**
- Modify: `tests/test_signal_tcp.c`
- Modify: `src/signal_tcp.c`

- [ ] **Step 1: Change the TCP integration payload to a forged capacity indication**

Replace `g_ping` with:

```c
static const char g_ping[] =
    "{\"AVCCapacityIndication\":{\"maxCapacity\":10,\"curCapacity\":1,"
    "\"addr\":\"203.0.113.77\",\"port\":1,\"identifier\":\"tcp-test-avc\"}}";
```

Update `server_cb()` to prove parsed enrichment and raw JSON preservation:

```c
static void server_cb(int fd, const void *json, int len, agorahex_message_t *msg_t) {
    const agorahex_avc_capacity_indication_t *capacity;

    if (fd < 0 || !msg_t || msg_t->kind != AGORAHEX_KIND_AVC_CAPACITY_INDICATION ||
        len != (int)(sizeof g_ping - 1u) || memcmp(json, g_ping, (size_t)len) != 0) {
        return;
    }
    capacity = &msg_t->u.avc_capacity_indication;
    if (!capacity->addr || strcmp(capacity->addr, "127.0.0.1") != 0 ||
        capacity->port <= 0 || capacity->port == 1 ||
        !capacity->identifier || strcmp(capacity->identifier, "tcp-test-avc") != 0) {
        return;
    }
    g_server_peer_fd = fd;
    g_server_received = 1;
}
```

- [ ] **Step 2: Run the tests to verify RED**

Run: `make test`

Expected: the TCP test times out because parsed `addr` remains `203.0.113.77` and `port` remains `1`.

- [ ] **Step 3: Implement server-only peer enrichment**

Add this helper in `src/signal_tcp.c`:

```c
static agorahex_result_t enrich_capacity_peer(int fd, agorahex_message_t *msg) {
    struct sockaddr_in peer;
    socklen_t peer_len = (socklen_t)sizeof peer;
    char addr[INET_ADDRSTRLEN];
    char *copy;

    memset(&peer, 0, sizeof peer);
    if (getpeername(fd, (struct sockaddr *)&peer, &peer_len) != 0 ||
        peer.sin_family != AF_INET || !inet_ntop(AF_INET, &peer.sin_addr, addr, sizeof addr)) {
        return AGORAHEX_ERR_IO;
    }
    copy = agorahex_strdup(addr);
    if (!copy) {
        return AGORAHEX_ERR_NO_MEMORY;
    }
    free(msg->u.avc_capacity_indication.addr);
    msg->u.avc_capacity_indication.addr = copy;
    msg->u.avc_capacity_indication.port = (int)ntohs(peer.sin_port);
    return AGORAHEX_OK;
}
```

In `on_signal_frame()`, enrich only server-side capacity messages before invoking the message callback, and free the parsed message on every error path:

```c
if (dispatch->server_mode && msg.kind == AGORAHEX_KIND_AVC_CAPACITY_INDICATION) {
    r = enrich_capacity_peer(dispatch->fd, &msg);
    if (r != AGORAHEX_OK) {
        agorahex_message_free(&msg);
        return r;
    }
}
```

The existing read-path mapping classifies `AGORAHEX_ERR_NO_MEMORY` as `IO_ERROR`; `getpeername()` and `inet_ntop()` already return `AGORAHEX_ERR_IO`.

- [ ] **Step 4: Run the complete test suite to verify GREEN**

Run: `make test`

Expected: the TCP callback sees loopback peer values, the raw JSON comparison still succeeds, and the orderly disconnect callback still fires once.

- [ ] **Step 5: Commit transport enrichment**

```bash
git add src/signal_tcp.c tests/test_signal_tcp.c
git commit -m "feat: enrich AVC capacity peer endpoint"
```

### Task 5: Track The Sample And Release Notes

**Files:**
- Modify: `docs/json_msg/AVCCapacityIndication.txt`
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Make the sample satisfy the confirmed identifier limit**

Change only the JSON value from the long explanatory sentence to a stable short identifier:

```json
"identifier": "avc-process-1"
```

Keep the explanatory prose after the JSON object; `test_samples` intentionally extracts the first complete object from documentation fixtures.

- [ ] **Step 2: Replace the empty Unreleased entry**

Change:

```markdown
## [Unreleased]

- 暂无未发布记录。
```

to:

```markdown
## [Unreleased]

### 新增

- 新增 `AVCCapacityIndication` 消息的公开数据结构、JSON 解析、序列化和资源释放支持。
- Signal Server 在回调前使用 TCP peer 信息覆盖容量消息中的 `addr` 和 `port`，业务侧通过解析后的 `msg_t` 获取可信客户端端点。
- `agorahex_signal_start()` 新增可为空的服务端断开回调参数，可区分正常关闭、I/O 异常和协议错误。

### 说明

- 容量数值只校验为 C `int` 范围内的整数，业务范围和 `identifier` 集群唯一性由业务侧管理。
- `identifier` 必须为 1 到 64 个 UTF-8 字节。
```

- [ ] **Step 3: Run sample and full regression tests**

Run: `make test`

Expected: `test_samples` parses the now-tracked capacity fixture and every test passes.

- [ ] **Step 4: Commit documentation and sample**

```bash
git add CHANGELOG.md docs/json_msg/AVCCapacityIndication.txt
git commit -m "docs: document AVC capacity indication"
```

### Task 6: Final Clean Verification

**Files:**
- Verify only; no planned source changes.

- [ ] **Step 1: Check the complete diff and whitespace**

Run: `git diff --check HEAD~4..HEAD`

Expected: no output and exit code 0.

- [ ] **Step 2: Perform a clean build**

Run: `make clean`

Expected: build artifacts are removed.

Run: `make all`

Expected: `libagorahex.a`, `hexagora-server`, and `hexagora-client` build without warnings.

- [ ] **Step 3: Run the complete suite from the clean build**

Run: `make test`

Expected: `test_framing`, `test_envelope`, `test_samples`, `test_signal_tcp`, and `test_signal_disconnect` all pass.

- [ ] **Step 4: Inspect repository state**

Run: `git status --short`

Expected: no tracked modifications remain. Pre-existing unrelated untracked documentation files may still be listed and must not be staged or removed.
