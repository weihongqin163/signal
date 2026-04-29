/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#include "agorahex/result.h"

const char *agorahex_strerror(agorahex_result_t code) {
    switch (code) {
    case AGORAHEX_OK:
        return "ok";
    case AGORAHEX_ERR_BAD_MAGIC:
        return "invalid frame magic";
    case AGORAHEX_ERR_FRAME_TOO_SHORT:
        return "frame length below header size";
    case AGORAHEX_ERR_FRAME_TOO_LARGE:
        return "frame length exceeds limit";
    case AGORAHEX_ERR_INVALID_ARG:
        return "invalid argument";
    case AGORAHEX_ERR_NO_MEMORY:
        return "out of memory";
    case AGORAHEX_ERR_ENVELOPE_NOT_OBJECT:
        return "envelope must be a JSON object";
    case AGORAHEX_ERR_ENVELOPE_KEY_COUNT:
        return "envelope must contain exactly one top-level key";
    case AGORAHEX_ERR_UNKNOWN_KIND:
        return "unknown message kind";
    case AGORAHEX_ERR_JSON_PARSE:
        return "json parse error";
    case AGORAHEX_ERR_NOSPACE:
        return "output buffer too small";
    case AGORAHEX_ERR_ALREADY_STARTED:
        return "already started";
    case AGORAHEX_ERR_NOT_STARTED:
        return "not started";
    case AGORAHEX_ERR_IO:
        return "i/o error";
    case AGORAHEX_ERR_NOT_CONNECTED:
        return "not connected";
    case AGORAHEX_ERR_CLIENT_LIMIT:
        return "client limit reached";
    case AGORAHEX_ERR_NOT_FOUND:
        return "not found";
    default:
        return "unknown error";
    }
}
