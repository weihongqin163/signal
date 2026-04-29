/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#ifndef AGORAHEX_RESULT_H
#define AGORAHEX_RESULT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum agorahex_result {
    AGORAHEX_OK = 0,
    AGORAHEX_ERR_BAD_MAGIC = -1,
    AGORAHEX_ERR_FRAME_TOO_SHORT = -2,
    AGORAHEX_ERR_FRAME_TOO_LARGE = -3,
    AGORAHEX_ERR_INVALID_ARG = -5,
    AGORAHEX_ERR_NO_MEMORY = -6,
    AGORAHEX_ERR_ENVELOPE_NOT_OBJECT = -7,
    AGORAHEX_ERR_ENVELOPE_KEY_COUNT = -8,
    AGORAHEX_ERR_UNKNOWN_KIND = -9,
    AGORAHEX_ERR_JSON_PARSE = -10,
    AGORAHEX_ERR_NOSPACE = -11,
    AGORAHEX_ERR_ALREADY_STARTED = -12,
    AGORAHEX_ERR_NOT_STARTED = -13,
    AGORAHEX_ERR_IO = -14,
    AGORAHEX_ERR_NOT_CONNECTED = -15,
    AGORAHEX_ERR_CLIENT_LIMIT = -16,
    AGORAHEX_ERR_NOT_FOUND = -17,
} agorahex_result_t;

const char *agorahex_strerror(agorahex_result_t code);

#ifdef __cplusplus
}
#endif

#endif /* AGORAHEX_RESULT_H */
