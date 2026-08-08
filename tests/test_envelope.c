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

static size_t cjson_fail_allocation_size;

static void *fail_cjson_allocation(size_t size) {
    if (size == cjson_fail_allocation_size) {
        return NULL;
    }
    return malloc(size);
}

static void free_cjson_allocation(void *ptr) {
    free(ptr);
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

static int test_avc_user_agent_audio_roundtrip(void) {
    const char *raw =
        "{\"AVCDialInRequest\":{\"avcEndpoint\":{\"avcEndpoint\":{\"userAgent\":\"Polycom RealPresence "
        "Group 500\"},\"audioProperty\":{\"mediaId\":\"audio-1\",\"samplerate\":16000,\"channels\":2,\"bits\":24}}}}";
    agorahex_message_t m;
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(raw, strlen(raw), &m) != AGORAHEX_OK) {
        return fail("parse AVC user agent and audio property");
    }
    const agorahex_avc_dial_endpoint_t *endpoint = &m.u.avc_dial_in_request.avc_endpoint;
    if (!endpoint->avc_endpoint.user_agent ||
        strcmp(endpoint->avc_endpoint.user_agent, "Polycom RealPresence Group 500") != 0 ||
        !endpoint->audio_property.media_id || strcmp(endpoint->audio_property.media_id, "audio-1") != 0 ||
        endpoint->audio_property.sample_rate != 16000 || endpoint->audio_property.channels != 2 ||
        endpoint->audio_property.bits != 24) {
        agorahex_message_free(&m);
        return fail("AVC user agent and audio property fields");
    }

    char *json = NULL;
    size_t json_len = 0;
    if (agorahex_marshal_envelope(&m, &json, &json_len) != AGORAHEX_OK) {
        agorahex_message_free(&m);
        return fail("marshal AVC user agent and audio property");
    }
    agorahex_message_free(&m);

    cJSON *root = cJSON_ParseWithLength(json, json_len);
    const cJSON *body = cJSON_GetObjectItemCaseSensitive(root, "AVCDialInRequest");
    const cJSON *marshaled_endpoint = cJSON_GetObjectItemCaseSensitive(body, "avcEndpoint");
    const cJSON *signal_leg = cJSON_GetObjectItemCaseSensitive(marshaled_endpoint, "avcEndpoint");
    const cJSON *user_agent = cJSON_GetObjectItemCaseSensitive(signal_leg, "userAgent");
    const cJSON *audio = cJSON_GetObjectItemCaseSensitive(marshaled_endpoint, "audioProperty");
    const cJSON *media_id = cJSON_GetObjectItemCaseSensitive(audio, "mediaId");
    const cJSON *sample_rate = cJSON_GetObjectItemCaseSensitive(audio, "samplerate");
    const cJSON *channels = cJSON_GetObjectItemCaseSensitive(audio, "channels");
    const cJSON *bits = cJSON_GetObjectItemCaseSensitive(audio, "bits");
    if (!cJSON_IsString(user_agent) ||
        strcmp(cJSON_GetStringValue(user_agent), "Polycom RealPresence Group 500") != 0 ||
        !cJSON_IsString(media_id) || strcmp(cJSON_GetStringValue(media_id), "audio-1") != 0 ||
        !cJSON_IsNumber(sample_rate) || (int)cJSON_GetNumberValue(sample_rate) != 16000 ||
        !cJSON_IsNumber(channels) || (int)cJSON_GetNumberValue(channels) != 2 || !cJSON_IsNumber(bits) ||
        (int)cJSON_GetNumberValue(bits) != 24) {
        cJSON_Delete(root);
        free(json);
        return fail("marshaled AVC user agent and audio property paths");
    }
    cJSON_Delete(root);

    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(json, json_len, &m) != AGORAHEX_OK) {
        free(json);
        return fail("parse marshaled AVC user agent and audio property");
    }
    free(json);
    endpoint = &m.u.avc_dial_in_request.avc_endpoint;
    if (!endpoint->avc_endpoint.user_agent ||
        strcmp(endpoint->avc_endpoint.user_agent, "Polycom RealPresence Group 500") != 0 ||
        !endpoint->audio_property.media_id || strcmp(endpoint->audio_property.media_id, "audio-1") != 0 ||
        endpoint->audio_property.sample_rate != 16000 || endpoint->audio_property.channels != 2 ||
        endpoint->audio_property.bits != 24) {
        agorahex_message_free(&m);
        return fail("roundtrip AVC user agent and audio property");
    }
    agorahex_message_free(&m);
    return 0;
}

static int test_avc_missing_audio_and_content_properties_default(void) {
    const char *raw =
        "{\"AVCDialInRequest\":{\"avcEndpoint\":{\"avcEndpoint\":{}}}}";
    agorahex_message_t m;
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(raw, strlen(raw), &m) != AGORAHEX_OK) {
        return fail("parse AVC endpoint without audio and content properties");
    }

    const agorahex_avc_dial_endpoint_t *endpoint = &m.u.avc_dial_in_request.avc_endpoint;
    const agorahex_audio_capabilities_t *audio = &endpoint->audio_property;
    const agorahex_media_capabilities_t *content = &endpoint->content_property;
    if (!audio->media_id || strcmp(audio->media_id, "") != 0 || audio->sample_rate != 48000 ||
        audio->channels != 1 || audio->bits != 16 || !content->media_id ||
        strcmp(content->media_id, "") != 0 || content->bitrate != 0 || content->width != 0 ||
        content->height != 0 || content->fps != 0) {
        agorahex_message_free(&m);
        return fail("default AVC audio and content properties");
    }

    char *json = NULL;
    size_t json_len = 0;
    if (agorahex_marshal_envelope(&m, &json, &json_len) != AGORAHEX_OK) {
        agorahex_message_free(&m);
        return fail("marshal default AVC audio and content properties");
    }
    agorahex_message_free(&m);

    cJSON *root = cJSON_ParseWithLength(json, json_len);
    const cJSON *body = cJSON_GetObjectItemCaseSensitive(root, "AVCDialInRequest");
    const cJSON *marshaled_endpoint = cJSON_GetObjectItemCaseSensitive(body, "avcEndpoint");
    const cJSON *marshaled_audio = cJSON_GetObjectItemCaseSensitive(marshaled_endpoint, "audioProperty");
    const cJSON *marshaled_content = cJSON_GetObjectItemCaseSensitive(marshaled_endpoint, "contentProperty");
    const cJSON *audio_media_id = cJSON_GetObjectItemCaseSensitive(marshaled_audio, "mediaId");
    const cJSON *audio_sample_rate = cJSON_GetObjectItemCaseSensitive(marshaled_audio, "samplerate");
    const cJSON *audio_channels = cJSON_GetObjectItemCaseSensitive(marshaled_audio, "channels");
    const cJSON *audio_bits = cJSON_GetObjectItemCaseSensitive(marshaled_audio, "bits");
    const cJSON *content_media_id = cJSON_GetObjectItemCaseSensitive(marshaled_content, "mediaId");
    const cJSON *content_bitrate = cJSON_GetObjectItemCaseSensitive(marshaled_content, "bitrate");
    const cJSON *content_width = cJSON_GetObjectItemCaseSensitive(marshaled_content, "width");
    const cJSON *content_height = cJSON_GetObjectItemCaseSensitive(marshaled_content, "height");
    const cJSON *content_fps = cJSON_GetObjectItemCaseSensitive(marshaled_content, "fps");
    if (!cJSON_IsObject(marshaled_audio) || cJSON_GetArraySize(marshaled_audio) != 4 ||
        !cJSON_IsString(audio_media_id) || strcmp(cJSON_GetStringValue(audio_media_id), "") != 0 ||
        !cJSON_IsNumber(audio_sample_rate) || (int)cJSON_GetNumberValue(audio_sample_rate) != 48000 ||
        !cJSON_IsNumber(audio_channels) || (int)cJSON_GetNumberValue(audio_channels) != 1 ||
        !cJSON_IsNumber(audio_bits) || (int)cJSON_GetNumberValue(audio_bits) != 16 ||
        !cJSON_IsObject(marshaled_content) || cJSON_GetArraySize(marshaled_content) != 5 ||
        !cJSON_IsString(content_media_id) || strcmp(cJSON_GetStringValue(content_media_id), "") != 0 ||
        !cJSON_IsNumber(content_bitrate) || (int)cJSON_GetNumberValue(content_bitrate) != 0 ||
        !cJSON_IsNumber(content_width) || (int)cJSON_GetNumberValue(content_width) != 0 ||
        !cJSON_IsNumber(content_height) || (int)cJSON_GetNumberValue(content_height) != 0 ||
        !cJSON_IsNumber(content_fps) || (int)cJSON_GetNumberValue(content_fps) != 0) {
        cJSON_Delete(root);
        free(json);
        return fail("marshal default AVC audio and content properties");
    }
    cJSON_Delete(root);
    free(json);
    return 0;
}

static int test_avc_partial_audio_and_content_properties_default(void) {
    const char *raw =
        "{\"AVCDialInRequest\":{\"avcEndpoint\":{\"audioProperty\":{\"channels\":2},"
        "\"contentProperty\":{\"bitrate\":2048}}}}";
    agorahex_message_t m;
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(raw, strlen(raw), &m) != AGORAHEX_OK) {
        return fail("parse partial AVC audio and content properties");
    }

    const agorahex_avc_dial_endpoint_t *endpoint = &m.u.avc_dial_in_request.avc_endpoint;
    const agorahex_audio_capabilities_t *audio = &endpoint->audio_property;
    const agorahex_media_capabilities_t *content = &endpoint->content_property;
    if (!audio->media_id || strcmp(audio->media_id, "") != 0 || audio->sample_rate != 48000 ||
        audio->channels != 2 || audio->bits != 16 || !content->media_id ||
        strcmp(content->media_id, "") != 0 || content->bitrate != 2048 || content->width != 0 ||
        content->height != 0 || content->fps != 0) {
        agorahex_message_free(&m);
        return fail("partial AVC audio and content property defaults");
    }
    agorahex_message_free(&m);
    return 0;
}

static int test_avc_audio_out_of_range_values_ignored(void) {
    const char *raw =
        "{\"AVCDialInRequest\":{\"avcEndpoint\":{\"audioProperty\":{\"samplerate\":1e100,"
        "\"channels\":-1e100,\"bits\":1e100}}}}";
    agorahex_message_t m;
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(raw, strlen(raw), &m) != AGORAHEX_OK) {
        return fail("parse out-of-range AVC audio values");
    }
    const agorahex_audio_capabilities_t *audio = &m.u.avc_dial_in_request.avc_endpoint.audio_property;
    if (audio->sample_rate != 48000 || audio->channels != 1 || audio->bits != 16) {
        agorahex_message_free(&m);
        return fail("default out-of-range AVC audio values");
    }
    agorahex_message_free(&m);

    raw =
        "{\"AVCDialInRequest\":{\"avcEndpoint\":{\"audioProperty\":{\"samplerate\":0,"
        "\"channels\":0,\"bits\":0}}}}";
    memset(&m, 0, sizeof m);
    if (agorahex_parse_envelope(raw, strlen(raw), &m) != AGORAHEX_OK) {
        return fail("parse zero AVC audio values");
    }
    audio = &m.u.avc_dial_in_request.avc_endpoint.audio_property;
    if (audio->sample_rate != 0 || audio->channels != 0 || audio->bits != 0) {
        agorahex_message_free(&m);
        return fail("accept zero AVC audio values");
    }
    agorahex_message_free(&m);
    return 0;
}

static int test_avc_new_json_fields_report_allocation_failure(void) {
    static const size_t fail_sizes[] = {
        sizeof("userAgent"),
        sizeof("mediaId"),
        sizeof("samplerate"),
        sizeof("channels"),
        sizeof("bits"),
        sizeof("audioProperty"),
    };
    agorahex_message_t m;
    memset(&m, 0, sizeof m);
    m.kind = AGORAHEX_KIND_AVC_DIAL_IN_REQUEST;
    m.u.avc_dial_in_request.avc_endpoint.avc_endpoint.user_agent = agorahex_strdup("agent");
    m.u.avc_dial_in_request.avc_endpoint.audio_property.media_id = agorahex_strdup("audio");
    if (!m.u.avc_dial_in_request.avc_endpoint.avc_endpoint.user_agent ||
        !m.u.avc_dial_in_request.avc_endpoint.audio_property.media_id) {
        agorahex_message_free(&m);
        return fail("allocate AVC JSON allocation failure fixture");
    }

    cJSON_Hooks hooks = {
        .malloc_fn = fail_cjson_allocation,
        .free_fn = free_cjson_allocation,
    };
    for (size_t i = 0; i < sizeof(fail_sizes) / sizeof(fail_sizes[0]); ++i) {
        char *json = NULL;
        size_t json_len = 0;
        cjson_fail_allocation_size = fail_sizes[i];
        cJSON_InitHooks(&hooks);
        const agorahex_result_t result = agorahex_marshal_envelope(&m, &json, &json_len);
        cJSON_InitHooks(NULL);
        free(json);
        if (result != AGORAHEX_ERR_NO_MEMORY || json_len != 0) {
            agorahex_message_free(&m);
            return fail("report AVC JSON allocation failure");
        }
    }
    cjson_fail_allocation_size = 0;
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
        test_avc_user_agent_audio_roundtrip() != 0 ||
        test_avc_missing_audio_and_content_properties_default() != 0 ||
        test_avc_partial_audio_and_content_properties_default() != 0 ||
        test_avc_audio_out_of_range_values_ignored() != 0 ||
        test_avc_new_json_fields_report_allocation_failure() != 0 || test_dtmf_indication_parse() != 0 ||
        test_avc_dial_in_reply_max_resolution_roundtrip() != 0) {
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
