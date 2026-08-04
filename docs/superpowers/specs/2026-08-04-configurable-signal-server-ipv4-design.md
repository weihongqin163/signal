# Configurable Signal Server IPv4 Design

## Goal

Allow the TCP signal client to connect to a caller-specified IPv4 address while the TCP signal server listens on every local IPv4 interface.

## Public API

Change the startup function to:

```c
int agorahex_signal_start(int server_mode,
                          char *server_ipv4_addr,
                          int tcp_port,
                          agorahex_signal_cb_t cb);
```

`server_ipv4_addr` is required in both modes. A `NULL` value or a string that is not a valid numeric IPv4 address returns `AGORAHEX_ERR_INVALID_ARG` without starting the runtime.

In client mode, the address selects the remote server. In server mode, the value is validated for a consistent API contract but does not select the listening interface.

## Runtime Behavior

The client runtime owns a fixed-size copy of `server_ipv4_addr`. This avoids depending on the caller's buffer after `agorahex_signal_start()` returns and keeps the address available for reconnection attempts driven by `agorahex_signal_poll()`.

`client_begin_connect()` converts the stored address with `inet_pton()` and connects to that address and `tcp_port`. Both the initial attempt and every later reconnect therefore use the configured server address.

`start_server()` binds its listening socket to `htonl(INADDR_ANY)`. It no longer binds only to `127.0.0.1`, so connections arriving on any local IPv4 interface can be accepted.

`agorahex_signal_close()` and runtime reset clear the stored client address together with the other client state.

## Call Sites And Documentation

All existing callers must pass an explicit IPv4 address. Tests and local examples use `127.0.0.1`. The public header documentation describes the required address, client behavior, and server `INADDR_ANY` behavior. User-facing example messages must no longer describe the transport as local-only where that is no longer true.

## Error Handling

- `NULL` or malformed `server_ipv4_addr`: `AGORAHEX_ERR_INVALID_ARG`.
- Valid IPv4 whose remote endpoint cannot currently be reached: preserve the existing client reconnect behavior; startup succeeds and polling retries.
- Server bind or listen failure: preserve the existing `AGORAHEX_ERR_IO` behavior.

## Tests

- Verify startup rejects a `NULL` server address.
- Verify startup rejects a malformed IPv4 address.
- Update the TCP integration test to pass `127.0.0.1` explicitly and verify bidirectional communication still works.
- Run the complete test suite and build the examples to catch all public API call sites.

## Scope

This change supports numeric IPv4 addresses only. DNS names, IPv6 addresses, interface selection for server mode, reconnect backoff, and API compatibility wrappers are outside scope.
