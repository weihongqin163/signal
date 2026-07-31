# JSON Protocol Call And Status Semantics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the protocol DOCX with normative call correlation and status-code semantics plus a full `dropCode` appendix.

**Architecture:** Apply surgical `python-docx` edits to the existing document while preserving its current styles and source fixtures. Verify semantic coverage structurally before rendering every page for visual QA.

**Tech Stack:** Python 3, python-docx, OOXML, LibreOffice document renderer

---

### Task 1: Establish Structural Verification

**Files:**
- Create temporarily: `/private/tmp/verify_json_protocol_doc.py`
- Verify: `docs/agora-hex-json-protocol.docx`

- [ ] **Step 1: Record source fixture hashes**

Run `shasum -a 256 docs/json_msg/*.txt` and retain the output for the post-edit
comparison. No source fixture may change.

- [ ] **Step 2: Create the structural verifier**

Create `/private/tmp/verify_json_protocol_doc.py` using this verification model:

```python
from docx import Document

DOC = "docs/agora-hex-json-protocol.docx"
REQUEST_ID = "f11db41e-7ba7-4d8b-9960-41363c0b711a"
CODES = {
    0: "NORMAL_REASON",
    1: "NO_BANDWIDTH_AVAILABLE",
    3: "DESTINATION_NOT_FOUND",
    4: "CAUSE_CALL_REJECT",
    6: "GATEKEEPER_OUTOF_RESOURCES",
    11: "CAUSE_USER_BUSY",
    13: "SECURITY_ACCESS_DENIED",
    16: "CAUSE_NO_RESOURCE",
    17: "CALLED_PARTY_NOH245",
    18: "CAUSE_NOROUTE_TODEST",
    23: "SIGNAL_IS_DISABLE",
    101: "NON_ENCRYPT_TERM_JOIN_ENCRYPT_MEETING",
    102: "RESOURCE_DEFICIENCY",
    104: "OPERATOR_DISCONNECT",
    106: "NO_CONFID_MATCHED",
    107: "NO_PACKET_RECEIVED",
    109: "CONF_IS_LOCKED",
    110: "INVALID_CONF_PASSWORD",
    111: "INVALID_CONF_NID",
    112: "ALREADY_DIALED_IN",
    115: "CONFERENCE_EXPIRED",
    116: "MCU_SHUTDOWN",
    117: "MCU_MODULE_DOWN",
    255: "UNDEFINED_REASON",
}

doc = Document(DOC)
paragraph_text = "\n".join(p.text for p in doc.paragraphs)
rows = [
    [cell.text.strip() for cell in row.cells]
    for table in doc.tables
    for row in table.rows
]
all_text = paragraph_text + "\n" + "\n".join(" | ".join(row) for row in rows)

assert "附录 D. HangupIndication dropCode 枚举" in all_text
assert "同一通话的后续消息必须复用" in all_text
assert "0 表示成功" in all_text
assert "0 表示正常挂断" in all_text
for value, symbol in CODES.items():
    assert any(row[:2] == [str(value), symbol] for row in rows), (value, symbol)

reply_examples = [
    row[0]
    for row in rows
    if len(row) == 1 and '"AVCDialInReply"' in row[0]
]
assert len(reply_examples) == 1
assert REQUEST_ID in reply_examples[0]
assert "103387fb-0caa-431b-8973-4a32a4a443a2" not in reply_examples[0]
```

- [ ] **Step 3: Run the verifier to establish RED**

Run:

```bash
/Users/weihognqin/.local/bin/uv run --offline --with python-docx /private/tmp/verify_json_protocol_doc.py
```

Expected: FAIL because Appendix D and the confirmed normative text do not yet
exist.

### Task 2: Apply The Confirmed Semantic Edits

**Files:**
- Create temporarily: `/private/tmp/update_json_protocol_doc.py`
- Modify: `docs/agora-hex-json-protocol.docx`

- [ ] **Step 1: Create document-editing helpers**

Use `python-docx` and define helpers that preserve paragraph styles, set table
cell text, insert a paragraph after an existing paragraph, delete an obsolete
paragraph, and mark a table row as repeating. The core helpers are:

```python
from docx import Document
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.text.paragraph import Paragraph

def set_cell_text(cell, value):
    paragraph = cell.paragraphs[0]
    style = paragraph.style
    paragraph.clear()
    paragraph.style = style
    paragraph.add_run(str(value))

def insert_after(paragraph, text, style=None):
    node = OxmlElement("w:p")
    paragraph._p.addnext(node)
    result = Paragraph(node, paragraph._parent)
    if style is not None:
        result.style = style
    result.add_run(text)
    return result

def delete_paragraph(paragraph):
    paragraph._element.getparent().remove(paragraph._element)

def repeat_header(row):
    tr_pr = row._tr.get_or_add_trPr()
    tbl_header = OxmlElement("w:tblHeader")
    tbl_header.set(qn("w:val"), "true")
    tr_pr.append(tbl_header)
```

- [ ] **Step 2: Update common `callId` semantics**

In field-definition rows whose first cell is `callId`, replace the current
description and constraint with:

```text
通话级关联标识。发起方在通话建立时生成；同一通话的后续消息必须复用该值。
必填；同一通话内保持一致
```

Update the Chapter 6 correlation row to:

```text
材料事实：11 类消息的消息体均包含 callId
协议约束状态：callId 为通话级关联标识；发起方在通话建立时生成，同一通话的后续消息必须复用。
```

Delete the obsolete Chapter 6 candidate bullet saying the session-control
messages "可能" use `callId`.

- [ ] **Step 3: Normalize the displayed reply call identifier**

Replace `103387fb-0caa-431b-8973-4a32a4a443a2` with
`f11db41e-7ba7-4d8b-9960-41363c0b711a` in the common field table, the
`AVCDialInReply` field table, its JSON example, and Appendix C. Add this reply
processing note:

```text
为展示同一通话的请求/应答关联，本文将 AVCDialInReply 示例的 callId 规范化为 AVCDialInRequest 示例值；原始独立 JSON 样例保持不变。
```

- [ ] **Step 4: Define `returnCode` and `dropCode`**

Set the `returnCode` field description and constraint to:

```text
处理结果码。0 表示成功；非零表示失败，具体错误码由双方另行约定。
0：成功；非零：双方另行约定
```

Set the `dropCode` field description and constraint to:

```text
挂断原因码。0 表示正常挂断；其他值见附录 D。
整数枚举；完整取值见附录 D
```

Add message-specific processing text stating the same rules without changing
unrelated direction, retry, or state-machine questions.

- [ ] **Step 5: Record the confirmed decisions**

Append these rows to the Chapter 7 review-conclusion table:

```text
C-01 | callId 关联 | callId 为通话级关联标识；后续消息必须复用 | 协议文档 | 当前版本 | 已确认
C-02 | returnCode | 0 表示成功；非零错误码由双方另行约定 | AVCDialInReply | 当前版本 | 已确认
C-03 | dropCode | 0 表示正常挂断；其他值按附录 D | HangupIndication | 当前版本 | 已确认
```

Keep broad unresolved entries P-11 and P-12 because other field constraints and
reliability rules remain open.

### Task 3: Add Appendix D

**Files:**
- Modify: `docs/agora-hex-json-protocol.docx`
- Reference: `docs/json_msg/drop_code.pdf`

- [ ] **Step 1: Add the contents entry and appendix heading**

Insert `附录 D. HangupIndication dropCode 枚举` after the existing Appendix C
entry in the front contents list. Append a Heading 1 with the same title after
Appendix C and a short source note naming `docs/json_msg/drop_code.pdf`.

- [ ] **Step 2: Add the complete code table**

Create a three-column table with headings `值`, `英文枚举`, and `含义`. Reuse
the existing `Normal Table` style, repeat the header row, and add these rows:

```text
0   | NORMAL_REASON                         | 正常挂断
1   | NO_BANDWIDTH_AVAILABLE                | 呼叫资源不足
3   | DESTINATION_NOT_FOUND                 | 目的地址不存在
4   | CAUSE_CALL_REJECT                     | 被叫拒绝应答
6   | GATEKEEPER_OUTOF_RESOURCES            | 双流请求太频繁
11  | CAUSE_USER_BUSY                       | 被叫忙
13  | SECURITY_ACCESS_DENIED                | 需要加密呼叫
16  | CAUSE_NO_RESOURCE                     | 资源耗尽
17  | CALLED_PARTY_NOH245                   | 媒体通道未建立
18  | CAUSE_NOROUTE_TODEST                  | 无法路由到目的地
23  | SIGNAL_IS_DISABLE                     | 本地信令未启用
101 | NON_ENCRYPT_TERM_JOIN_ENCRYPT_MEETING | 终端的媒体需要加密
102 | RESOURCE_DEFICIENCY                   | 本地端口资源不足
104 | OPERATOR_DISCONNECT                   | 管理员挂断
106 | NO_CONFID_MATCHED                     | 未找到相应的会议 ID
107 | NO_PACKET_RECEIVED                    | 未收到媒体流
109 | CONF_IS_LOCKED                        | 会议被锁定
110 | INVALID_CONF_PASSWORD                 | 会议密码错误
111 | INVALID_CONF_NID                      | 会议不能被创建
112 | ALREADY_DIALED_IN                     | 终端已经在会议中
115 | CONFERENCE_EXPIRED                    | 会议已经过期
116 | MCU_SHUTDOWN                          | 系统重启
117 | MCU_MODULE_DOWN                       | 模块重启
255 | UNDEFINED_REASON                      | 未定义原因
```

- [ ] **Step 3: Save atomically**

Save first to `/private/tmp/agora-hex-json-protocol.updated.docx`, reopen it with
`python-docx` to ensure the package is valid, then replace
`docs/agora-hex-json-protocol.docx` with the validated output.

### Task 4: Verify Content And Source Integrity

**Files:**
- Verify: `docs/agora-hex-json-protocol.docx`
- Verify unchanged: `docs/json_msg/*.txt`

- [ ] **Step 1: Run structural verification to establish GREEN**

Run the Task 1 verifier. Expected: exit 0 with all semantic, identifier, and
24-row assertions passing.

- [ ] **Step 2: Compare source hashes**

Run `shasum -a 256 docs/json_msg/*.txt`. Expected: every hash matches Task 1
Step 1.

- [ ] **Step 3: Inspect repository scope**

Run `git status --short` and `git diff --check`. Expected: no source or test
modification; the DOCX remains the only delivery artifact changed by
implementation, apart from the committed spec and plan.

### Task 5: Render And Visually Verify

**Files:**
- Verify: `docs/agora-hex-json-protocol.docx`
- Generate temporarily: `/private/tmp/agora-hex-json-protocol-render/`

- [ ] **Step 1: Render the completed DOCX**

Run:

```bash
/Users/weihognqin/.local/bin/uv run --offline --with python-docx \
  /Users/weihognqin/.codex/plugins/cache/openai-primary-runtime/documents/26.426.12240/skills/documents/render_docx.py \
  docs/agora-hex-json-protocol.docx \
  --output_dir /private/tmp/agora-hex-json-protocol-render \
  --emit_pdf
```

Expected: one PNG per page and a non-empty rendered PDF.

- [ ] **Step 2: Inspect every rendered page**

Open every `page-*.png` at full resolution. Confirm there is no clipping,
overlap, missing Chinese glyph, broken table, orphaned appendix heading, or
excessive blank space. Pay particular attention to Appendix D and pages whose
field descriptions became longer.

- [ ] **Step 3: Iterate if needed**

If visual defects exist, adjust only affected paragraph spacing, table widths,
cell padding, or page-break behavior. Rerun Tasks 4 and 5 after each edit batch.

- [ ] **Step 4: Final verification**

Run the structural verifier, `git diff --check`, and `git status --short`.
Expected: verifier exit 0, no whitespace errors, and no source fixture or code
file changed.
