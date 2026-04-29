/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#ifndef AGORAHEX_FRAMING_H
#define AGORAHEX_FRAMING_H

#include "result.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AGORAHEX_FRAME_HEADER_SIZE 8u
#define AGORAHEX_DEFAULT_MAX_FRAME_BYTES (4u << 20)

extern const uint8_t agorahex_frame_magic[4];

/**
 * Returns total byte size of a frame for the given JSON body length.
 */
size_t agorahex_frame_encoded_size(size_t json_len);

/**
 * Encodes UTF-8 JSON bytes into out (magic + BE total length + json).
 * out must have capacity >= agorahex_frame_encoded_size(json_len).
 */
void agorahex_frame_encode(const uint8_t *json, size_t json_len, uint8_t *out);

typedef agorahex_result_t (*agorahex_on_frame_fn)(void *ctx, const uint8_t *json, size_t json_len);

typedef struct agorahex_frame_decoder {
    uint8_t *buf;
    size_t len;
    size_t cap;
    uint32_t max_frame_bytes;
    agorahex_result_t last_error;
} agorahex_frame_decoder_t;

void agorahex_frame_decoder_init(agorahex_frame_decoder_t *d, uint32_t max_frame_bytes);
void agorahex_frame_decoder_reset(agorahex_frame_decoder_t *d);
void agorahex_frame_decoder_fini(agorahex_frame_decoder_t *d);

/**
 * Appends stream bytes. Invokes on_frame for each complete JSON body (without the 8-byte header).
 * Returns AGORAHEX_OK even when no full frame is available yet.
 */
agorahex_result_t agorahex_frame_decoder_append(agorahex_frame_decoder_t *d, const uint8_t *p,
                                                 size_t n, void *ctx, agorahex_on_frame_fn on_frame);

#ifdef __cplusplus
}
#endif

#endif /* AGORAHEX_FRAMING_H */
