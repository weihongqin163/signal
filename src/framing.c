/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#include "agorahex/framing.h"

#include <stdlib.h>
#include <string.h>

const uint8_t agorahex_frame_magic[4] = {0xaa, 0xaa, 0x55, 0x55};

static uint32_t read_be_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void write_be_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xffu);
    p[1] = (uint8_t)((v >> 16) & 0xffu);
    p[2] = (uint8_t)((v >> 8) & 0xffu);
    p[3] = (uint8_t)(v & 0xffu);
}

size_t agorahex_frame_encoded_size(size_t json_len) {
    return (size_t)AGORAHEX_FRAME_HEADER_SIZE + json_len;
}

void agorahex_frame_encode(const uint8_t *json, size_t json_len, uint8_t *out) {
    memcpy(out, agorahex_frame_magic, 4);
    uint32_t total = (uint32_t)(AGORAHEX_FRAME_HEADER_SIZE + json_len);
    write_be_u32(out + 4, total);
    if (json_len > 0 && json != NULL) {
        memcpy(out + AGORAHEX_FRAME_HEADER_SIZE, json, json_len);
    }
}

void agorahex_frame_decoder_init(agorahex_frame_decoder_t *d, uint32_t max_frame_bytes) {
    d->buf = NULL;
    d->len = 0;
    d->cap = 0;
    d->max_frame_bytes = max_frame_bytes == 0 ? AGORAHEX_DEFAULT_MAX_FRAME_BYTES : max_frame_bytes;
    d->last_error = AGORAHEX_OK;
}

void agorahex_frame_decoder_reset(agorahex_frame_decoder_t *d) {
    d->len = 0;
    d->last_error = AGORAHEX_OK;
}

void agorahex_frame_decoder_fini(agorahex_frame_decoder_t *d) {
    if (d && d->buf) {
        free(d->buf);
        d->buf = NULL;
        d->len = 0;
        d->cap = 0;
    }
}

static int ensure_cap(agorahex_frame_decoder_t *d, size_t need) {
    if (need <= d->cap) {
        return 0;
    }
    size_t ncap = d->cap ? d->cap : 256;
    while (ncap < need) {
        if (ncap > (SIZE_MAX >> 1)) {
            return -1;
        }
        ncap *= 2;
    }
    uint8_t *nb = (uint8_t *)realloc(d->buf, ncap);
    if (!nb) {
        return -1;
    }
    d->buf = nb;
    d->cap = ncap;
    return 0;
}

agorahex_result_t agorahex_frame_decoder_append(agorahex_frame_decoder_t *d, const uint8_t *p, size_t n,
                                                void *ctx, agorahex_on_frame_fn on_frame) {
    if (d->last_error != AGORAHEX_OK) {
        return d->last_error;
    }
    if (n == 0) {
        return AGORAHEX_OK;
    }
    if (ensure_cap(d, d->len + n) != 0) {
        d->last_error = AGORAHEX_ERR_NO_MEMORY;
        free(d->buf);
        d->buf = NULL;
        d->len = d->cap = 0;
        return AGORAHEX_ERR_NO_MEMORY;
    }
    memcpy(d->buf + d->len, p, n);
    d->len += n;

    for (;;) {
        if (d->len < AGORAHEX_FRAME_HEADER_SIZE) {
            return AGORAHEX_OK;
        }
        if (d->buf[0] != agorahex_frame_magic[0] || d->buf[1] != agorahex_frame_magic[1] ||
            d->buf[2] != agorahex_frame_magic[2] || d->buf[3] != agorahex_frame_magic[3]) {
            d->last_error = AGORAHEX_ERR_BAD_MAGIC;
            free(d->buf);
            d->buf = NULL;
            d->len = d->cap = 0;
            return AGORAHEX_ERR_BAD_MAGIC;
        }
        uint32_t total = read_be_u32(d->buf + 4);
        if (total < AGORAHEX_FRAME_HEADER_SIZE) {
            d->last_error = AGORAHEX_ERR_FRAME_TOO_SHORT;
            free(d->buf);
            d->buf = NULL;
            d->len = d->cap = 0;
            return AGORAHEX_ERR_FRAME_TOO_SHORT;
        }
        if (total > d->max_frame_bytes) {
            d->last_error = AGORAHEX_ERR_FRAME_TOO_LARGE;
            free(d->buf);
            d->buf = NULL;
            d->len = d->cap = 0;
            return AGORAHEX_ERR_FRAME_TOO_LARGE;
        }
        if (d->len < (size_t)total) {
            return AGORAHEX_OK;
        }
        size_t body_len = (size_t)total - AGORAHEX_FRAME_HEADER_SIZE;
        const uint8_t *body = d->buf + AGORAHEX_FRAME_HEADER_SIZE;
        if (on_frame) {
            agorahex_result_t cb = on_frame(ctx, body, body_len);
            if (cb != AGORAHEX_OK) {
                d->last_error = cb;
                free(d->buf);
                d->buf = NULL;
                d->len = d->cap = 0;
                return cb;
            }
        }
        size_t rest = d->len - (size_t)total;
        memmove(d->buf, d->buf + total, rest);
        d->len = rest;
    }
}
