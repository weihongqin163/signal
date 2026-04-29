/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#ifndef AGORAHEX_TYPES_H
#define AGORAHEX_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agorahex_display_info {
    char *name;
    char *url;
    char *conference_id;
} agorahex_display_info_t;

typedef struct agorahex_audio_to_mcu {
    char *media_id;
    bool muted;
} agorahex_audio_to_mcu_t;

typedef struct agorahex_audio_from_mcu {
    char *media_id;
} agorahex_audio_from_mcu_t;

typedef struct agorahex_video_track {
    char *media_id;
    int bitrate;
    int width;
    int height;
    int fps;
} agorahex_video_track_t;

typedef struct agorahex_media_endpoint {
    agorahex_display_info_t agora_endpoint;
    int call_bitrate;
    bool is_audio_only;
    agorahex_audio_to_mcu_t to_mcu_audio;
    agorahex_audio_from_mcu_t from_mcu_audio;
    agorahex_video_track_t to_gw_video;
    agorahex_video_track_t from_gw_video;
    agorahex_video_track_t to_gw_content;
    agorahex_video_track_t from_gw_content;
} agorahex_media_endpoint_t;

typedef struct agorahex_avc_signal_leg {
    char *signal_type;
    bool encryption;
    char *ipv4;
    char *ipv6;
    char *e164;
    char *conference_id;
    char *url;
} agorahex_avc_signal_leg_t;

typedef struct agorahex_media_capabilities {
    char *media_id;
    int bitrate;
    int width;
    int height;
    int fps;
} agorahex_media_capabilities_t;

typedef struct agorahex_avc_dial_endpoint {
    agorahex_avc_signal_leg_t avc_endpoint;
    bool is_audio_only;
    agorahex_media_capabilities_t people_property;
    agorahex_media_capabilities_t content_property;
} agorahex_avc_dial_endpoint_t;

typedef struct agorahex_agora_dial_in_indication {
    char *call_id;
    agorahex_media_endpoint_t agora_endpoint;
    agorahex_avc_signal_leg_t avc_leg;
} agorahex_agora_dial_in_indication_t;

typedef struct agorahex_avc_dial_in_request {
    char *call_id;
    agorahex_avc_dial_endpoint_t avc_endpoint;
    char *conference_id;
    char *password;
} agorahex_avc_dial_in_request_t;

typedef struct agorahex_avc_dial_in_reply {
    char *call_id;
    int return_code;
    agorahex_media_endpoint_t agora_endpoint;
} agorahex_avc_dial_in_reply_t;

typedef struct agorahex_hangup_indication {
    char *call_id;
    int drop_code;
} agorahex_hangup_indication_t;

typedef struct agorahex_muted_indication {
    char *call_id;
    bool muted;
} agorahex_muted_indication_t;

typedef struct agorahex_avc_name_changed_indication {
    char *call_id;
    char *name;
} agorahex_avc_name_changed_indication_t;

typedef struct agorahex_agora_start_content_indication {
    char *call_id;
} agorahex_agora_start_content_indication_t;

typedef struct agorahex_avc_start_content_request {
    char *call_id;
} agorahex_avc_start_content_request_t;

typedef struct agorahex_avc_start_content_replay {
    char *call_id;
    bool accept;
} agorahex_avc_start_content_replay_t;

typedef struct agorahex_stop_content_indication {
    char *call_id;
} agorahex_stop_content_indication_t;

#ifdef __cplusplus
}
#endif

#endif /* AGORAHEX_TYPES_H */
