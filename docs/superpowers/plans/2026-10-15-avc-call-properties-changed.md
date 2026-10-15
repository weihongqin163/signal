# AVC Call Properties Changed Implementation Plan

> **For agentic workers:** Execute inline using superpowers:executing-plans; use test-driven-development and verification-before-completion. The user has approved the design and implementation.

**Goal:** Parse `AVCCallPropertiesChangedIndication`, defaulting absent `isAudioOnly` to `false`.

**Architecture:** Extend the existing envelope kind mappings, typed payload union, parser switch and cleanup switch. Keep the existing TCP callback and receive-only marshaling behavior.

**Tech Stack:** C11, bundled cJSON, Makefile test executables.

## Task 1: Establish failing coverage

**File:** `tests/test_envelope.c`.

- [x] Add a smoke test that calls `agorahex_parse_envelope()` with the documented message and requires `AGORAHEX_OK`.
- [x] Run `make build/test_envelope` using the Makefile's absolute target (`make "$PWD/build/test_envelope"`), then `./build/test_envelope`. Expect the new smoke assertion to fail because the kind is unknown.
- [x] Expand coverage to assert the new kind, kind name, owned `call_id`, nested boolean, missing-field defaults, invalid nested types, unknown fields, cleanup, unsupported marshaling and non-object body rejection.

## Task 2: Extend the public model and envelope implementation

**Files:** `include/agorahex/types.h`, `include/agorahex/envelope.h`, `src/envelope.c`.

- [x] Add the types from the approved design:

```c
typedef struct agorahex_avc_call_properties {
    bool is_audio_only; /* Defaults to false when absent. */
} agorahex_avc_call_properties_t;

typedef struct agorahex_avc_call_properties_changed_indication {
    char *call_id;
    agorahex_avc_call_properties_t properties;
} agorahex_avc_call_properties_changed_indication_t;
```

- [x] Append `AGORAHEX_KIND_AVC_CALL_PROPERTIES_CHANGED_INDICATION` to the enum; append `agorahex_avc_call_properties_changed_indication_t avc_call_properties_changed_indication` to the message union.
- [x] Map that enum to and from the exact name `AVCCallPropertiesChangedIndication` in both kind mapping functions.
- [x] In `parse_kind_body()`, implement:

```c
case AGORAHEX_KIND_AVC_CALL_PROPERTIES_CHANGED_INDICATION: {
    agorahex_avc_call_properties_changed_indication_t *x =
        &out->u.avc_call_properties_changed_indication;
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(body, "callId");
    const cJSON *properties = cJSON_GetObjectItemCaseSensitive(body, "properties");
    if (cJSON_IsString(it)) {
        x->call_id = dup_or_null(cJSON_GetStringValue(it));
        if (!x->call_id) {
            return AGORAHEX_ERR_NO_MEMORY;
        }
    }
    if (cJSON_IsObject(properties)) {
        it = cJSON_GetObjectItemCaseSensitive(properties, "isAudioOnly");
        if (cJSON_IsBool(it)) {
            x->properties.is_audio_only = cJSON_IsTrue(it);
        }
    }
    return AGORAHEX_OK;
}
```

- [x] In the cleanup switch, free the new payload's `call_id`; the existing final `memset(m, 0, sizeof(*m))` clears the entire payload.
- [x] Leave the serializer's default `AGORAHEX_ERR_UNKNOWN_KIND` behavior for this receive-only kind.

## Task 3: Verify and document

**Files:** `README.md`, the source/test files above, existing JSON sample.

- [x] Run `make test`; require all five executables to pass, including the sample suite discovering the user's sample automatically. If socket tests skip because binding is sandboxed, rerun with the required sandbox permissions.
- [x] Run `make all` to ensure both examples compile against the public model.
- [x] Document the callback payload path and the `false` default in README.
- [x] Inspect `git diff` and run `git diff --check`; preserve unrelated user files. Deliver the local edits without committing or publishing.

## Execution evidence

The initial smoke test failed with `FAIL: parse AVC call properties changed indication` before implementation. After implementation, `make test` passed all five test executables, including the supplied JSON sample, and `make all` built both examples. `git diff --check` passed.

A final `make -B test all` rebuilt every source and both examples without compiler warnings. Its two TCP tests skipped because the sandbox blocked local binding, so `make test` was rerun outside the sandbox: all five executables passed without skips. Independent read-only code review found no issues.
