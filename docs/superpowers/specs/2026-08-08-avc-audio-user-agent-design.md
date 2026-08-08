# AVC Audio Property and User Agent Support

**Date:** 2026-08-08

## Goal

Extend `AVCDialInRequest` so the public C model, JSON parser, serializer, and
cleanup path preserve these fields:

- `AVCDialInRequest.avcEndpoint.avcEndpoint.userAgent`
- `AVCDialInRequest.avcEndpoint.audioProperty`

Also materialize documented defaults when `audioProperty` or
`contentProperty` fields are absent.

## Public Data Model

Add `char *user_agent` to `agorahex_avc_signal_leg_t`. The shared signal-leg
type remains appropriate because `userAgent` describes the inner AVC signaling
endpoint. An absent `userAgent` remains `NULL` and is omitted when serializing;
an explicitly empty string is preserved.

Add a dedicated audio capability type:

```c
typedef struct agorahex_audio_capabilities {
    char *media_id;
    int sample_rate;
    int channels;
    int bits;
} agorahex_audio_capabilities_t;
```

Add `agorahex_audio_capabilities_t audio_property` to
`agorahex_avc_dial_endpoint_t`. A dedicated type avoids forcing audio fields
into the existing video-oriented capability type.

All string fields owned by a parsed message are heap allocated and released by
`agorahex_message_free()`.

## Parsing And Defaults

`parse_avc_signal_leg()` reads a string-valued `userAgent` into `user_agent`.
Unknown or incorrectly typed values retain the existing permissive behavior and
are ignored.

`parse_avc_dial_endpoint()` reads `audioProperty` from the same object that
contains `peopleProperty` and `contentProperty`.

Audio defaults are initialized before applying JSON values:

| Field | Default |
|---|---:|
| `mediaId` | `""` |
| `samplerate` | `48000` |
| `channels` | `1` |
| `bits` | `16` |

These defaults apply when the entire `audioProperty` object is absent and when
individual fields are absent. Explicit, correctly typed JSON fields override
the defaults.

When `contentProperty` is absent or omits `mediaId`, its in-memory defaults are
`media_id=""`, `bitrate=0`, `width=0`, `height=0`, and `fps=0`. Explicit,
correctly typed fields override those defaults. `peopleProperty` behavior is
unchanged.

The JSON spelling remains exactly `samplerate`, matching the supplied protocol
sample. C code uses the conventional member name `sample_rate`.

## Serialization

`json_avc_signal_inner()` emits `userAgent` whenever `user_agent` is non-NULL,
including an explicitly empty string.

`json_avc_dial_endpoint()` always emits `audioProperty`, using the values in
the public model. Parsed messages therefore serialize either the explicit input
values or the materialized defaults.

Existing `contentProperty` serialization remains in place. Because parsing now
materializes an empty `media_id`, a parsed request with no `contentProperty`
serializes a complete zero-valued object including `"mediaId":""`.

## Error And Compatibility Behavior

The feature preserves the parser's existing permissive nested-field policy:
unknown fields and fields with unexpected JSON types are ignored. No new JSON
validation errors are introduced.

Adding fields to public structs changes their size but preserves existing field
names and source-level behavior. Code that zero-initializes these structs keeps
working. Manually constructed messages with a zero-initialized audio property
serialize zero values; protocol defaults are applied during parsing, not by the
serializer.

## Tests

Add focused envelope tests that first fail against the current implementation:

1. Parse an explicit `userAgent` and `audioProperty`, assert every public C
   field, serialize the message, assert the exact JSON paths, parse it again,
   and assert the same values.
2. Parse a request without `audioProperty` and `contentProperty`, assert the
   in-memory defaults, serialize it, and assert that both complete default
   objects are present.
3. Run the complete test suite after implementation.

The modified sample currently contains `//` comments, which are outside JSON
syntax and are independent of typed field support. Sample normalization is not
part of this code change.
