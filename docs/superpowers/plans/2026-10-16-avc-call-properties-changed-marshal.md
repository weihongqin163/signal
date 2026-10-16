# AVC Call Properties Changed Serialization Plan

**Goal:** Serialize the existing AVC call properties changed payload according to
the user-confirmed rules in the companion design.

**Architecture:** Add one case in `src/envelope.c`; keep the public model and
parser unchanged. Use cJSON's owned child objects and existing error handling.

**Tech stack:** C11, cJSON, Make.

- [x] In `tests/test_envelope.c`, remove the obsolete receive-only expectation.
  Add serialization and round-trip coverage for the six combinations of NULL,
  empty and nonempty call IDs with true and false. Inspect the JSON to require
  string-valued callId and nested boolean-valued properties.isAudioOnly.
- [x] Use the existing cJSON allocation hooks to fail allocation of the callId,
  properties and isAudioOnly keys; require NO_MEMORY, NULL JSON and zero length.
- [x] Run `make build/test_envelope` using the Makefile's absolute build target,
  then `./build/test_envelope`; confirm failure because marshaling is unsupported.
- [x] Add the switch case: create body, add callId using
  `x->call_id ? x->call_id : ""`, add properties object and isAudioOnly boolean. On construction failure,
  delete body and set it to NULL for the existing NO_MEMORY return path.
- [x] Update `README.md` to describe serialization and the NULL-to-empty rule.
- [x] Run `make test` and `git diff --check`; review the final diff for scope.
