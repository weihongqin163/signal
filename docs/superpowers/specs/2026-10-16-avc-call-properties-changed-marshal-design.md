# AVC Call Properties Changed Serialization

Extend `agorahex_marshal_envelope()` with a case for
`AGORAHEX_KIND_AVC_CALL_PROPERTIES_CHANGED_INDICATION`, using the existing public
payload and `AVCCallPropertiesChangedIndication` envelope key.

Confirmed behavior:

- Always emit string-valued `callId`. A NULL `call_id` becomes an empty string;
  an empty string stays empty; other strings retain their contents.
- Always emit the nested `properties` object and boolean `isAudioOnly`, including
  explicit false. Do not change parsing defaults or public structures.
- Follow existing JSON ownership conventions and report construction failures as
  `AGORAHEX_ERR_NO_MEMORY`, with NULL output and zero length.
- Update the previous receive-only test and README statement.

Implement directly in the switch: this is a single small payload, so an extracted
helper adds no useful reuse. Arbitrary-property preservation is outside this scope.

Validation covers NULL, empty and nonempty call IDs with both boolean values,
JSON field types and nesting, parse/marshal round trips, and allocation failures
while constructing the new fields. Run the envelope tests followed by `make test`
and `git diff --check`.
