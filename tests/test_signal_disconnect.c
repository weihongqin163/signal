/**
 * author: wei
 * date: 2026-08-17
 * copyright 2026 agora.io
 */

#define _POSIX_C_SOURCE 200809L

#include "agorahex/framing.h"
#include "agorahex/result.h"
#include "agorahex/signal_tcp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static char g_server_addr[] = "127.0.0.1";
static const char g_valid_message[] =
    "{\"HangupIndication\":{\"callId\":\"00000000-0000-0000-0000-000000000001\",\"dropCode\":0}}";
static const char g_reply[] =
    "{\"MutedIndication\":{\"callId\":\"00000000-0000-0000-0000-000000000001\",\"muted\":true}}";

static int g_message_fd = AGORAHEX_SIGNAL_BROADCAST_FD;
static int g_disconnect_count;
static int g_disconnect_fd = AGORAHEX_SIGNAL_BROADCAST_FD;
static agorahex_signal_disconnect_reason_t g_disconnect_reason;

static void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000l);
    ts.tv_nsec = (long)((ms % 1000l) * 1000000l);
    nanosleep(&ts, NULL);
}

static void reset_observations(void) {
    g_message_fd = AGORAHEX_SIGNAL_BROADCAST_FD;
    g_disconnect_count = 0;
    g_disconnect_fd = AGORAHEX_SIGNAL_BROADCAST_FD;
    g_disconnect_reason = AGORAHEX_SIGNAL_DISCONNECT_PEER_CLOSED;
}

static void message_cb(int fd, const void *json, int len, agorahex_message_t *msg_t) {
    (void)json;
    (void)len;
    if (msg_t && msg_t->kind == AGORAHEX_KIND_HANGUP_INDICATION) {
        g_message_fd = fd;
    }
}

static void disconnect_cb(int fd, agorahex_signal_disconnect_reason_t reason) {
    g_disconnect_count++;
    g_disconnect_fd = fd;
    g_disconnect_reason = reason;
}

static int connect_raw_client(int port) {
    struct sockaddr_in addr;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, g_server_addr, &addr.sin_addr) != 1 ||
        connect(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int send_frame(int fd, const char *json) {
    size_t json_len = strlen(json);
    size_t frame_len = agorahex_frame_encoded_size(json_len);
    uint8_t *frame = (uint8_t *)malloc(frame_len);
    size_t off = 0;
    if (!frame) {
        return -1;
    }
    agorahex_frame_encode((const uint8_t *)json, json_len, frame);
    while (off < frame_len) {
        ssize_t n = send(fd, frame + off, frame_len - off, 0);
        if (n > 0) {
            off += (size_t)n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            free(frame);
            return -1;
        }
    }
    free(frame);
    return 0;
}

static int start_server(int port, agorahex_signal_disconnect_cb_t cb) {
    int rc = agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, g_server_addr, port, message_cb, cb);
    if (rc == AGORAHEX_ERR_IO) {
        fprintf(stderr, "skip: local tcp bind not permitted in this environment\n");
        return 77;
    }
    return rc == AGORAHEX_OK ? 0 : 1;
}

static int accept_client(void) {
    return agorahex_signal_poll(50) == AGORAHEX_OK ? 0 : 1;
}

static int test_protocol_error(int port) {
    int client_fd;
    int rc;
    reset_observations();
    rc = start_server(port, disconnect_cb);
    if (rc != 0) {
        return rc;
    }
    client_fd = connect_raw_client(port);
    if (client_fd < 0 || accept_client() != 0 || send_frame(client_fd, "{}") != 0 ||
        agorahex_signal_poll(50) != AGORAHEX_OK) {
        if (client_fd >= 0) {
            close(client_fd);
        }
        agorahex_signal_close();
        return 1;
    }
    close(client_fd);
    if (g_disconnect_count != 1 || g_disconnect_reason != AGORAHEX_SIGNAL_DISCONNECT_PROTOCOL_ERROR ||
        g_disconnect_fd < 0) {
        agorahex_signal_close();
        return 1;
    }
    (void)agorahex_signal_poll(0);
    agorahex_signal_close();
    return g_disconnect_count == 1 ? 0 : 1;
}

static int test_send_failure(int port) {
    struct linger reset = {1, 0};
    int client_fd;
    int rc;
    reset_observations();
    rc = start_server(port, disconnect_cb);
    if (rc != 0) {
        return rc;
    }
    client_fd = connect_raw_client(port);
    if (client_fd < 0 || accept_client() != 0 || send_frame(client_fd, g_valid_message) != 0 ||
        agorahex_signal_poll(50) != AGORAHEX_OK || g_message_fd < 0) {
        if (client_fd >= 0) {
            close(client_fd);
        }
        agorahex_signal_close();
        return 1;
    }
    (void)setsockopt(client_fd, SOL_SOCKET, SO_LINGER, &reset, sizeof reset);
    close(client_fd);
    sleep_ms(20);
    rc = agorahex_signal_send(g_message_fd, g_reply, (int)(sizeof g_reply - 1u));
    if (rc != AGORAHEX_ERR_IO || g_disconnect_count != 1 || g_disconnect_fd != g_message_fd ||
        g_disconnect_reason != AGORAHEX_SIGNAL_DISCONNECT_IO_ERROR) {
        fprintf(stderr, "send failure callback mismatch rc=%d count=%d reason=%d\n", rc, g_disconnect_count,
                (int)g_disconnect_reason);
        agorahex_signal_close();
        return 1;
    }
    (void)agorahex_signal_poll(0);
    agorahex_signal_close();
    return g_disconnect_count == 1 ? 0 : 1;
}

static int test_no_callback_cases(int port) {
    int client_fd;
    int rc;
    reset_observations();
    rc = start_server(port, NULL);
    if (rc != 0) {
        return rc;
    }
    client_fd = connect_raw_client(port);
    if (client_fd < 0 || accept_client() != 0 || send_frame(client_fd, "{}") != 0 ||
        agorahex_signal_poll(50) != AGORAHEX_OK) {
        if (client_fd >= 0) {
            close(client_fd);
        }
        agorahex_signal_close();
        return 1;
    }
    close(client_fd);
    agorahex_signal_close();

    rc = start_server(port + 1, disconnect_cb);
    if (rc != 0) {
        return rc;
    }
    client_fd = connect_raw_client(port + 1);
    if (client_fd < 0 || accept_client() != 0) {
        if (client_fd >= 0) {
            close(client_fd);
        }
        agorahex_signal_close();
        return 1;
    }
    agorahex_signal_close();
    close(client_fd);
    return g_disconnect_count == 0 ? 0 : 1;
}

int main(void) {
    int port = 30000 + (int)(getpid() % 10000);
    int rc = test_protocol_error(port);
    if (rc != 0) {
        return rc == 77 ? 0 : 1;
    }
    rc = test_send_failure(port + 1);
    if (rc != 0) {
        return rc == 77 ? 0 : 1;
    }
    rc = test_no_callback_cases(port + 2);
    return rc == 77 ? 0 : rc;
}
