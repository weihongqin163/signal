# AVCCallPropertiesChangedIndication Parse Support

## Goal and scope

Add receive-side parsing for the envelope in
`docs/json_msg/AVCCallPropertiesChangedIndication.txt`, following the existing
receive-only `DTMFIndication` pattern. The existing TCP callback delivers the
new typed message without a new callback or changes to transport behavior.

## Public model

Append `AGORAHEX_KIND_AVC_CALL_PROPERTIES_CHANGED_INDICATION` to
`agorahex_kind_t`, preserving existing enum values. Add the following types
to `include/agorahex/types.h`:

```c
typedef struct agorahex_avc_call_properties {
    bool is_audio_only;
} agorahex_avc_call_properties_t;

typedef struct agorahex_avc_call_properties_changed_indication {
    char *call_id;
    agorahex_avc_call_properties_t properties;
} agorahex_avc_call_properties_changed_indication_t;
```

Expose the payload as `message.u.avc_call_properties_changed_indication`.
The nested `properties` member mirrors the JSON object and leaves a clear
place for future supported properties.

## Parsing and ownership

- Recognize the exact top-level key `AVCCallPropertiesChangedIndication` and
  report that name from `agorahex_kind_cstr()`.
- Require the envelope body to be an object, as for existing message types.
- Copy string-valued `callId` into owned `call_id` memory. Missing or
  non-string values leave it `NULL`, matching existing call indications.
  Return `AGORAHEX_ERR_NO_MEMORY` if copying a valid string fails.
- Read the boolean at `properties.isAudioOnly`. Preserve explicit `true`
  and `false`.
- As confirmed by the user, missing `isAudioOnly` means `false`; no presence
  flag is needed. Missing `properties` likewise leaves the default `false`.
- Follow the existing tolerant nested-field policy: non-object `properties`
  or non-boolean `isAudioOnly` leave the default `false`. Do not coerce strings
  or numbers to booleans. Ignore unknown fields.
- `agorahex_message_free()` frees `call_id` and clears the message, including
  the new payload. Existing callback lifetime rules continue to apply.

The library exposes the parsed value to the application; it does not maintain
per-call audio state. Marshaling the receive-only kind remains unsupported and
returns `AGORAHEX_ERR_UNKNOWN_KIND`.

## Alternatives

1. Recommended: typed receive-only support with nested `properties`, matching
   both the sample and existing `DTMFIndication` scope.
2. Add serialization too: useful for generating messages, but extends the
   requested receive-side scope.
3. Preserve arbitrary properties as raw JSON: supports unknown fields, but
   adds public ownership complexity without a current consumer requirement.

## Validation

Add focused envelope regressions for kind/name mapping, copied `callId`,
explicit boolean values, absent `isAudioOnly`, absent `properties`, invalid
nested types, unknown fields, non-object bodies, and cleanup. Check that
marshaling this receive-only kind returns `AGORAHEX_ERR_UNKNOWN_KIND`.

The existing sample suite automatically discovers the supplied `.txt` sample;
it must parse successfully after implementation. Run `make test` for the full
suite, including transport regressions, and `git diff --check`.
