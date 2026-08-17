# AVCCapacityIndication Protocol Design

## Goal

Add `AVCCapacityIndication` to the Agora-Hex C11 protocol library. The message reports an AVC process's current capacity to the Signal Server. The library parses and serializes the message, while the Signal Server receive path replaces the reported address and port with the actual TCP peer endpoint before invoking the application callback.

The library does not decide when the AVC process reports capacity, validate business-level capacity values, or enforce identifier uniqueness.

## Wire Format

The JSON envelope uses the following shape:

```json
{
  "AVCCapacityIndication": {
    "maxCapacity": 10,
    "curCapacity": 1,
    "addr": "192.168.1.1",
    "port": 8080,
    "identifier": "avc-process-1"
  }
}
```

The message direction is fixed as AVC TCP client to Signal Server. The `addr` and `port` values sent by the client are not trusted. In server mode, the library replaces them with the peer address and port obtained from the accepted TCP connection.

## Public Data Model

Add the following type to `include/agorahex/types.h`:

```c
typedef struct agorahex_avc_capacity_indication {
    int max_capacity;
    int cur_capacity;
    char *addr;
    int port;
    char *identifier;
} agorahex_avc_capacity_indication_t;
```

Append `AGORAHEX_KIND_AVC_CAPACITY_INDICATION` to `agorahex_kind_t` so existing enum values remain unchanged. Add `avc_capacity_indication` to the `agorahex_message_t` union.

`agorahex_message_free()` owns and releases `addr` and `identifier`. As with other parsed messages, callback consumers must not retain these pointers after the callback returns.

## Parsing Rules

The envelope parser recognizes the exact top-level key `AVCCapacityIndication`.

Required fields:

- `maxCapacity` must be a JSON integer representable by C `int`.
- `curCapacity` must be a JSON integer representable by C `int`.
- `identifier` must be a non-empty JSON string whose UTF-8 encoding is at most 64 bytes.

The parser does not impose business constraints on capacity values. Negative values, `curCapacity > maxCapacity`, and other semantically unusual values remain valid protocol data for the application to evaluate.

Optional fields:

- `addr` is copied when it is a JSON string. Missing values and values of another JSON type are treated as absent.
- `port` is copied when it is a JSON integer representable by C `int`. Missing values, non-integer values, values outside C `int`, and values of another JSON type are treated as absent with value `0`.

The parser does not validate the address text or port range because the server receive path replaces both values with the actual TCP peer endpoint.

## Serialization Rules

The serializer always emits `maxCapacity`, `curCapacity`, and `identifier`. It rejects a missing, empty, or over-64-byte identifier.

It emits `addr` only when `addr != NULL`, and emits `port` only when `port != 0`. Capacity values are serialized without applying business-level range or relationship checks.

## TCP Server Enrichment

The existing `signal_tcp` frame dispatch remains the integration point:

1. Decode the frame and parse its JSON envelope.
2. If the runtime is in server mode and the message kind is `AGORAHEX_KIND_AVC_CAPACITY_INDICATION`, call `getpeername(fd)` for the connection that supplied the frame.
3. Convert the IPv4 peer address with `inet_ntop()` and convert the peer port to host byte order.
4. Replace the parsed message's existing `addr` and `port` with the actual peer values.
5. Invoke the existing callback with the original raw `json` and the enriched `msg_t`.

Client-mode parsing does not perform enrichment. The original callback `json` remains byte-for-byte unchanged, so applications must read `msg_t.u.avc_capacity_indication.addr` and `.port` when they need the trusted endpoint.

If `getpeername()`, IPv4 conversion, or address allocation fails, the library does not invoke the application callback. It returns an I/O or allocation error through the existing receive path, which closes that client connection.

The transport remains IPv4-only, matching the current `signal_tcp` implementation.

## Responsibilities Outside The Library

The AVC application decides when to send the indication. Load-change reporting and periodic reporting are not implemented by this library.

The Signal Server application validates capacity semantics and manages any `identifier` to `fd` mapping. It is responsible for ensuring identifier uniqueness across the cluster. The protocol library only enforces that `identifier` is present, non-empty, and no longer than 64 UTF-8 bytes.

## Tests

Envelope tests cover:

- Parsing every field from a complete indication.
- Parsing without optional `addr` and `port`.
- Rejecting missing or incorrectly typed required fields.
- Rejecting fractional and out-of-C-`int` capacity values.
- Rejecting empty and over-64-byte identifiers.
- Accepting negative capacities and `curCapacity > maxCapacity` to prove business constraints are not enforced.
- Serializing complete and minimal indications.
- Round-tripping the message and returning the correct kind string.

The TCP integration test sends an indication containing a deliberately false address and port. The server callback verifies that the parsed message contains the actual loopback peer address and a real peer port, while the raw callback JSON still contains the original values.

`docs/json_msg/AVCCapacityIndication.txt` becomes a tracked sample and is parsed by the existing sample regression test.

## Documentation And Release Notes

Add an entry under `CHANGELOG.md`'s `Unreleased` section describing the new protocol message, its required fields, and server-side peer endpoint replacement.

No new public transport API, automatic reporting scheduler, capacity registry, or identifier uniqueness mechanism is added.
