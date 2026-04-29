/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#include "agorahex/framing.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int count;
    uint8_t *bufs[8];
    size_t lens[8];
} collect_t;

static agorahex_result_t on_frame(void *ctx, const uint8_t *json, size_t json_len) {
    collect_t *c = (collect_t *)ctx;
    if (c->count >= 8) {
        return AGORAHEX_OK;
    }
    uint8_t *copy = (uint8_t *)malloc(json_len);
    if (!copy) {
        return AGORAHEX_ERR_NO_MEMORY;
    }
    memcpy(copy, json, json_len);
    c->bufs[c->count] = copy;
    c->lens[c->count++] = json_len;
    return AGORAHEX_OK;
}

static void free_collect(collect_t *c) {
    for (int i = 0; i < c->count; i++) {
        free(c->bufs[i]);
        c->bufs[i] = NULL;
    }
    c->count = 0;
}

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

int main(void) {
    const uint8_t json[] = "{\"HangupIndication\":{\"callId\":\"x\",\"dropCode\":0}}";
    size_t json_len = sizeof json - 1u;
    size_t flen = agorahex_frame_encoded_size(json_len);
    uint8_t *frame = (uint8_t *)malloc(flen);
    agorahex_frame_encode(json, json_len, frame);

    collect_t col;
    memset(&col, 0, sizeof col);
    agorahex_frame_decoder_t dec;
    agorahex_frame_decoder_init(&dec, AGORAHEX_DEFAULT_MAX_FRAME_BYTES);
    if (agorahex_frame_decoder_append(&dec, frame, flen, &col, on_frame) != AGORAHEX_OK) {
        return fail("append1");
    }
    if (col.count != 1 || col.lens[0] != json_len || memcmp(col.bufs[0], json, json_len) != 0) {
        return fail("roundtrip");
    }
    free_collect(&col);
    agorahex_frame_decoder_reset(&dec);

    const uint8_t a[] = "{\"a\":1}";
    const uint8_t b[] = "{\"b\":2}";
    size_t al = sizeof a - 1u, bl = sizeof b - 1u;
    size_t fa = agorahex_frame_encoded_size(al);
    size_t fb = agorahex_frame_encoded_size(bl);
    uint8_t *buf2 = (uint8_t *)malloc(fa + fb);
    agorahex_frame_encode(a, al, buf2);
    agorahex_frame_encode(b, bl, buf2 + fa);
    if (agorahex_frame_decoder_append(&dec, buf2, fa + fb, &col, on_frame) != AGORAHEX_OK) {
        return fail("append2");
    }
    if (col.count != 2 || memcmp(col.bufs[0], a, al) != 0 || memcmp(col.bufs[1], b, bl) != 0) {
        return fail("two frames");
    }
    free_collect(&col);
    agorahex_frame_decoder_reset(&dec);

    const uint8_t hang[] = "{\"HangupIndication\":{\"callId\":\"id\",\"dropCode\":1}}";
    size_t hl = sizeof hang - 1u;
    size_t fh = agorahex_frame_encoded_size(hl);
    uint8_t *frh = (uint8_t *)malloc(fh);
    agorahex_frame_encode(hang, hl, frh);
    size_t mid = fh / 2;
    if (agorahex_frame_decoder_append(&dec, frh, mid, &col, on_frame) != AGORAHEX_OK || col.count != 0) {
        return fail("half1");
    }
    if (agorahex_frame_decoder_append(&dec, frh + mid, fh - mid, &col, on_frame) != AGORAHEX_OK) {
        return fail("half2");
    }
    if (col.count != 1 || col.lens[0] != hl || memcmp(col.bufs[0], hang, hl) != 0) {
        return fail("half reasm");
    }
    free_collect(&col);
    agorahex_frame_decoder_reset(&dec);

    uint8_t bad[] = {0, 0, 0, 0, 0, 0, 0, 8};
    if (agorahex_frame_decoder_append(&dec, bad, sizeof bad, &col, on_frame) != AGORAHEX_ERR_BAD_MAGIC) {
        return fail("bad magic");
    }
    if (agorahex_frame_decoder_append(&dec, (const uint8_t *)"\1\2\3", 3, &col, on_frame) != AGORAHEX_ERR_BAD_MAGIC) {
        return fail("sticky");
    }
    agorahex_frame_decoder_reset(&dec);

    uint8_t shortf[8];
    memcpy(shortf, agorahex_frame_magic, 4);
    shortf[4] = 0;
    shortf[5] = 0;
    shortf[6] = 0;
    shortf[7] = 4;
    if (agorahex_frame_decoder_append(&dec, shortf, sizeof shortf, &col, on_frame) != AGORAHEX_ERR_FRAME_TOO_SHORT) {
        return fail("too short");
    }
    agorahex_frame_decoder_reset(&dec);

    agorahex_frame_decoder_init(&dec, 100u);
    uint8_t big[8];
    memcpy(big, agorahex_frame_magic, 4);
    big[4] = 0;
    big[5] = 0;
    big[6] = 0;
    big[7] = 200;
    if (agorahex_frame_decoder_append(&dec, big, sizeof big, &col, on_frame) != AGORAHEX_ERR_FRAME_TOO_LARGE) {
        return fail("too large");
    }

    agorahex_frame_decoder_fini(&dec);
    free(frame);
    free(buf2);
    free(frh);
    return 0;
}
