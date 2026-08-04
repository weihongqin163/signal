# Configurable Signal Server IPv4 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the TCP signal server listen on every local IPv4 interface and make the client connect and reconnect to a required caller-supplied numeric IPv4 address.

**Architecture:** Extend the public startup API with `char *server_ipv4_addr`, validate it before starting either mode, and copy it into the client runtime so reconnect attempts remain independent of the caller's buffer lifetime. Server setup binds `INADDR_ANY`; client setup parses the stored address for every connection attempt.

**Tech Stack:** C11, POSIX sockets (`inet_pton`, `bind`, `connect`, `poll`), Make

---

## File Map

- `tests/test_signal_tcp.c`: argument validation and end-to-end address-selection regression coverage.
- `include/agorahex/signal_tcp.h`: public startup signature and API contract.
- `src/signal_tcp.c`: address validation, client address ownership, reconnect target, and server bind address.
- `examples/hexagora_server.c`: updated server-mode call site and listening message.
- `examples/hexagora_client.c`: configurable client target address and updated call site.
- `README.md`: current example commands and TCP signal behavior.
- `docs/plan-interface.md`: keep the existing interface reference consistent with the implemented API.

### Task 1: Add Failing Address-Selection Coverage

**Files:**
- Modify: `tests/test_signal_tcp.c:18-25`
- Modify: `tests/test_signal_tcp.c:63-65`
- Modify: `tests/test_signal_tcp.c:100-119`
- Test: `tests/test_signal_tcp.c`

- [ ] **Step 1: Add a non-default loopback target and invalid-address checks**

Add a target that distinguishes the configured address from the old hard-coded `127.0.0.1`:

```c
static char g_server_ipv4_addr[] = "127.0.0.2";
```

Change both integration startup calls to the four-argument API:

```c
int rc = agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, g_server_ipv4_addr, port, server_cb);
```

```c
if (agorahex_signal_start(AGORAHEX_SIGNAL_CLIENT_MODE, g_server_ipv4_addr, port, client_cb) != AGORAHEX_OK) {
```

At the beginning of `main()`, before `fork()`, verify the required argument contract in both modes:

```c
if (agorahex_signal_start(AGORAHEX_SIGNAL_CLIENT_MODE, NULL, port, client_cb) != AGORAHEX_ERR_INVALID_ARG) {
    fprintf(stderr, "client: NULL server address was accepted\n");
    return 1;
}
if (agorahex_signal_start(AGORAHEX_SIGNAL_CLIENT_MODE, "not-an-ipv4", port, client_cb) !=
    AGORAHEX_ERR_INVALID_ARG) {
    fprintf(stderr, "client: invalid server address was accepted\n");
    return 1;
}
if (agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, NULL, port, server_cb) != AGORAHEX_ERR_INVALID_ARG) {
    fprintf(stderr, "server: NULL server address was accepted\n");
    return 1;
}
if (agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, "not-an-ipv4", port, server_cb) !=
    AGORAHEX_ERR_INVALID_ARG) {
    fprintf(stderr, "server: invalid server address was accepted\n");
    return 1;
}
```

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```bash
make "$PWD/build/test_signal_tcp"
```

Expected: compilation fails because `agorahex_signal_start()` still accepts only three arguments. This is the expected failure caused by the missing API and behavior.

### Task 2: Implement The Public API And Socket Behavior

**Files:**
- Modify: `include/agorahex/signal_tcp.h:34-51`
- Modify: `src/signal_tcp.c:48-53`
- Modify: `src/signal_tcp.c:75-76`
- Modify: `src/signal_tcp.c:122-140`
- Modify: `src/signal_tcp.c:193-262`
- Modify: `src/signal_tcp.c:572-590`
- Test: `tests/test_signal_tcp.c`

- [ ] **Step 1: Extend the public API contract**

Update the declaration and its comments to state that the address is required, numeric IPv4, used as the client destination, and ignored as a bind selector in server mode:

```c
/**
 * Starts the TCP signal runtime in client or server mode.
 * Parameters:
 *   server_mode      - AGORAHEX_SIGNAL_CLIENT_MODE or AGORAHEX_SIGNAL_SERVER_MODE.
 *   server_ipv4_addr - required numeric IPv4 address; client target in client mode. Server mode
 *                      validates this value but always listens on all local IPv4 interfaces.
 *   tcp_port         - server listen port or client destination port, valid range 1..65535.
 *   cb               - receive callback for decoded JSON messages.
 * Returns:
 *   AGORAHEX_ERR_INVALID_ARG if mode, address, port, or callback is invalid.
 */
int agorahex_signal_start(int server_mode, char *server_ipv4_addr, int tcp_port, agorahex_signal_cb_t cb);
```

Retain the existing usage steps and other return documentation around this updated text.

- [ ] **Step 2: Store and reset the client target address**

Extend the client runtime:

```c
typedef struct agorahex_signal_client {
    agorahex_signal_conn_t conn;
    char server_ipv4_addr[INET_ADDRSTRLEN];
    int tcp_port;
    int running;
    agorahex_signal_cb_t cb;
} agorahex_signal_client_t;
```

Clear it in `runtime_reset()`:

```c
memset(g_runtime.client.server_ipv4_addr, 0, sizeof g_runtime.client.server_ipv4_addr);
g_runtime.client.tcp_port = 0;
```

- [ ] **Step 3: Bind server sockets to every IPv4 interface**

Replace the loopback conversion block in `start_server()` with:

```c
addr.sin_addr.s_addr = htonl(INADDR_ANY);
```

Keep all existing socket error handling around `bind()` and `listen()`.

- [ ] **Step 4: Validate and copy the required address at startup**

Change the internal client starter declaration and implementation to accept the address:

```c
static int start_client(const char *server_ipv4_addr, int tcp_port, agorahex_signal_cb_t cb);
```

```c
static int start_client(const char *server_ipv4_addr, int tcp_port, agorahex_signal_cb_t cb) {
    size_t addr_len = strlen(server_ipv4_addr);

    g_runtime.mode = AGORAHEX_SIGNAL_CLIENT_MODE;
    memcpy(g_runtime.client.server_ipv4_addr, server_ipv4_addr, addr_len + 1u);
    g_runtime.client.tcp_port = tcp_port;
    g_runtime.client.running = 1;
    g_runtime.client.cb = cb;
    conn_reset(&g_runtime.client.conn);
    (void)client_begin_connect(&g_runtime.client);
    return AGORAHEX_OK;
}
```

Update the public entry point:

```c
int agorahex_signal_start(int server_mode, char *server_ipv4_addr, int tcp_port, agorahex_signal_cb_t cb) {
    struct in_addr parsed_addr;

    ensure_runtime_initialized();
    if (g_runtime.mode != -1) {
        return AGORAHEX_ERR_ALREADY_STARTED;
    }
    if ((server_mode != AGORAHEX_SIGNAL_CLIENT_MODE && server_mode != AGORAHEX_SIGNAL_SERVER_MODE) ||
        !server_ipv4_addr || inet_pton(AF_INET, server_ipv4_addr, &parsed_addr) != 1 || !cb || tcp_port <= 0 ||
        tcp_port > 65535) {
        return AGORAHEX_ERR_INVALID_ARG;
    }
    if (server_mode == AGORAHEX_SIGNAL_SERVER_MODE) {
        return start_server(tcp_port, cb);
    }
    return start_client(server_ipv4_addr, tcp_port, cb);
}
```

The successful `inet_pton()` validation guarantees the copied text fits in `INET_ADDRSTRLEN`.

- [ ] **Step 5: Use the stored address for every client connection attempt**

Replace the hard-coded address in `client_begin_connect()`:

```c
if (inet_pton(AF_INET, client->server_ipv4_addr, &addr.sin_addr) != 1) {
    close(fd);
    client->conn.state = AGORAHEX_SIGNAL_CONN_DISCONNECTED;
    return AGORAHEX_ERR_IO;
}
```

This path is shared by initial connect and poll-driven reconnects.

- [ ] **Step 6: Run the focused test and verify GREEN**

Run:

```bash
make "$PWD/build/test_signal_tcp" && ./build/test_signal_tcp
```

Expected: compilation succeeds and the test exits with status 0. The `127.0.0.2` round trip demonstrates both `INADDR_ANY` listening and use of the configured client address.

- [ ] **Step 7: Commit the tested core change**

```bash
git add include/agorahex/signal_tcp.h src/signal_tcp.c tests/test_signal_tcp.c
git commit -m "feat: configure signal server IPv4 address"
```

### Task 3: Update Examples And Current Documentation

**Files:**
- Modify: `examples/hexagora_server.c:72-108`
- Modify: `examples/hexagora_client.c:140-167`
- Modify: `examples/hexagora_client.c:205-212`
- Modify: `README.md:65-92`
- Modify: `README.md:102-108`
- Modify: `docs/plan-interface.md:82-93`
- Modify: `docs/plan-interface.md:147-154`
- Modify: `docs/plan-interface.md:355-390`

- [ ] **Step 1: Update the server example call site**

Add a required address value for the API call, while making the actual bind behavior clear:

```c
char server_ipv4_addr[] = "127.0.0.1";
int rc = agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, server_ipv4_addr, port, server_cb);
```

Change the status output to:

```c
fprintf(stderr, "listening for signal TCP connections on 0.0.0.0:%d\n", port);
```

- [ ] **Step 2: Let the client example select its server address**

Add a default and parse `-server-ipv4-addr`:

```c
char *server_ipv4_addr = "127.0.0.1";
```

```c
if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
    port = atoi(argv[++i]);
} else if (strcmp(argv[i], "-server-ipv4-addr") == 0 && i + 1 < argc) {
    server_ipv4_addr = argv[++i];
```

Update usage, startup, and status output:

```c
fprintf(stderr,
        "usage: %s [-server-ipv4-addr address] -port tcp_port "
        "(-file envelope.json | -sample hangup|muted|start_content_request)\n",
        argv0);
```

```c
rc = agorahex_signal_start(AGORAHEX_SIGNAL_CLIENT_MODE, server_ipv4_addr, port, client_cb);
```

```c
fprintf(stderr, "connecting to signal server %s:%d\n", server_ipv4_addr, port);
```

- [ ] **Step 3: Update README commands and API summary**

Document that the server listens on `0.0.0.0:9876`, show the local default client invocation, add a remote example using `-server-ipv4-addr 192.0.2.10`, and rename “本机 TCP signal 层” to “TCP signal 层”. Remove the statement that examples only support processes on one machine.

- [ ] **Step 4: Update the existing interface reference**

Replace each three-argument declaration with:

```c
int agorahex_signal_start(int server_mode, char *server_ipv4_addr, int tcp_port, agorahex_signal_cb_t cb);
```

In its behavior section, add `server_ipv4_addr` as a required numeric IPv4 argument, state that server mode binds `INADDR_ANY`, and replace the old local-only limitation with the implemented client-target and reconnect semantics.

- [ ] **Step 5: Build all library and example targets**

Run:

```bash
make all
```

Expected: library and both examples compile with no warnings or errors.

- [ ] **Step 6: Commit examples and documentation**

```bash
git add examples/hexagora_server.c examples/hexagora_client.c README.md docs/plan-interface.md
git commit -m "docs: describe configurable signal server address"
```

### Task 4: Final Verification

**Files:**
- Verify: all modified files

- [ ] **Step 1: Check formatting and stale call sites**

Run:

```bash
git diff --check HEAD~2..HEAD
rg -n "agorahex_signal_start\(" include src tests examples docs README.md
rg -n 'inet_pton\(AF_INET, "127\.0\.0\.1"|listening local signal|connecting to local signal|本机 TCP signal' src examples include README.md
```

Expected: `git diff --check` exits 0; every startup call has four arguments; the stale-text search has no matches.

- [ ] **Step 2: Run the complete test suite from a clean build**

Run:

```bash
make clean
make test
make all
```

Expected: all four test binaries exit 0 and the library plus both examples compile successfully under the repository warning flags.

- [ ] **Step 3: Review the final diff against the approved design**

Run:

```bash
git status --short
git diff HEAD~2 -- include/agorahex/signal_tcp.h src/signal_tcp.c tests/test_signal_tcp.c examples README.md docs/plan-interface.md
```

Expected: only the planned API, socket behavior, tests, examples, and documentation changes appear; unrelated pre-existing untracked files remain untouched.
