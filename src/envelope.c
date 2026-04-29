/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#include "agorahex/envelope.h"

#include "cJSON.h"

#include <stdlib.h>
#include <string.h>

static char *dup_or_null(const char *s) {
    if (!s) {
        return NULL;
    }
    return strdup(s);
}

static void free_display_info(agorahex_display_info_t *d) {
    free(d->name);
    free(d->url);
    free(d->conference_id);
    memset(d, 0, sizeof(*d));
}

static void free_audio_to_mcu(agorahex_audio_to_mcu_t *a) {
    free(a->media_id);
    memset(a, 0, sizeof(*a));
}

static void free_audio_from_mcu(agorahex_audio_from_mcu_t *a) {
    free(a->media_id);
    memset(a, 0, sizeof(*a));
}

static void free_video_track(agorahex_video_track_t *v) {
    free(v->media_id);
    memset(v, 0, sizeof(*v));
}

void agorahex_message_free(agorahex_message_t *m) {
    if (!m) {
        return;
    }
    switch (m->kind) {
    case AGORAHEX_KIND_AGORA_DIAL_IN_INDICATION: {
        agorahex_agora_dial_in_indication_t *x = &m->u.agora_dial_in_indication;
        free(x->call_id);
        free_display_info(&x->agora_endpoint.agora_endpoint);
        free_audio_to_mcu(&x->agora_endpoint.to_mcu_audio);
        free_audio_from_mcu(&x->agora_endpoint.from_mcu_audio);
        free_video_track(&x->agora_endpoint.to_gw_video);
        free_video_track(&x->agora_endpoint.from_gw_video);
        free_video_track(&x->agora_endpoint.to_gw_content);
        free_video_track(&x->agora_endpoint.from_gw_content);
        free(x->avc_leg.signal_type);
        free(x->avc_leg.ipv4);
        free(x->avc_leg.ipv6);
        free(x->avc_leg.e164);
        free(x->avc_leg.conference_id);
        free(x->avc_leg.url);
        memset(x, 0, sizeof(*x));
        break;
    }
    case AGORAHEX_KIND_AVC_DIAL_IN_REQUEST: {
        agorahex_avc_dial_in_request_t *x = &m->u.avc_dial_in_request;
        free(x->call_id);
        free(x->conference_id);
        free(x->password);
        free(x->avc_endpoint.avc_endpoint.signal_type);
        free(x->avc_endpoint.avc_endpoint.ipv4);
        free(x->avc_endpoint.avc_endpoint.ipv6);
        free(x->avc_endpoint.avc_endpoint.e164);
        free(x->avc_endpoint.avc_endpoint.conference_id);
        free(x->avc_endpoint.avc_endpoint.url);
        free(x->avc_endpoint.people_property.media_id);
        free(x->avc_endpoint.content_property.media_id);
        memset(x, 0, sizeof(*x));
        break;
    }
    case AGORAHEX_KIND_AVC_DIAL_IN_REPLY: {
        agorahex_avc_dial_in_reply_t *x = &m->u.avc_dial_in_reply;
        free(x->call_id);
        free_display_info(&x->agora_endpoint.agora_endpoint);
        free_audio_to_mcu(&x->agora_endpoint.to_mcu_audio);
        free_audio_from_mcu(&x->agora_endpoint.from_mcu_audio);
        free_video_track(&x->agora_endpoint.to_gw_video);
        free_video_track(&x->agora_endpoint.from_gw_video);
        free_video_track(&x->agora_endpoint.to_gw_content);
        free_video_track(&x->agora_endpoint.from_gw_content);
        memset(x, 0, sizeof(*x));
        break;
    }
    case AGORAHEX_KIND_HANGUP_INDICATION:
        free(m->u.hangup_indication.call_id);
        memset(&m->u.hangup_indication, 0, sizeof(m->u.hangup_indication));
        break;
    case AGORAHEX_KIND_MUTED_INDICATION:
        free(m->u.muted_indication.call_id);
        memset(&m->u.muted_indication, 0, sizeof(m->u.muted_indication));
        break;
    case AGORAHEX_KIND_AVC_NAME_CHANGED_INDICATION:
        free(m->u.avc_name_changed_indication.call_id);
        free(m->u.avc_name_changed_indication.name);
        memset(&m->u.avc_name_changed_indication, 0, sizeof(m->u.avc_name_changed_indication));
        break;
    case AGORAHEX_KIND_AGORA_START_CONTENT_INDICATION:
        free(m->u.agora_start_content_indication.call_id);
        memset(&m->u.agora_start_content_indication, 0, sizeof(m->u.agora_start_content_indication));
        break;
    case AGORAHEX_KIND_AVC_START_CONTENT_REQUEST:
        free(m->u.avc_start_content_request.call_id);
        memset(&m->u.avc_start_content_request, 0, sizeof(m->u.avc_start_content_request));
        break;
    case AGORAHEX_KIND_AVC_START_CONTENT_REPLAY:
        free(m->u.avc_start_content_replay.call_id);
        memset(&m->u.avc_start_content_replay, 0, sizeof(m->u.avc_start_content_replay));
        break;
    case AGORAHEX_KIND_STOP_CONTENT_INDICATION:
        free(m->u.stop_content_indication.call_id);
        memset(&m->u.stop_content_indication, 0, sizeof(m->u.stop_content_indication));
        break;
    default:
        break;
    }
    memset(m, 0, sizeof(*m));
}

const char *agorahex_kind_cstr(agorahex_kind_t k) {
    switch (k) {
    case AGORAHEX_KIND_AGORA_DIAL_IN_INDICATION:
        return "AgoraDialInIndication";
    case AGORAHEX_KIND_AVC_DIAL_IN_REQUEST:
        return "AVCDialInRequest";
    case AGORAHEX_KIND_AVC_DIAL_IN_REPLY:
        return "AVCDialInReply";
    case AGORAHEX_KIND_HANGUP_INDICATION:
        return "HangupIndication";
    case AGORAHEX_KIND_MUTED_INDICATION:
        return "MutedIndication";
    case AGORAHEX_KIND_AVC_NAME_CHANGED_INDICATION:
        return "AVCNameChangedIndication";
    case AGORAHEX_KIND_AGORA_START_CONTENT_INDICATION:
        return "AgoraStartContentIndication";
    case AGORAHEX_KIND_AVC_START_CONTENT_REQUEST:
        return "AVCStartContentRequest";
    case AGORAHEX_KIND_AVC_START_CONTENT_REPLAY:
        return "AVCStartContentReplay";
    case AGORAHEX_KIND_STOP_CONTENT_INDICATION:
        return "StopContentIndication";
    default:
        return "";
    }
}

static agorahex_kind_t kind_from_cstr(const char *k) {
    if (!k) {
        return (agorahex_kind_t)-1;
    }
    if (strcmp(k, "AgoraDialInIndication") == 0) {
        return AGORAHEX_KIND_AGORA_DIAL_IN_INDICATION;
    }
    if (strcmp(k, "AVCDialInRequest") == 0) {
        return AGORAHEX_KIND_AVC_DIAL_IN_REQUEST;
    }
    if (strcmp(k, "AVCDialInReply") == 0) {
        return AGORAHEX_KIND_AVC_DIAL_IN_REPLY;
    }
    if (strcmp(k, "HangupIndication") == 0) {
        return AGORAHEX_KIND_HANGUP_INDICATION;
    }
    if (strcmp(k, "MutedIndication") == 0) {
        return AGORAHEX_KIND_MUTED_INDICATION;
    }
    if (strcmp(k, "AVCNameChangedIndication") == 0) {
        return AGORAHEX_KIND_AVC_NAME_CHANGED_INDICATION;
    }
    if (strcmp(k, "AgoraStartContentIndication") == 0) {
        return AGORAHEX_KIND_AGORA_START_CONTENT_INDICATION;
    }
    if (strcmp(k, "AVCStartContentRequest") == 0) {
        return AGORAHEX_KIND_AVC_START_CONTENT_REQUEST;
    }
    if (strcmp(k, "AVCStartContentReplay") == 0) {
        return AGORAHEX_KIND_AVC_START_CONTENT_REPLAY;
    }
    if (strcmp(k, "StopContentIndication") == 0) {
        return AGORAHEX_KIND_STOP_CONTENT_INDICATION;
    }
    return (agorahex_kind_t)-1;
}

static int object_key_count(const cJSON *o) {
    int n = 0;
    for (const cJSON *c = o->child; c; c = c->next) {
        n++;
    }
    return n;
}

static void parse_display_info(const cJSON *obj, agorahex_display_info_t *out) {
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }
    const cJSON *it = NULL;
    it = cJSON_GetObjectItemCaseSensitive(obj, "name");
    if (cJSON_IsString(it)) {
        out->name = dup_or_null(cJSON_GetStringValue(it));
    }
    it = cJSON_GetObjectItemCaseSensitive(obj, "url");
    if (cJSON_IsString(it)) {
        out->url = dup_or_null(cJSON_GetStringValue(it));
    }
    it = cJSON_GetObjectItemCaseSensitive(obj, "conferenceId");
    if (cJSON_IsString(it)) {
        out->conference_id = dup_or_null(cJSON_GetStringValue(it));
    }
}

static void parse_audio_to_mcu(const cJSON *obj, agorahex_audio_to_mcu_t *out) {
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, "mediaId");
    if (cJSON_IsString(it)) {
        out->media_id = dup_or_null(cJSON_GetStringValue(it));
    }
    it = cJSON_GetObjectItemCaseSensitive(obj, "muted");
    if (cJSON_IsBool(it)) {
        out->muted = cJSON_IsTrue(it);
    }
}

static void parse_audio_from_mcu(const cJSON *obj, agorahex_audio_from_mcu_t *out) {
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, "mediaId");
    if (cJSON_IsString(it)) {
        out->media_id = dup_or_null(cJSON_GetStringValue(it));
    }
}

static void parse_video_track(const cJSON *obj, agorahex_video_track_t *out) {
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, "mediaId");
    if (cJSON_IsString(it)) {
        out->media_id = dup_or_null(cJSON_GetStringValue(it));
    }
    it = cJSON_GetObjectItemCaseSensitive(obj, "bitrate");
    if (cJSON_IsNumber(it)) {
        out->bitrate = (int)cJSON_GetNumberValue(it);
    }
    it = cJSON_GetObjectItemCaseSensitive(obj, "width");
    if (cJSON_IsNumber(it)) {
        out->width = (int)cJSON_GetNumberValue(it);
    }
    it = cJSON_GetObjectItemCaseSensitive(obj, "height");
    if (cJSON_IsNumber(it)) {
        out->height = (int)cJSON_GetNumberValue(it);
    }
    it = cJSON_GetObjectItemCaseSensitive(obj, "fps");
    if (cJSON_IsNumber(it)) {
        out->fps = (int)cJSON_GetNumberValue(it);
    }
}

static void parse_media_endpoint(const cJSON *obj, agorahex_media_endpoint_t *out) {
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }
    const cJSON *outer = cJSON_GetObjectItemCaseSensitive(obj, "agoraEndpoint");
    if (cJSON_IsObject(outer)) {
        const cJSON *disp = cJSON_GetObjectItemCaseSensitive(outer, "agoraEndpoint");
        if (cJSON_IsObject(disp)) {
            parse_display_info(disp, &out->agora_endpoint);
        }
    }
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, "callBitrate");
    if (cJSON_IsNumber(it)) {
        out->call_bitrate = (int)cJSON_GetNumberValue(it);
    }
    it = cJSON_GetObjectItemCaseSensitive(obj, "isAudioOnly");
    if (cJSON_IsBool(it)) {
        out->is_audio_only = cJSON_IsTrue(it);
    }
    parse_audio_to_mcu(cJSON_GetObjectItemCaseSensitive(obj, "toMcuAudioProperty"), &out->to_mcu_audio);
    parse_audio_from_mcu(cJSON_GetObjectItemCaseSensitive(obj, "fromMcuAudioProperty"), &out->from_mcu_audio);
    parse_video_track(cJSON_GetObjectItemCaseSensitive(obj, "toGWVideoProperty"), &out->to_gw_video);
    parse_video_track(cJSON_GetObjectItemCaseSensitive(obj, "fromGWVideoProperty"), &out->from_gw_video);
    parse_video_track(cJSON_GetObjectItemCaseSensitive(obj, "toGWContentProperty"), &out->to_gw_content);
    parse_video_track(cJSON_GetObjectItemCaseSensitive(obj, "fromGWContentProperty"), &out->from_gw_content);
}

static void parse_avc_signal_leg(const cJSON *obj, agorahex_avc_signal_leg_t *out) {
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }
    const cJSON *inner = cJSON_GetObjectItemCaseSensitive(obj, "avcEndpoint");
    const cJSON *src = cJSON_IsObject(inner) ? inner : obj;
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(src, "signalType");
    if (cJSON_IsString(it)) {
        out->signal_type = dup_or_null(cJSON_GetStringValue(it));
    }
    it = cJSON_GetObjectItemCaseSensitive(src, "encryption");
    if (cJSON_IsBool(it)) {
        out->encryption = cJSON_IsTrue(it);
    }
    it = cJSON_GetObjectItemCaseSensitive(src, "ipv4");
    if (cJSON_IsString(it)) {
        out->ipv4 = dup_or_null(cJSON_GetStringValue(it));
    }
    it = cJSON_GetObjectItemCaseSensitive(src, "ipv6");
    if (cJSON_IsString(it)) {
        out->ipv6 = dup_or_null(cJSON_GetStringValue(it));
    }
    it = cJSON_GetObjectItemCaseSensitive(src, "e164");
    if (cJSON_IsString(it)) {
        out->e164 = dup_or_null(cJSON_GetStringValue(it));
    }
    it = cJSON_GetObjectItemCaseSensitive(src, "conferenceId");
    if (cJSON_IsString(it)) {
        out->conference_id = dup_or_null(cJSON_GetStringValue(it));
    }
    it = cJSON_GetObjectItemCaseSensitive(src, "url");
    if (cJSON_IsString(it)) {
        out->url = dup_or_null(cJSON_GetStringValue(it));
    }
}

static void parse_media_capabilities(const cJSON *obj, agorahex_media_capabilities_t *out) {
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, "mediaId");
    if (cJSON_IsString(it)) {
        out->media_id = dup_or_null(cJSON_GetStringValue(it));
    }
    it = cJSON_GetObjectItemCaseSensitive(obj, "bitrate");
    if (cJSON_IsNumber(it)) {
        out->bitrate = (int)cJSON_GetNumberValue(it);
    }
    it = cJSON_GetObjectItemCaseSensitive(obj, "width");
    if (cJSON_IsNumber(it)) {
        out->width = (int)cJSON_GetNumberValue(it);
    }
    it = cJSON_GetObjectItemCaseSensitive(obj, "height");
    if (cJSON_IsNumber(it)) {
        out->height = (int)cJSON_GetNumberValue(it);
    }
    it = cJSON_GetObjectItemCaseSensitive(obj, "fps");
    if (cJSON_IsNumber(it)) {
        out->fps = (int)cJSON_GetNumberValue(it);
    }
}

static void parse_avc_dial_endpoint(const cJSON *obj, agorahex_avc_dial_endpoint_t *out) {
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }
    const cJSON *legwrap = cJSON_GetObjectItemCaseSensitive(obj, "avcEndpoint");
    if (cJSON_IsObject(legwrap)) {
        parse_avc_signal_leg(legwrap, &out->avc_endpoint);
    }
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, "isAudioOnly");
    if (cJSON_IsBool(it)) {
        out->is_audio_only = cJSON_IsTrue(it);
    }
    parse_media_capabilities(cJSON_GetObjectItemCaseSensitive(obj, "peopleProperty"), &out->people_property);
    parse_media_capabilities(cJSON_GetObjectItemCaseSensitive(obj, "contentProperty"), &out->content_property);
}

static agorahex_result_t parse_kind_body(agorahex_kind_t kind, const cJSON *body, agorahex_message_t *out) {
    out->kind = kind;
    if (!cJSON_IsObject(body)) {
        return AGORAHEX_ERR_JSON_PARSE;
    }
    switch (kind) {
    case AGORAHEX_KIND_AGORA_DIAL_IN_INDICATION: {
        agorahex_agora_dial_in_indication_t *x = &out->u.agora_dial_in_indication;
        const cJSON *it = cJSON_GetObjectItemCaseSensitive(body, "callId");
        if (cJSON_IsString(it)) {
            x->call_id = dup_or_null(cJSON_GetStringValue(it));
        }
        parse_media_endpoint(cJSON_GetObjectItemCaseSensitive(body, "agoraEndpoint"), &x->agora_endpoint);
        const cJSON *leg = cJSON_GetObjectItemCaseSensitive(body, "avcLeg");
        if (cJSON_IsObject(leg)) {
            parse_avc_signal_leg(leg, &x->avc_leg);
        }
        return AGORAHEX_OK;
    }
    case AGORAHEX_KIND_AVC_DIAL_IN_REQUEST: {
        agorahex_avc_dial_in_request_t *x = &out->u.avc_dial_in_request;
        const cJSON *it = cJSON_GetObjectItemCaseSensitive(body, "callId");
        if (cJSON_IsString(it)) {
            x->call_id = dup_or_null(cJSON_GetStringValue(it));
        }
        parse_avc_dial_endpoint(cJSON_GetObjectItemCaseSensitive(body, "avcEndpoint"), &x->avc_endpoint);
        it = cJSON_GetObjectItemCaseSensitive(body, "conferenceId");
        if (cJSON_IsString(it)) {
            x->conference_id = dup_or_null(cJSON_GetStringValue(it));
        }
        it = cJSON_GetObjectItemCaseSensitive(body, "password");
        if (cJSON_IsString(it)) {
            x->password = dup_or_null(cJSON_GetStringValue(it));
        }
        return AGORAHEX_OK;
    }
    case AGORAHEX_KIND_AVC_DIAL_IN_REPLY: {
        agorahex_avc_dial_in_reply_t *x = &out->u.avc_dial_in_reply;
        const cJSON *it = cJSON_GetObjectItemCaseSensitive(body, "callId");
        if (cJSON_IsString(it)) {
            x->call_id = dup_or_null(cJSON_GetStringValue(it));
        }
        it = cJSON_GetObjectItemCaseSensitive(body, "returnCode");
        if (cJSON_IsNumber(it)) {
            x->return_code = (int)cJSON_GetNumberValue(it);
        }
        parse_media_endpoint(cJSON_GetObjectItemCaseSensitive(body, "agoraEndpoint"), &x->agora_endpoint);
        return AGORAHEX_OK;
    }
    case AGORAHEX_KIND_HANGUP_INDICATION: {
        agorahex_hangup_indication_t *x = &out->u.hangup_indication;
        const cJSON *it = cJSON_GetObjectItemCaseSensitive(body, "callId");
        if (cJSON_IsString(it)) {
            x->call_id = dup_or_null(cJSON_GetStringValue(it));
        }
        it = cJSON_GetObjectItemCaseSensitive(body, "dropCode");
        if (cJSON_IsNumber(it)) {
            x->drop_code = (int)cJSON_GetNumberValue(it);
        }
        return AGORAHEX_OK;
    }
    case AGORAHEX_KIND_MUTED_INDICATION: {
        agorahex_muted_indication_t *x = &out->u.muted_indication;
        const cJSON *it = cJSON_GetObjectItemCaseSensitive(body, "callId");
        if (cJSON_IsString(it)) {
            x->call_id = dup_or_null(cJSON_GetStringValue(it));
        }
        it = cJSON_GetObjectItemCaseSensitive(body, "muted");
        if (cJSON_IsBool(it)) {
            x->muted = cJSON_IsTrue(it);
        }
        return AGORAHEX_OK;
    }
    case AGORAHEX_KIND_AVC_NAME_CHANGED_INDICATION: {
        agorahex_avc_name_changed_indication_t *x = &out->u.avc_name_changed_indication;
        const cJSON *it = cJSON_GetObjectItemCaseSensitive(body, "callId");
        if (cJSON_IsString(it)) {
            x->call_id = dup_or_null(cJSON_GetStringValue(it));
        }
        it = cJSON_GetObjectItemCaseSensitive(body, "name");
        if (cJSON_IsString(it)) {
            x->name = dup_or_null(cJSON_GetStringValue(it));
        }
        return AGORAHEX_OK;
    }
    case AGORAHEX_KIND_AGORA_START_CONTENT_INDICATION: {
        agorahex_agora_start_content_indication_t *x = &out->u.agora_start_content_indication;
        const cJSON *it = cJSON_GetObjectItemCaseSensitive(body, "callId");
        if (cJSON_IsString(it)) {
            x->call_id = dup_or_null(cJSON_GetStringValue(it));
        }
        return AGORAHEX_OK;
    }
    case AGORAHEX_KIND_AVC_START_CONTENT_REQUEST: {
        agorahex_avc_start_content_request_t *x = &out->u.avc_start_content_request;
        const cJSON *it = cJSON_GetObjectItemCaseSensitive(body, "callId");
        if (cJSON_IsString(it)) {
            x->call_id = dup_or_null(cJSON_GetStringValue(it));
        }
        return AGORAHEX_OK;
    }
    case AGORAHEX_KIND_AVC_START_CONTENT_REPLAY: {
        agorahex_avc_start_content_replay_t *x = &out->u.avc_start_content_replay;
        const cJSON *it = cJSON_GetObjectItemCaseSensitive(body, "callId");
        if (cJSON_IsString(it)) {
            x->call_id = dup_or_null(cJSON_GetStringValue(it));
        }
        it = cJSON_GetObjectItemCaseSensitive(body, "accept");
        if (cJSON_IsBool(it)) {
            x->accept = cJSON_IsTrue(it);
        }
        return AGORAHEX_OK;
    }
    case AGORAHEX_KIND_STOP_CONTENT_INDICATION: {
        agorahex_stop_content_indication_t *x = &out->u.stop_content_indication;
        const cJSON *it = cJSON_GetObjectItemCaseSensitive(body, "callId");
        if (cJSON_IsString(it)) {
            x->call_id = dup_or_null(cJSON_GetStringValue(it));
        }
        return AGORAHEX_OK;
    }
    default:
        return AGORAHEX_ERR_UNKNOWN_KIND;
    }
}

agorahex_result_t agorahex_parse_envelope(const char *buf, size_t buf_len, agorahex_message_t *out) {
    if (!out) {
        return AGORAHEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (!buf && buf_len > 0) {
        return AGORAHEX_ERR_INVALID_ARG;
    }
    char *tmp = (char *)malloc(buf_len + 1u);
    if (!tmp) {
        return AGORAHEX_ERR_NO_MEMORY;
    }
    if (buf_len > 0 && buf) {
        memcpy(tmp, buf, buf_len);
    }
    tmp[buf_len] = '\0';

    cJSON *root = cJSON_ParseWithLength(tmp, buf_len);
    free(tmp);
    if (!root) {
        return AGORAHEX_ERR_JSON_PARSE;
    }
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return AGORAHEX_ERR_ENVELOPE_NOT_OBJECT;
    }
    if (object_key_count(root) != 1) {
        cJSON_Delete(root);
        return AGORAHEX_ERR_ENVELOPE_KEY_COUNT;
    }
    cJSON *only = root->child;
    const char *key = only->string;
    agorahex_kind_t kind = kind_from_cstr(key);
    if ((int)kind < 0) {
        cJSON_Delete(root);
        return AGORAHEX_ERR_UNKNOWN_KIND;
    }
    agorahex_result_t pr = parse_kind_body(kind, only, out);
    cJSON_Delete(root);
    if (pr != AGORAHEX_OK) {
        agorahex_message_free(out);
        memset(out, 0, sizeof(*out));
    }
    return pr;
}

static cJSON *json_display_info(const agorahex_display_info_t *d) {
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    if (d->name) {
        cJSON_AddStringToObject(o, "name", d->name);
    }
    if (d->url) {
        cJSON_AddStringToObject(o, "url", d->url);
    }
    if (d->conference_id) {
        cJSON_AddStringToObject(o, "conferenceId", d->conference_id);
    }
    return o;
}

static cJSON *json_audio_to_mcu(const agorahex_audio_to_mcu_t *a) {
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    if (a->media_id) {
        cJSON_AddStringToObject(o, "mediaId", a->media_id);
    }
    cJSON_AddBoolToObject(o, "muted", a->muted ? 1 : 0);
    return o;
}

static cJSON *json_audio_from_mcu(const agorahex_audio_from_mcu_t *a) {
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    if (a->media_id) {
        cJSON_AddStringToObject(o, "mediaId", a->media_id);
    }
    return o;
}

static cJSON *json_video_track(const agorahex_video_track_t *v) {
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    if (v->media_id) {
        cJSON_AddStringToObject(o, "mediaId", v->media_id);
    }
    cJSON_AddNumberToObject(o, "bitrate", (double)v->bitrate);
    cJSON_AddNumberToObject(o, "width", (double)v->width);
    cJSON_AddNumberToObject(o, "height", (double)v->height);
    cJSON_AddNumberToObject(o, "fps", (double)v->fps);
    return o;
}

static cJSON *json_media_endpoint(const agorahex_media_endpoint_t *m) {
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    cJSON *inner = json_display_info(&m->agora_endpoint);
    if (!inner) {
        cJSON_Delete(o);
        return NULL;
    }
    cJSON *wrap = cJSON_CreateObject();
    if (!wrap) {
        cJSON_Delete(inner);
        cJSON_Delete(o);
        return NULL;
    }
    cJSON_AddItemToObject(wrap, "agoraEndpoint", inner);
    cJSON_AddItemToObject(o, "agoraEndpoint", wrap);
    cJSON_AddNumberToObject(o, "callBitrate", (double)m->call_bitrate);
    cJSON_AddBoolToObject(o, "isAudioOnly", m->is_audio_only ? 1 : 0);
    cJSON *t1 = json_audio_to_mcu(&m->to_mcu_audio);
    cJSON *t2 = json_audio_from_mcu(&m->from_mcu_audio);
    cJSON *v1 = json_video_track(&m->to_gw_video);
    cJSON *v2 = json_video_track(&m->from_gw_video);
    cJSON *v3 = json_video_track(&m->to_gw_content);
    cJSON *v4 = json_video_track(&m->from_gw_content);
    if (!t1 || !t2 || !v1 || !v2 || !v3 || !v4) {
        cJSON_Delete(t1);
        cJSON_Delete(t2);
        cJSON_Delete(v1);
        cJSON_Delete(v2);
        cJSON_Delete(v3);
        cJSON_Delete(v4);
        cJSON_Delete(o);
        return NULL;
    }
    cJSON_AddItemToObject(o, "toMcuAudioProperty", t1);
    cJSON_AddItemToObject(o, "fromMcuAudioProperty", t2);
    cJSON_AddItemToObject(o, "toGWVideoProperty", v1);
    cJSON_AddItemToObject(o, "fromGWVideoProperty", v2);
    cJSON_AddItemToObject(o, "toGWContentProperty", v3);
    cJSON_AddItemToObject(o, "fromGWContentProperty", v4);
    return o;
}

static cJSON *json_avc_signal_inner(const agorahex_avc_signal_leg_t *leg) {
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    if (leg->signal_type) {
        cJSON_AddStringToObject(o, "signalType", leg->signal_type);
    }
    cJSON_AddBoolToObject(o, "encryption", leg->encryption ? 1 : 0);
    if (leg->ipv4) {
        cJSON_AddStringToObject(o, "ipv4", leg->ipv4);
    }
    if (leg->ipv6) {
        cJSON_AddStringToObject(o, "ipv6", leg->ipv6);
    }
    if (leg->e164) {
        cJSON_AddStringToObject(o, "e164", leg->e164);
    }
    if (leg->conference_id) {
        cJSON_AddStringToObject(o, "conferenceId", leg->conference_id);
    }
    if (leg->url) {
        cJSON_AddStringToObject(o, "url", leg->url);
    }
    return o;
}

static cJSON *json_avc_signal_wrapped(const agorahex_avc_signal_leg_t *leg) {
    cJSON *inner = json_avc_signal_inner(leg);
    if (!inner) {
        return NULL;
    }
    cJSON *w = cJSON_CreateObject();
    if (!w) {
        cJSON_Delete(inner);
        return NULL;
    }
    cJSON_AddItemToObject(w, "avcEndpoint", inner);
    return w;
}

static cJSON *json_media_caps(const agorahex_media_capabilities_t *c) {
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    if (c->media_id) {
        cJSON_AddStringToObject(o, "mediaId", c->media_id);
    }
    cJSON_AddNumberToObject(o, "bitrate", (double)c->bitrate);
    cJSON_AddNumberToObject(o, "width", (double)c->width);
    cJSON_AddNumberToObject(o, "height", (double)c->height);
    cJSON_AddNumberToObject(o, "fps", (double)c->fps);
    return o;
}

static cJSON *json_avc_dial_endpoint(const agorahex_avc_dial_endpoint_t *e) {
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    cJSON *leg = json_avc_signal_wrapped(&e->avc_endpoint);
    cJSON *p = json_media_caps(&e->people_property);
    cJSON *c = json_media_caps(&e->content_property);
    if (!leg || !p || !c) {
        cJSON_Delete(leg);
        cJSON_Delete(p);
        cJSON_Delete(c);
        cJSON_Delete(o);
        return NULL;
    }
    cJSON_AddItemToObject(o, "avcEndpoint", leg);
    cJSON_AddBoolToObject(o, "isAudioOnly", e->is_audio_only ? 1 : 0);
    cJSON_AddItemToObject(o, "peopleProperty", p);
    cJSON_AddItemToObject(o, "contentProperty", c);
    return o;
}

agorahex_result_t agorahex_marshal_envelope(const agorahex_message_t *msg, char **out_json, size_t *out_len) {
    if (!msg || !out_json || !out_len) {
        return AGORAHEX_ERR_INVALID_ARG;
    }
    *out_json = NULL;
    *out_len = 0;

    const char *k = agorahex_kind_cstr(msg->kind);
    if (!k || k[0] == '\0') {
        return AGORAHEX_ERR_UNKNOWN_KIND;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *body = NULL;
    if (!root) {
        return AGORAHEX_ERR_NO_MEMORY;
    }

    switch (msg->kind) {
    case AGORAHEX_KIND_AGORA_DIAL_IN_INDICATION: {
        const agorahex_agora_dial_in_indication_t *x = &msg->u.agora_dial_in_indication;
        body = cJSON_CreateObject();
        if (!body) {
            break;
        }
        if (x->call_id) {
            cJSON_AddStringToObject(body, "callId", x->call_id);
        }
        cJSON *ae = json_media_endpoint(&x->agora_endpoint);
        cJSON *leg = json_avc_signal_inner(&x->avc_leg);
        if (!ae || !leg) {
            cJSON_Delete(ae);
            cJSON_Delete(leg);
            cJSON_Delete(body);
            body = NULL;
            break;
        }
        cJSON_AddItemToObject(body, "agoraEndpoint", ae);
        cJSON_AddItemToObject(body, "avcLeg", leg);
        break;
    }
    case AGORAHEX_KIND_AVC_DIAL_IN_REQUEST: {
        const agorahex_avc_dial_in_request_t *x = &msg->u.avc_dial_in_request;
        body = cJSON_CreateObject();
        if (!body) {
            break;
        }
        if (x->call_id) {
            cJSON_AddStringToObject(body, "callId", x->call_id);
        }
        cJSON *ep = json_avc_dial_endpoint(&x->avc_endpoint);
        if (!ep) {
            cJSON_Delete(body);
            body = NULL;
            break;
        }
        cJSON_AddItemToObject(body, "avcEndpoint", ep);
        if (x->conference_id) {
            cJSON_AddStringToObject(body, "conferenceId", x->conference_id);
        }
        if (x->password) {
            cJSON_AddStringToObject(body, "password", x->password);
        }
        break;
    }
    case AGORAHEX_KIND_AVC_DIAL_IN_REPLY: {
        const agorahex_avc_dial_in_reply_t *x = &msg->u.avc_dial_in_reply;
        body = cJSON_CreateObject();
        if (!body) {
            break;
        }
        if (x->call_id) {
            cJSON_AddStringToObject(body, "callId", x->call_id);
        }
        cJSON_AddNumberToObject(body, "returnCode", (double)x->return_code);
        cJSON *ae = json_media_endpoint(&x->agora_endpoint);
        if (!ae) {
            cJSON_Delete(body);
            body = NULL;
            break;
        }
        cJSON_AddItemToObject(body, "agoraEndpoint", ae);
        break;
    }
    case AGORAHEX_KIND_HANGUP_INDICATION: {
        const agorahex_hangup_indication_t *x = &msg->u.hangup_indication;
        body = cJSON_CreateObject();
        if (!body) {
            break;
        }
        if (x->call_id) {
            cJSON_AddStringToObject(body, "callId", x->call_id);
        }
        cJSON_AddNumberToObject(body, "dropCode", (double)x->drop_code);
        break;
    }
    case AGORAHEX_KIND_MUTED_INDICATION: {
        const agorahex_muted_indication_t *x = &msg->u.muted_indication;
        body = cJSON_CreateObject();
        if (!body) {
            break;
        }
        if (x->call_id) {
            cJSON_AddStringToObject(body, "callId", x->call_id);
        }
        cJSON_AddBoolToObject(body, "muted", x->muted ? 1 : 0);
        break;
    }
    case AGORAHEX_KIND_AVC_NAME_CHANGED_INDICATION: {
        const agorahex_avc_name_changed_indication_t *x = &msg->u.avc_name_changed_indication;
        body = cJSON_CreateObject();
        if (!body) {
            break;
        }
        if (x->call_id) {
            cJSON_AddStringToObject(body, "callId", x->call_id);
        }
        if (x->name) {
            cJSON_AddStringToObject(body, "name", x->name);
        }
        break;
    }
    case AGORAHEX_KIND_AGORA_START_CONTENT_INDICATION: {
        const agorahex_agora_start_content_indication_t *x = &msg->u.agora_start_content_indication;
        body = cJSON_CreateObject();
        if (!body) {
            break;
        }
        if (x->call_id) {
            cJSON_AddStringToObject(body, "callId", x->call_id);
        }
        break;
    }
    case AGORAHEX_KIND_AVC_START_CONTENT_REQUEST: {
        const agorahex_avc_start_content_request_t *x = &msg->u.avc_start_content_request;
        body = cJSON_CreateObject();
        if (!body) {
            break;
        }
        if (x->call_id) {
            cJSON_AddStringToObject(body, "callId", x->call_id);
        }
        break;
    }
    case AGORAHEX_KIND_AVC_START_CONTENT_REPLAY: {
        const agorahex_avc_start_content_replay_t *x = &msg->u.avc_start_content_replay;
        body = cJSON_CreateObject();
        if (!body) {
            break;
        }
        if (x->call_id) {
            cJSON_AddStringToObject(body, "callId", x->call_id);
        }
        cJSON_AddBoolToObject(body, "accept", x->accept ? 1 : 0);
        break;
    }
    case AGORAHEX_KIND_STOP_CONTENT_INDICATION: {
        const agorahex_stop_content_indication_t *x = &msg->u.stop_content_indication;
        body = cJSON_CreateObject();
        if (!body) {
            break;
        }
        if (x->call_id) {
            cJSON_AddStringToObject(body, "callId", x->call_id);
        }
        break;
    }
    default:
        cJSON_Delete(root);
        return AGORAHEX_ERR_UNKNOWN_KIND;
    }

    if (!body) {
        cJSON_Delete(root);
        return AGORAHEX_ERR_NO_MEMORY;
    }
    cJSON_AddItemToObject(root, k, body);
    char *printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) {
        return AGORAHEX_ERR_NO_MEMORY;
    }
    *out_json = printed;
    *out_len = strlen(printed);
    return AGORAHEX_OK;
}
