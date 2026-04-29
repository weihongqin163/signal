/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#define _POSIX_C_SOURCE 200809L

#include "agorahex/result.h"
#include "agorahex/signal_tcp.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static const char g_ping[] = "{\"HangupIndication\":{\"callId\":\"00000000-0000-0000-0000-000000000001\",\"dropCode\":0}}";
static const char g_pong[] = "{\"MutedIndication\":{\"callId\":\"00000000-0000-0000-0000-000000000001\",\"muted\":true}}";

static int g_server_received;
static int g_server_reply_sent;
static int g_server_peer_fd = AGORAHEX_SIGNAL_BROADCAST_FD;
static int g_client_received;

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000ll + (long long)(ts.tv_nsec / 1000000ll);
}

static void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000l);
    ts.tv_nsec = (long)((ms % 1000l) * 1000000l);
    nanosleep(&ts, NULL);
}

static void server_cb(int fd, const void *json, int len, agorahex_message_t *msg_t) {
    if (fd < 0 || !msg_t) {
        return;
    }
    if (msg_t->kind == AGORAHEX_KIND_HANGUP_INDICATION && len == (int)(sizeof g_ping - 1u) &&
        memcmp(json, g_ping, (size_t)len) == 0) {
        g_server_peer_fd = fd;
        g_server_received = 1;
    }
}

static void client_cb(int fd, const void *json, int len, agorahex_message_t *msg_t) {
    if (fd < 0 || !msg_t) {
        return;
    }
    if (msg_t->kind == AGORAHEX_KIND_MUTED_INDICATION && len == (int)(sizeof g_pong - 1u) &&
        memcmp(json, g_pong, (size_t)len) == 0) {
        g_client_received = 1;
    }
}

static int run_server(int port) {
    long long deadline = now_ms() + 5000ll;
    int rc = agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, port, server_cb);
    if (rc != AGORAHEX_OK) {
        if (rc == AGORAHEX_ERR_IO) {
            fprintf(stderr, "skip: local tcp bind not permitted in this environment\n");
            return 77;
        }
        fprintf(stderr, "server: start failed rc=%d\n", rc);
        return 1;
    }
    while (now_ms() < deadline) {
        rc = agorahex_signal_poll(50);
        if (rc < 0) {
            fprintf(stderr, "server: poll failed rc=%d\n", rc);
            agorahex_signal_close();
            return 1;
        }
        if (g_server_received && !g_server_reply_sent && g_server_peer_fd >= 0) {
            rc = agorahex_signal_send(g_server_peer_fd, g_pong, (int)(sizeof g_pong - 1u));
            if (rc != AGORAHEX_OK) {
                fprintf(stderr, "server: send failed rc=%d\n", rc);
                agorahex_signal_close();
                return 1;
            }
            g_server_reply_sent = 1;
        }
        if (g_server_reply_sent) {
            agorahex_signal_close();
            return 0;
        }
    }
    fprintf(stderr, "server: timeout received=%d peer_fd=%d sent=%d\n", g_server_received, g_server_peer_fd, g_server_reply_sent);
    agorahex_signal_close();
    return 1;
}

int main(void) {
    int port = 20000 + (int)(getpid() % 20000);
    int status = 0;
    pid_t pid = fork();
    if (pid < 0) {
        return 1;
    }
    if (pid == 0) {
        int rc = run_server(port);
        _exit(rc);
    }

    sleep_ms(150);
    if (waitpid(pid, &status, WNOHANG) == pid) {
        if (WIFEXITED(status) && WEXITSTATUS(status) == 77) {
            return 0;
        }
        return 1;
    }
    if (agorahex_signal_start(AGORAHEX_SIGNAL_CLIENT_MODE, port, client_cb) != AGORAHEX_OK) {
        fprintf(stderr, "client: start failed\n");
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return 1;
    }

    long long deadline = now_ms() + 5000ll;
    int ping_sent = 0;
    while (now_ms() < deadline && !g_client_received) {
        if (waitpid(pid, &status, WNOHANG) == pid) {
            agorahex_signal_close();
            if (WIFEXITED(status) && WEXITSTATUS(status) == 77) {
                return 0;
            }
            fprintf(stderr, "client: server exited early status=%d\n", status);
            return 1;
        }
        int rc = agorahex_signal_poll(50);
        if (rc < 0 && rc != AGORAHEX_ERR_NOT_CONNECTED) {
            fprintf(stderr, "client: poll failed rc=%d\n", rc);
            agorahex_signal_close();
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            return 1;
        }
        if (!ping_sent) {
            rc = agorahex_signal_send(AGORAHEX_SIGNAL_BROADCAST_FD, g_ping, (int)(sizeof g_ping - 1u));
            if (rc == AGORAHEX_OK) {
                ping_sent = 1;
            } else if (rc != AGORAHEX_ERR_NOT_CONNECTED) {
                fprintf(stderr, "client: send failed rc=%d\n", rc);
                agorahex_signal_close();
                kill(pid, SIGKILL);
                waitpid(pid, NULL, 0);
                return 1;
            }
        }
    }
    agorahex_signal_close();

    if (!g_client_received) {
        fprintf(stderr, "client: timeout ping_sent=%d received=%d\n", ping_sent, g_client_received);
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return 1;
    }

    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "client: waitpid failed\n");
        return 1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 77) {
        return 0;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "client: server exit status=%d\n", status);
        return 1;
    }
    return 0;
}
