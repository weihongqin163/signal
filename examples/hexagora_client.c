/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#define _POSIX_C_SOURCE 200809L

#include "agorahex/envelope.h"
#include "agorahex/result.h"
#include "agorahex/signal_tcp.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t g_stop;

static void on_sigint(int signo) {
    (void)signo;
    g_stop = 1;
}

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000ll + (long long)(ts.tv_nsec / 1000000ll);
}

static const char *extract_call_id(const agorahex_message_t *m) {
    switch (m->kind) {
    case AGORAHEX_KIND_AGORA_DIAL_IN_INDICATION:
        return m->u.agora_dial_in_indication.call_id;
    case AGORAHEX_KIND_AVC_DIAL_IN_REQUEST:
        return m->u.avc_dial_in_request.call_id;
    case AGORAHEX_KIND_AVC_DIAL_IN_REPLY:
        return m->u.avc_dial_in_reply.call_id;
    case AGORAHEX_KIND_HANGUP_INDICATION:
        return m->u.hangup_indication.call_id;
    case AGORAHEX_KIND_MUTED_INDICATION:
        return m->u.muted_indication.call_id;
    case AGORAHEX_KIND_AVC_NAME_CHANGED_INDICATION:
        return m->u.avc_name_changed_indication.call_id;
    case AGORAHEX_KIND_AGORA_START_CONTENT_INDICATION:
        return m->u.agora_start_content_indication.call_id;
    case AGORAHEX_KIND_AVC_START_CONTENT_REQUEST:
        return m->u.avc_start_content_request.call_id;
    case AGORAHEX_KIND_AVC_START_CONTENT_REPLAY:
        return m->u.avc_start_content_replay.call_id;
    case AGORAHEX_KIND_STOP_CONTENT_INDICATION:
        return m->u.stop_content_indication.call_id;
    default:
        return "";
    }
}

static void client_cb(int fd, const void *json, int len, agorahex_message_t *msg_t) {
    const char *cid;

    if (fd < 0 || !json || len < 0 || !msg_t) {
        return;
    }
    cid = extract_call_id(msg_t);
    fprintf(stdout, "received fd=%d kind=%s callId=%s json=%.*s\n", fd, agorahex_kind_cstr(msg_t->kind),
            cid ? cid : "", len, (const char *)json);
    fflush(stdout);
}

static int read_file(const char *path, char **out, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return -1;
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        return -1;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    *out = buf;
    *out_len = n;
    return 0;
}

static int build_sample(const char *sample, char **out_json, size_t *out_len) {
    agorahex_message_t m;
    char *json = NULL;
    size_t jlen = 0;

    memset(&m, 0, sizeof m);
    if (strcmp(sample, "hangup") == 0) {
        m.kind = AGORAHEX_KIND_HANGUP_INDICATION;
        m.u.hangup_indication.call_id = strdup("00000000-0000-0000-0000-000000000001");
        m.u.hangup_indication.drop_code = 0;
        if (!m.u.hangup_indication.call_id) {
            return -1;
        }
    } else if (strcmp(sample, "muted") == 0) {
        m.kind = AGORAHEX_KIND_MUTED_INDICATION;
        m.u.muted_indication.call_id = strdup("00000000-0000-0000-0000-000000000001");
        m.u.muted_indication.muted = true;
        if (!m.u.muted_indication.call_id) {
            return -1;
        }
    } else if (strcmp(sample, "start_content_request") == 0) {
        m.kind = AGORAHEX_KIND_AVC_START_CONTENT_REQUEST;
        m.u.avc_start_content_request.call_id = strdup("00000000-0000-0000-0000-000000000001");
        if (!m.u.avc_start_content_request.call_id) {
            return -1;
        }
    } else {
        return -1;
    }

    if (agorahex_marshal_envelope(&m, &json, &jlen) != AGORAHEX_OK) {
        agorahex_message_free(&m);
        return -1;
    }
    agorahex_message_free(&m);
    *out_json = json;
    *out_len = jlen;
    return 0;
}

static void usage(const char *argv0) {
    fprintf(stderr, "usage: %s -port tcp_port (-file envelope.json | -sample hangup|muted|start_content_request)\n",
            argv0);
}

int main(int argc, char **argv) {
    int port = 9876;
    const char *file = NULL;
    const char *sample = NULL;
    char *payload = NULL;
    size_t payload_len = 0;
    int rc;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-file") == 0 && i + 1 < argc) {
            file = argv[++i];
        } else if (strcmp(argv[i], "-sample") == 0 && i + 1 < argc) {
            sample = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "invalid tcp port: %d\n", port);
        return 2;
    }
    if (!!file == !!sample) {
        usage(argv[0]);
        return 2;
    }

    if (file) {
        if (read_file(file, &payload, &payload_len) != 0) {
            return 1;
        }
    } else {
        if (build_sample(sample, &payload, &payload_len) != 0) {
            fprintf(stderr, "unknown or invalid sample: %s\n", sample);
            return 2;
        }
    }

    agorahex_message_t chk;
    memset(&chk, 0, sizeof chk);
    rc = agorahex_parse_envelope(payload, payload_len, &chk);
    if (rc != AGORAHEX_OK) {
        fprintf(stderr, "parse_envelope_local err=%d (%s)\n", rc, agorahex_strerror((agorahex_result_t)rc));
        free(payload);
        return 1;
    }
    agorahex_message_free(&chk);

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    (void)sigaction(SIGINT, &sa, NULL);

    rc = agorahex_signal_start(AGORAHEX_SIGNAL_CLIENT_MODE, port, client_cb);
    if (rc != AGORAHEX_OK) {
        fprintf(stderr, "agorahex_signal_start err=%d (%s)\n", rc, agorahex_strerror((agorahex_result_t)rc));
        free(payload);
        return 1;
    }

    fprintf(stderr, "connecting to local signal tcp port %d\n", port);
    long long next_send_at = now_ms();
    while (!g_stop) {
        rc = agorahex_signal_poll(200);
        if (rc != AGORAHEX_OK) {
            fprintf(stderr, "agorahex_signal_poll err=%d (%s)\n", rc, agorahex_strerror((agorahex_result_t)rc));
            agorahex_signal_close();
            free(payload);
            return 1;
        }

        if (now_ms() >= next_send_at) {
            rc = agorahex_signal_send(AGORAHEX_SIGNAL_BROADCAST_FD, payload, (int)payload_len);
            if (rc == AGORAHEX_OK) {
                fprintf(stderr, "sent %zu bytes to local port %d\n", payload_len, port);
            } else if (rc != AGORAHEX_ERR_NOT_CONNECTED) {
                fprintf(stderr, "agorahex_signal_send err=%d (%s)\n", rc, agorahex_strerror((agorahex_result_t)rc));
                agorahex_signal_close();
                free(payload);
                return 1;
            } else {
                fprintf(stderr, "send skipped: not connected yet\n");
            }
            next_send_at = now_ms() + 1000ll;
        }
    }

    agorahex_signal_close();
    free(payload);
    return 0;
}
