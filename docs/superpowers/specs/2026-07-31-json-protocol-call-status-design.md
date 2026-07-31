# JSON Protocol Call And Status Semantics Design

## Goal

Complete the Agora-Hex JSON protocol document with normative `callId`,
`returnCode`, and `dropCode` semantics while preserving the original JSON sample
files and the document's existing structure and visual style.

## Scope

Update `docs/agora-hex-json-protocol.docx` only. The source `.txt` fixtures,
library code, tests, and supplied PDF references remain unchanged.

The document will state:

- `callId` is the call-level correlation identifier.
- Messages after the first call-establishing message must reuse that call's
  `callId`.
- `AVCDialInReply.returnCode == 0` means success.
- Non-zero `returnCode` values are failure codes whose meanings are agreed by
  both parties separately.
- `HangupIndication.dropCode == 0` means a normal hangup.
- Other `dropCode` values use the enumeration supplied in
  `docs/json_msg/drop_code.pdf`.

## Document Changes

### Call Correlation

Revise the common `callId` definition and the interaction rules in Chapters 4
and 6. Replace affected "to be determined" language in message field tables and
processing notes with the confirmed call-level reuse rule.

The displayed `AVCDialInReply.callId` example will use the same value as the
displayed `AVCDialInRequest.callId` example. Add a short note explaining that
the document normalizes this value to demonstrate one call flow while the
original standalone JSON fixtures remain unchanged.

### Status Codes

Revise the `AVCDialInReply.returnCode` field description and processing note to
define zero as success and non-zero values as separately agreed failure codes.

Revise the `HangupIndication.dropCode` field description and processing note to
define zero as a normal hangup and direct readers to Appendix D for the complete
enumeration.

Remove or resolve only the review questions made obsolete by these confirmed
rules. Unrelated open protocol questions remain intact.

### Appendix D

Add `Appendix D. HangupIndication dropCode Enumeration` after the existing
Appendix C and add it to the document contents list.

The appendix contains one table with these columns:

- Value
- Symbol
- Meaning

Transcribe all 24 rows from `drop_code.pdf`. Preserve numeric values and English
symbols exactly. Use the PDF's Chinese reference text, with `NORMAL_REASON`
expressed as "normal hangup" in the `HangupIndication` context and
`UNDEFINED_REASON` expressed as "undefined reason" from its symbol.

## Editing And Layout

Make surgical edits with `python-docx`, preserving the current styles, page
setup, headers, footers, and existing tables. The new appendix table will reuse
the document's existing field-table styling, repeat its header when it spans
pages, and allow rows to expand without clipping.

## Verification

The completed document must pass all of the following checks:

1. All 24 PDF code values and symbols appear exactly once in Appendix D.
2. The normative call and status semantics appear in the relevant common and
   message-specific sections.
3. The displayed request and reply `callId` values match.
4. The original JSON `.txt` files are unchanged.
5. The document renders successfully to page images.
6. Every rendered page is visually inspected for clipping, overlap, broken
   tables, missing glyphs, and inconsistent pagination.

## Out Of Scope

- Defining non-zero `returnCode` meanings
- Changing any `dropCode` value or symbol from the supplied PDF
- Changing message parsing or marshaling behavior
- Modifying source JSON samples to represent one continuous call flow
- Resolving unrelated protocol direction, retry, timeout, or state-machine
  questions
