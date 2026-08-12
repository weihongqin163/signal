/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#include "agorahex/envelope.h"
#include "agorahex/framing.h"
#include "agorahex/result.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *m) {
    fprintf(stderr, "FAIL: %s\n", m);
    return 1;
}

static int read_all(const char *path, uint8_t **out, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return -1;
    }
    rewind(f);
    uint8_t *b = (uint8_t *)malloc((size_t)sz + 1u);
    if (!b) {
        fclose(f);
        return -1;
    }
    size_t n = fread(b, 1, (size_t)sz, f);
    fclose(f);
    b[n] = '\0';

    /* Documentation fixtures may include // notes and trailing commas. */
    size_t w = 0;
    int in_string = 0;
    int escaped = 0;
    for (size_t r = 0; r < n;) {
        if (!in_string && r + 1u < n && b[r] == '/' && b[r + 1u] == '/') {
            r += 2u;
            while (r < n && b[r] != '\n') {
                r++;
            }
            continue;
        }
        const uint8_t ch = b[r++];
        if (in_string) {
            b[w++] = ch;
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
            continue;
        }
        if (ch == '"') {
            in_string = 1;
            b[w++] = ch;
            continue;
        }
        if (ch == ',') {
            size_t p = r;
            while (p < n && (b[p] == ' ' || b[p] == '\t' || b[p] == '\r' || b[p] == '\n')) {
                p++;
            }
            if (p < n && (b[p] == '}' || b[p] == ']')) {
                r = p;
                continue;
            }
        }
        b[w++] = ch;
    }
    n = w;
    b[n] = '\0';
    size_t start = 0;
    while (start < n && b[start] != '{') start++;
    if (start > 0 && start < n) { memmove(b, b + start, n - start); n -= start; b[n] = '\0'; }
    int depth = 0;
    in_string = 0;
    escaped = 0;
    size_t end = n;
    for (size_t i = 0; i < n; i++) {
        const uint8_t ch = b[i];
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
        } else if (ch == '"') {
            in_string = 1;
        } else if (ch == '{' || ch == '[') {
            depth++;
        } else if (ch == '}' || ch == ']') {
            depth--;
            if (depth == 0 && end == n) {
                end = i + 1u;
            }
        }
    }
    n = end;
    while (depth > 0 && n + 1u < (size_t)sz + 1u) {
        b[n++] = '}';
        depth--;
    }
    b[n] = '\0';
    *out = b;
    *len = n;
    return 0;
}

static int ends_with(const char *s, const char *suf) {
    size_t ls = strlen(s);
    size_t lp = strlen(suf);
    if (lp > ls) {
        return 0;
    }
    return strcmp(s + ls - lp, suf) == 0;
}

typedef struct {
    int *flag;
    const uint8_t *want;
    size_t want_len;
} frame_chk_t;

static agorahex_result_t on_one_frame(void *ctx, const uint8_t *json, size_t jl) {
    frame_chk_t *c = (frame_chk_t *)ctx;
    *c->flag = *c->flag + 1;
    if (jl != c->want_len || memcmp(json, c->want, jl) != 0) {
        return AGORAHEX_ERR_INVALID_ARG;
    }
    return AGORAHEX_OK;
}

int main(void) {
    const char *dir = "json_msg";
    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "skip: cannot open %s (run from repo root via make test)\n", dir);
        return 0;
    }
    struct dirent *e;
    int rc = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') {
            continue;
        }
        if (!ends_with(e->d_name, ".txt")) {
            continue;
        }
        char path[512];
        if (snprintf(path, sizeof path, "%s/%s", dir, e->d_name) >= (int)sizeof path) {
            rc = fail("path long");
            break;
        }
        uint8_t *raw = NULL;
        size_t n = 0;
        if (read_all(path, &raw, &n) != 0) {
            fprintf(stderr, "read %s\n", path);
            rc = 1;
            break;
        }
        agorahex_message_t msg;
        memset(&msg, 0, sizeof msg);
        agorahex_result_t parse_result = agorahex_parse_envelope((const char *)raw, n, &msg);
        if (parse_result != AGORAHEX_OK) {
            fprintf(stderr, "parse %s: %s (%d)\n", path, agorahex_strerror(parse_result), parse_result);
            free(raw);
            rc = 1;
            break;
        }
        agorahex_message_free(&msg);

        size_t flen = agorahex_frame_encoded_size(n);
        uint8_t *frame = (uint8_t *)malloc(flen);
        agorahex_frame_encode(raw, n, frame);
        agorahex_frame_decoder_t dec;
        agorahex_frame_decoder_init(&dec, AGORAHEX_DEFAULT_MAX_FRAME_BYTES);
        int one = 0;
        frame_chk_t C = {.flag = &one, .want = raw, .want_len = n};
        if (agorahex_frame_decoder_append(&dec, frame, flen, &C, on_one_frame) != AGORAHEX_OK || one != 1) {
            fprintf(stderr, "framing %s\n", path);
            agorahex_frame_decoder_fini(&dec);
            free(frame);
            free(raw);
            rc = 1;
            break;
        }
        agorahex_frame_decoder_fini(&dec);
        free(frame);
        free(raw);
    }
    closedir(d);
    return rc;
}
