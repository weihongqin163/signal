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

static volatile sig_atomic_t g_stop;

static void on_sigint(int signo) {
    (void)signo;
    g_stop = 1;
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

static void server_cb(int fd, const void *json, int len, agorahex_message_t *msg_t) {
    const char *cid;
    int rc;

    if (fd < 0 || !json || len < 0 || !msg_t) {
        return;
    }
    cid = extract_call_id(msg_t);
    fprintf(stdout, "message fd=%d kind=%s callId=%s jsonLen=%d\n", fd, agorahex_kind_cstr(msg_t->kind),
            cid ? cid : "", len);
    rc = agorahex_signal_send(fd, json, len);
    if (rc != AGORAHEX_OK) {
        fprintf(stderr, "echo send err=%d (%s)\n", rc, agorahex_strerror((agorahex_result_t)rc));
        return;
    }
    fprintf(stdout, "echoed fd=%d kind=%s callId=%s jsonLen=%d\n", fd, agorahex_kind_cstr(msg_t->kind),
            cid ? cid : "", len);
    fflush(stdout);
}

static void usage(const char *argv0) {
    fprintf(stderr, "usage: %s [-port tcp_port]\n", argv0);
}

int main(int argc, char **argv) {
    int port = 9876;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
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

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    (void)sigaction(SIGINT, &sa, NULL);

    int rc = agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, port, server_cb);
    if (rc != AGORAHEX_OK) {
        fprintf(stderr, "agorahex_signal_start err=%d (%s)\n", rc, agorahex_strerror((agorahex_result_t)rc));
        return 1;
    }

    fprintf(stderr, "listening local signal tcp port %d\n", port);
    while (!g_stop) {
        rc = agorahex_signal_poll(200);
        if (rc != AGORAHEX_OK) {
            fprintf(stderr, "agorahex_signal_poll err=%d (%s)\n", rc, agorahex_strerror((agorahex_result_t)rc));
            agorahex_signal_close();
            return 1;
        }
    }

    agorahex_signal_close();
    return 0;
}
