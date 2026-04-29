/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#ifndef AGORAHEX_ENVELOPE_H
#define AGORAHEX_ENVELOPE_H

#include "result.h"
#include "types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum agorahex_kind {
    AGORAHEX_KIND_AGORA_DIAL_IN_INDICATION = 0,
    AGORAHEX_KIND_AVC_DIAL_IN_REQUEST,
    AGORAHEX_KIND_AVC_DIAL_IN_REPLY,
    AGORAHEX_KIND_HANGUP_INDICATION,
    AGORAHEX_KIND_MUTED_INDICATION,
    AGORAHEX_KIND_AVC_NAME_CHANGED_INDICATION,
    AGORAHEX_KIND_AGORA_START_CONTENT_INDICATION,
    AGORAHEX_KIND_AVC_START_CONTENT_REQUEST,
    AGORAHEX_KIND_AVC_START_CONTENT_REPLAY,
    AGORAHEX_KIND_STOP_CONTENT_INDICATION,
} agorahex_kind_t;

typedef struct agorahex_message {
    agorahex_kind_t kind;
    union {
        agorahex_agora_dial_in_indication_t agora_dial_in_indication;
        agorahex_avc_dial_in_request_t avc_dial_in_request;
        agorahex_avc_dial_in_reply_t avc_dial_in_reply;
        agorahex_hangup_indication_t hangup_indication;
        agorahex_muted_indication_t muted_indication;
        agorahex_avc_name_changed_indication_t avc_name_changed_indication;
        agorahex_agora_start_content_indication_t agora_start_content_indication;
        agorahex_avc_start_content_request_t avc_start_content_request;
        agorahex_avc_start_content_replay_t avc_start_content_replay;
        agorahex_stop_content_indication_t stop_content_indication;
    } u;
} agorahex_message_t;

const char *agorahex_kind_cstr(agorahex_kind_t k);

/**
 * Parses a single-message JSON object (UTF-8). buf need not be NUL-terminated; a copy is made internally.
 */
agorahex_result_t agorahex_parse_envelope(const char *buf, size_t buf_len, agorahex_message_t *out);

void agorahex_message_free(agorahex_message_t *m);

/**
 * Marshals one known kind into a heap-allocated JSON envelope. Caller must free(*out_json).
 */
agorahex_result_t agorahex_marshal_envelope(const agorahex_message_t *msg, char **out_json, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* AGORAHEX_ENVELOPE_H */
