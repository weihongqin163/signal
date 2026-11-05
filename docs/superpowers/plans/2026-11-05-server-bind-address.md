# Server IPv4 bind address implementation plan

**Goal:** Make the existing server IPv4 parameter select the listening address.

**Architecture:** Reuse the address parsed by `agorahex_signal_start()` for
`bind()`. Keep the public API, validation, socket cleanup, and client behavior.

**Tech Stack:** C11, POSIX IPv4 TCP sockets, Make.

## Tasks

- [x] Add `tests/test_signal_bind.c` and register it in `Makefile`. Discover a
  non-loopback local IPv4 address with `getifaddrs()`. Connect real sockets with
  bounded waits and send a framed message to confirm accepted connections are
  processed. Cover loopback-only, wildcard, and a specific local address.
- [x] Run `make test` before changing production code. The new test must fail
  because a loopback-configured server still accepts the non-loopback address.
- [x] Change internal startup to
  `start_server(struct in_addr bind_addr, int tcp_port, ...)`, assign
  `addr.sin_addr = bind_addr`, and pass `parsed_addr` from the public entry point.
- [x] Update `examples/hexagora_server.c` to accept `-server-ipv4-addr`, default
  to `127.0.0.1`, and print the configured listening address.
- [x] Update the public header, README, interface document, and changelog to
  document address selection and the loopback network namespace requirement.
- [x] Run `make test`, `make all`, and `git diff --check`. Review the diff for
  unrelated changes and retain the user's existing untracked files.

## Results

The regression failed against the old implementation because a server configured
with `127.0.0.1` accepted a connection to the non-loopback local address.
After the change, all six test binaries and both example builds passed. The
successful full run used sandbox escalation to permit local TCP socket binding;
there were no skipped network checks. The NULL server-address default is covered
alongside explicit wildcard, loopback, and specific-interface addresses.
