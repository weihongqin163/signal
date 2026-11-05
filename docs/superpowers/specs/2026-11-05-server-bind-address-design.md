# Server IPv4 bind address

## Approved behavior

`agorahex_signal_start()` uses `server_ipv4_addr` as the listening address in
server mode and as the connection target in client mode. The public signature
and numeric IPv4 validation remain unchanged. Preserve the concurrent workspace
change that normalizes a NULL server address to `0.0.0.0`; client NULL addresses
remain invalid.

- `127.0.0.1` listens on the loopback address only.
- `0.0.0.0` listens on all local IPv4 interfaces.
- A concrete local IPv4 address listens on that address only.
- An unavailable address or occupied endpoint fails with `AGORAHEX_ERR_IO`.

This controls destination addresses, not client identities or source addresses.
Loopback access requires a shared network namespace. A client Pod accessing a
host process through loopback must use host networking on the same node.

## Implementation

Pass the already parsed IPv4 address to the internal server startup function
and assign it to `sockaddr_in.sin_addr`. Preserve socket cleanup and runtime
state handling on failure. Client connection and reconnection behavior stays
unchanged.

The server example defaults to `127.0.0.1` and accepts `-server-ipv4-addr` to
select another listening address. Its startup log reports that address.

## Verification

Exercise actual TCP connections against loopback, wildcard, and a concrete
non-loopback local address. Confirm both accepted and refused destinations.
If no non-loopback IPv4 interface exists, explicitly report the coverage skip
and still exercise loopback. Run the existing full test suite and example build.
