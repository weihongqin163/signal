/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#include "agorahex/envelope.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *m) {
    fprintf(stderr, "FAIL: %s\n", m);
    return 1;
}

static int test_avc_display_name_empty_value(void) {
    const char *raw =
        "{\"AVCDialInRequest\":{\"avcEndpoint\":{\"avcEndpoint\":{\"displayName\":\"\"}}}}";
    agorahex_message_t m;
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(raw, strlen(raw), &m) != AGORAHEX_OK) {
        return fail("parse empty AVC display name");
    }
    const char *display_name = m.u.avc_dial_in_request.avc_endpoint.avc_endpoint.display_name;
    if (!display_name || display_name[0] != '\0') {
        agorahex_message_free(&m);
        return fail("empty AVC display name");
    }
    agorahex_message_free(&m);
    return 0;
}

static int test_avc_display_name_roundtrip(void) {
    const char *raw =
        "{\"AVCDialInRequest\":{\"avcEndpoint\":{\"avcEndpoint\":{\"displayName\":\"Room A\"}}}}";
    agorahex_message_t m;
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(raw, strlen(raw), &m) != AGORAHEX_OK) {
        return fail("parse AVC display name");
    }
    const char *display_name = m.u.avc_dial_in_request.avc_endpoint.avc_endpoint.display_name;
    if (!display_name || strcmp(display_name, "Room A") != 0) {
        agorahex_message_free(&m);
        return fail("AVC display name field");
    }

    char *json = NULL;
    size_t json_len = 0;
    if (agorahex_marshal_envelope(&m, &json, &json_len) != AGORAHEX_OK) {
        agorahex_message_free(&m);
        return fail("marshal AVC display name");
    }
    agorahex_message_free(&m);

    cJSON *root = cJSON_ParseWithLength(json, json_len);
    const cJSON *body = cJSON_GetObjectItemCaseSensitive(root, "AVCDialInRequest");
    const cJSON *endpoint = cJSON_GetObjectItemCaseSensitive(body, "avcEndpoint");
    const cJSON *signal_leg = cJSON_GetObjectItemCaseSensitive(endpoint, "avcEndpoint");
    const cJSON *marshaled_name = cJSON_GetObjectItemCaseSensitive(signal_leg, "displayName");
    if (!cJSON_IsString(marshaled_name) || strcmp(cJSON_GetStringValue(marshaled_name), "Room A") != 0) {
        cJSON_Delete(root);
        free(json);
        return fail("marshaled AVC display name path");
    }
    cJSON_Delete(root);

    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(json, json_len, &m) != AGORAHEX_OK) {
        free(json);
        return fail("parse marshaled AVC display name");
    }
    free(json);
    display_name = m.u.avc_dial_in_request.avc_endpoint.avc_endpoint.display_name;
    if (!display_name || strcmp(display_name, "Room A") != 0) {
        agorahex_message_free(&m);
        return fail("roundtrip AVC display name");
    }
    agorahex_message_free(&m);
    return 0;
}

static int test_dtmf_indication_parse(void) {
    const char *raw =
        "{\"DTMFIndication\":{\"callId\":\"89b559b9-a4bb-46ff-b819-9ba67b892cdb\",\"event\":\"2\"}}";
    agorahex_message_t m;
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(raw, strlen(raw), &m) != AGORAHEX_OK) {
        return fail("parse DTMF indication");
    }
    if (m.kind != AGORAHEX_KIND_DTMF_INDICATION) {
        agorahex_message_free(&m);
        return fail("DTMF indication kind");
    }
    const agorahex_dtmf_indication_t *dtmf = &m.u.dtmf_indication;
    if (!dtmf->call_id || strcmp(dtmf->call_id, "89b559b9-a4bb-46ff-b819-9ba67b892cdb") != 0 ||
        !dtmf->event || strcmp(dtmf->event, "2") != 0) {
        agorahex_message_free(&m);
        return fail("DTMF indication fields");
    }
    agorahex_message_free(&m);
    return 0;
}

static int resolution_equals(const agorahex_avc_resolution_t *resolution, int width, int height) {
    return resolution->avc_width == width && resolution->avc_height == height;
}

static int json_resolution_equals(const cJSON *body, const char *name, int width, int height) {
    const cJSON *resolution = cJSON_GetObjectItemCaseSensitive(body, name);
    const cJSON *avc_width = cJSON_GetObjectItemCaseSensitive(resolution, "avcWidth");
    const cJSON *avc_height = cJSON_GetObjectItemCaseSensitive(resolution, "avcHeight");
    return cJSON_IsNumber(avc_width) && (int)cJSON_GetNumberValue(avc_width) == width &&
           cJSON_IsNumber(avc_height) && (int)cJSON_GetNumberValue(avc_height) == height;
}

static int test_avc_dial_in_reply_max_resolution_roundtrip(void) {
    const char *raw =
        "{\"AVCDialInReply\":{\"maxPeopleResolution\":{\"avcWidth\":640,\"avcHeight\":360},"
        "\"maxContentResolution\":{\"avcWidth\":1280,\"avcHeight\":720}}}";
    agorahex_message_t m;
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(raw, strlen(raw), &m) != AGORAHEX_OK) {
        return fail("parse AVC dial-in reply max resolution");
    }
    const agorahex_avc_dial_in_reply_t *reply = &m.u.avc_dial_in_reply;
    if (!resolution_equals(&reply->max_people_resolution, 640, 360) ||
        !resolution_equals(&reply->max_content_resolution, 1280, 720)) {
        agorahex_message_free(&m);
        return fail("AVC dial-in reply max resolution fields");
    }

    char *json = NULL;
    size_t json_len = 0;
    if (agorahex_marshal_envelope(&m, &json, &json_len) != AGORAHEX_OK) {
        agorahex_message_free(&m);
        return fail("marshal AVC dial-in reply max resolution");
    }
    agorahex_message_free(&m);

    cJSON *root = cJSON_ParseWithLength(json, json_len);
    const cJSON *body = cJSON_GetObjectItemCaseSensitive(root, "AVCDialInReply");
    if (!json_resolution_equals(body, "maxPeopleResolution", 640, 360) ||
        !json_resolution_equals(body, "maxContentResolution", 1280, 720)) {
        cJSON_Delete(root);
        free(json);
        return fail("marshaled AVC dial-in reply max resolution paths");
    }
    cJSON_Delete(root);

    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(json, json_len, &m) != AGORAHEX_OK) {
        free(json);
        return fail("parse marshaled AVC dial-in reply max resolution");
    }
    free(json);
    reply = &m.u.avc_dial_in_reply;
    if (!resolution_equals(&reply->max_people_resolution, 640, 360) ||
        !resolution_equals(&reply->max_content_resolution, 1280, 720)) {
        agorahex_message_free(&m);
        return fail("roundtrip AVC dial-in reply max resolution");
    }
    agorahex_message_free(&m);
    return 0;
}

int main(void) {
    if (test_avc_display_name_empty_value() != 0 || test_avc_display_name_roundtrip() != 0 ||
        test_dtmf_indication_parse() != 0 || test_avc_dial_in_reply_max_resolution_roundtrip() != 0) {
        return 1;
    }

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
    out.u.avc_start_content_replay.call_id = agorahex_strdup("abc");
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
