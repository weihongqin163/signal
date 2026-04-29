/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#include "agorahex/envelope.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *m) {
    fprintf(stderr, "FAIL: %s\n", m);
    return 1;
}

int main(void) {
    const char *raw =
        "{\"HangupIndication\":{\"callId\":\"f11db41e-7ba7-4d8b-9960-41363c0b711a\",\"dropCode\":0}}";
    agorahex_message_t m;
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(raw, strlen(raw), &m) != AGORAHEX_OK) {
        return fail("parse hangup");
    }
    if (m.kind != AGORAHEX_KIND_HANGUP_INDICATION) {
        return fail("kind");
    }
    const char *cid = m.u.hangup_indication.call_id;
    if (!cid || strcmp(cid, "f11db41e-7ba7-4d8b-9960-41363c0b711a") != 0 || m.u.hangup_indication.drop_code != 0) {
        return fail("fields");
    }
    agorahex_message_free(&m);

    const char *unk = "{\"Unknown\":{}}";
    if (agorahex_parse_envelope(unk, strlen(unk), &m) != AGORAHEX_ERR_UNKNOWN_KIND) {
        return fail("unknown");
    }

    const char *twokeys = "{\"HangupIndication\":{},\"MutedIndication\":{}}";
    if (agorahex_parse_envelope(twokeys, strlen(twokeys), &m) != AGORAHEX_ERR_ENVELOPE_KEY_COUNT) {
        return fail("two keys");
    }

    agorahex_message_t out;
    memset(&out, 0, sizeof out);
    out.kind = AGORAHEX_KIND_AVC_START_CONTENT_REPLAY;
    out.u.avc_start_content_replay.call_id = strdup("abc");
    out.u.avc_start_content_replay.accept = true;
    char *json = NULL;
    size_t jl = 0;
    if (agorahex_marshal_envelope(&out, &json, &jl) != AGORAHEX_OK) {
        free(out.u.avc_start_content_replay.call_id);
        return fail("marshal");
    }
    free(out.u.avc_start_content_replay.call_id);
    memset(&out, 0, sizeof out);

    if (agorahex_parse_envelope(json, jl, &out) != AGORAHEX_OK) {
        free(json);
        return fail("parse marshaled");
    }
    free(json);
    if (out.kind != AGORAHEX_KIND_AVC_START_CONTENT_REPLAY) {
        agorahex_message_free(&out);
        return fail("roundtrip kind");
    }
    if (!out.u.avc_start_content_replay.call_id || strcmp(out.u.avc_start_content_replay.call_id, "abc") != 0 ||
        !out.u.avc_start_content_replay.accept) {
        agorahex_message_free(&out);
        return fail("roundtrip fields");
    }
    agorahex_message_free(&out);
    return 0;
}
