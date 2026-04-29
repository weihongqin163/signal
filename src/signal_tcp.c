/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#define _POSIX_C_SOURCE 200809L

#include "agorahex/signal_tcp.h"

#include "agorahex/envelope.h"
#include "agorahex/framing.h"
#include "agorahex/result.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef enum agorahex_signal_conn_state {
    AGORAHEX_SIGNAL_CONN_DISCONNECTED = 0,
    AGORAHEX_SIGNAL_CONN_CONNECTING = 1,
    AGORAHEX_SIGNAL_CONN_CONNECTED = 2,
} agorahex_signal_conn_state_t;

typedef struct agorahex_signal_conn {
    int fd;
    int state;
    agorahex_frame_decoder_t decoder;
} agorahex_signal_conn_t;

typedef struct agorahex_signal_server {
    int server_fd;
    int tcp_port;
    int running;
    agorahex_signal_cb_t cb;
    agorahex_signal_conn_t clients[AGORAHEX_SIGNAL_MAX_CLIENTS];
    int client_count;
} agorahex_signal_server_t;

typedef struct agorahex_signal_client {
    agorahex_signal_conn_t conn;
    int tcp_port;
    int running;
    agorahex_signal_cb_t cb;
} agorahex_signal_client_t;

typedef struct agorahex_signal_runtime {
    int mode;
    agorahex_signal_server_t server;
    agorahex_signal_client_t client;
} agorahex_signal_runtime_t;

typedef struct agorahex_signal_dispatch_ctx {
    int fd;
    agorahex_signal_cb_t cb;
} agorahex_signal_dispatch_ctx_t;

static agorahex_signal_runtime_t g_runtime;
static int g_runtime_initialized;

static void ensure_runtime_initialized(void);
static void runtime_reset(void);
static void conn_reset(agorahex_signal_conn_t *conn);
static int set_nonblocking(int fd);
static int set_no_sigpipe(int fd);
static int create_stream_socket(void);
static int start_server(int tcp_port, agorahex_signal_cb_t cb);
static int start_client(int tcp_port, agorahex_signal_cb_t cb);
static void close_server_mode(void);
static void close_client_mode(void);
static int validate_signal_payload(const void *buffer, int len);
static int write_all_or_close(agorahex_signal_conn_t *conn, const uint8_t *buf, size_t len);
static int server_send_one(int fd, const uint8_t *frame, size_t frame_len);
static int server_send_all(const uint8_t *frame, size_t frame_len);
static int handle_server_poll(int timeout_ms);
static int handle_client_poll(int timeout_ms);
static int handle_server_readable(agorahex_signal_conn_t *conn);
static int handle_client_readable(agorahex_signal_conn_t *conn, agorahex_signal_cb_t cb);
static int accept_new_clients(agorahex_signal_server_t *server);
static int find_free_client_slot(const agorahex_signal_server_t *server);
static agorahex_signal_conn_t *find_server_client_by_fd(int fd);
static int client_begin_connect(agorahex_signal_client_t *client);
static int check_connect_complete(int fd);
static agorahex_result_t on_signal_frame(void *ctx, const uint8_t *json, size_t json_len);

static void ensure_runtime_initialized(void) {
    int i;
    if (!g_runtime_initialized) {
        memset(&g_runtime, 0, sizeof g_runtime);
        g_runtime.mode = -1;
        g_runtime.server.server_fd = AGORAHEX_SIGNAL_BROADCAST_FD;
        g_runtime.client.conn.fd = AGORAHEX_SIGNAL_BROADCAST_FD;
        for (i = 0; i < AGORAHEX_SIGNAL_MAX_CLIENTS; i++) {
            g_runtime.server.clients[i].fd = AGORAHEX_SIGNAL_BROADCAST_FD;
        }
        runtime_reset();
        g_runtime_initialized = 1;
    }
}

static void conn_reset(agorahex_signal_conn_t *conn) {
    if (!conn) {
        return;
    }
    if (conn->fd >= 0) {
        close(conn->fd);
    }
    conn->fd = AGORAHEX_SIGNAL_BROADCAST_FD;
    conn->state = AGORAHEX_SIGNAL_CONN_DISCONNECTED;
    agorahex_frame_decoder_fini(&conn->decoder);
    agorahex_frame_decoder_init(&conn->decoder, AGORAHEX_DEFAULT_MAX_FRAME_BYTES);
}

static void runtime_reset(void) {
    int i;

    g_runtime.mode = -1;

    g_runtime.server.server_fd = AGORAHEX_SIGNAL_BROADCAST_FD;
    g_runtime.server.tcp_port = 0;
    g_runtime.server.running = 0;
    g_runtime.server.cb = NULL;
    g_runtime.server.client_count = 0;
    for (i = 0; i < AGORAHEX_SIGNAL_MAX_CLIENTS; i++) {
        conn_reset(&g_runtime.server.clients[i]);
    }

    g_runtime.client.tcp_port = 0;
    g_runtime.client.running = 0;
    g_runtime.client.cb = NULL;
    conn_reset(&g_runtime.client.conn);
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        return -1;
    }
    return 0;
}

static int set_no_sigpipe(int fd) {
#ifdef SO_NOSIGPIPE
    int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) != 0) {
        return -1;
    }
#else
    (void)fd;
#endif
    return 0;
}

static int create_stream_socket(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    if (set_nonblocking(fd) != 0 || set_no_sigpipe(fd) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int validate_signal_payload(const void *buffer, int len) {
    agorahex_message_t msg;
    agorahex_result_t r;

    if (len < 0 || (!buffer && len > 0)) {
        return AGORAHEX_ERR_INVALID_ARG;
    }
    memset(&msg, 0, sizeof msg);
    r = agorahex_parse_envelope((const char *)buffer, (size_t)len, &msg);
    if (r != AGORAHEX_OK) {
        return (int)r;
    }
    agorahex_message_free(&msg);
    return AGORAHEX_OK;
}

static int start_server(int tcp_port, agorahex_signal_cb_t cb) {
    int fd;
    int one = 1;
    struct sockaddr_in addr;
    int i;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return AGORAHEX_ERR_IO;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
        close(fd);
        return AGORAHEX_ERR_IO;
    }
    if (set_nonblocking(fd) != 0 || set_no_sigpipe(fd) != 0) {
        close(fd);
        return AGORAHEX_ERR_IO;
    }

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)tcp_port);
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        close(fd);
        return AGORAHEX_ERR_IO;
    }
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        close(fd);
        return AGORAHEX_ERR_IO;
    }
    if (listen(fd, AGORAHEX_SIGNAL_MAX_CLIENTS) != 0) {
        close(fd);
        return AGORAHEX_ERR_IO;
    }

    g_runtime.mode = AGORAHEX_SIGNAL_SERVER_MODE;
    g_runtime.server.server_fd = fd;
    g_runtime.server.tcp_port = tcp_port;
    g_runtime.server.running = 1;
    g_runtime.server.cb = cb;
    g_runtime.server.client_count = 0;
    for (i = 0; i < AGORAHEX_SIGNAL_MAX_CLIENTS; i++) {
        conn_reset(&g_runtime.server.clients[i]);
    }
    return AGORAHEX_OK;
}

static int start_client(int tcp_port, agorahex_signal_cb_t cb) {
    g_runtime.mode = AGORAHEX_SIGNAL_CLIENT_MODE;
    g_runtime.client.tcp_port = tcp_port;
    g_runtime.client.running = 1;
    g_runtime.client.cb = cb;
    conn_reset(&g_runtime.client.conn);
    (void)client_begin_connect(&g_runtime.client);
    return AGORAHEX_OK;
}

int agorahex_signal_start(int server_mode, int tcp_port, agorahex_signal_cb_t cb) {
    ensure_runtime_initialized();
    if (g_runtime.mode != -1) {
        return AGORAHEX_ERR_ALREADY_STARTED;
    }
    if ((server_mode != AGORAHEX_SIGNAL_CLIENT_MODE && server_mode != AGORAHEX_SIGNAL_SERVER_MODE) || !cb ||
        tcp_port <= 0 || tcp_port > 65535) {
        return AGORAHEX_ERR_INVALID_ARG;
    }
    if (server_mode == AGORAHEX_SIGNAL_SERVER_MODE) {
        return start_server(tcp_port, cb);
    }
    return start_client(tcp_port, cb);
}

static int write_all_or_close(agorahex_signal_conn_t *conn, const uint8_t *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
#ifdef MSG_NOSIGNAL
        ssize_t n = send(conn->fd, buf + off, len - off, MSG_NOSIGNAL);
#else
        ssize_t n = send(conn->fd, buf + off, len - off, 0);
#endif
        if (n > 0) {
            off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        conn_reset(conn);
        return AGORAHEX_ERR_IO;
    }
    return AGORAHEX_OK;
}

static agorahex_signal_conn_t *find_server_client_by_fd(int fd) {
    int i;
    for (i = 0; i < AGORAHEX_SIGNAL_MAX_CLIENTS; i++) {
        if (g_runtime.server.clients[i].fd == fd &&
            g_runtime.server.clients[i].state == AGORAHEX_SIGNAL_CONN_CONNECTED) {
            return &g_runtime.server.clients[i];
        }
    }
    return NULL;
}

static int server_send_one(int fd, const uint8_t *frame, size_t frame_len) {
    agorahex_signal_conn_t *conn = find_server_client_by_fd(fd);
    if (!conn) {
        return AGORAHEX_ERR_NOT_FOUND;
    }
    if (write_all_or_close(conn, frame, frame_len) != AGORAHEX_OK) {
        if (g_runtime.server.client_count > 0) {
            g_runtime.server.client_count--;
        }
        return AGORAHEX_ERR_IO;
    }
    return AGORAHEX_OK;
}

static int server_send_all(const uint8_t *frame, size_t frame_len) {
    int i;
    int rc = AGORAHEX_ERR_NOT_FOUND;

    for (i = 0; i < AGORAHEX_SIGNAL_MAX_CLIENTS; i++) {
        agorahex_signal_conn_t *conn = &g_runtime.server.clients[i];
        int send_rc;
        if (conn->fd < 0 || conn->state != AGORAHEX_SIGNAL_CONN_CONNECTED) {
            continue;
        }
        send_rc = write_all_or_close(conn, frame, frame_len);
        if (send_rc == AGORAHEX_OK) {
            rc = AGORAHEX_OK;
            continue;
        }
        if (g_runtime.server.client_count > 0) {
            g_runtime.server.client_count--;
        }
        if (rc == AGORAHEX_ERR_NOT_FOUND) {
            rc = send_rc;
        }
    }
    return rc;
}

int agorahex_signal_send(int fd, const void *buffer, int len) {
    uint8_t *frame;
    size_t frame_len;
    int rc;

    ensure_runtime_initialized();
    if (g_runtime.mode == -1) {
        return AGORAHEX_ERR_NOT_STARTED;
    }
    rc = validate_signal_payload(buffer, len);
    if (rc != AGORAHEX_OK) {
        return rc;
    }

    frame_len = agorahex_frame_encoded_size((size_t)len);
    frame = (uint8_t *)malloc(frame_len);
    if (!frame) {
        return AGORAHEX_ERR_NO_MEMORY;
    }
    agorahex_frame_encode((const uint8_t *)buffer, (size_t)len, frame);

    if (g_runtime.mode == AGORAHEX_SIGNAL_SERVER_MODE) {
        rc = (fd == AGORAHEX_SIGNAL_BROADCAST_FD) ? server_send_all(frame, frame_len) : server_send_one(fd, frame, frame_len);
    } else {
        if (g_runtime.client.conn.state != AGORAHEX_SIGNAL_CONN_CONNECTED || g_runtime.client.conn.fd < 0) {
            free(frame);
            return AGORAHEX_ERR_NOT_CONNECTED;
        }
        rc = write_all_or_close(&g_runtime.client.conn, frame, frame_len);
        if (rc != AGORAHEX_OK) {
            return rc;
        }
    }
    free(frame);
    return rc;
}

static int find_free_client_slot(const agorahex_signal_server_t *server) {
    int i;
    for (i = 0; i < AGORAHEX_SIGNAL_MAX_CLIENTS; i++) {
        if (server->clients[i].fd == AGORAHEX_SIGNAL_BROADCAST_FD) {
            return i;
        }
    }
    return -1;
}

static int accept_new_clients(agorahex_signal_server_t *server) {
    for (;;) {
        int cfd = accept(server->server_fd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return AGORAHEX_OK;
            }
            return AGORAHEX_ERR_IO;
        }

        int slot = find_free_client_slot(server);
        if (slot < 0) {
            fprintf(stderr, "agorahex_signal_tcp: client limit reached\n");
            close(cfd);
            continue;
        }
        if (set_nonblocking(cfd) != 0 || set_no_sigpipe(cfd) != 0) {
            close(cfd);
            continue;
        }

        conn_reset(&server->clients[slot]);
        server->clients[slot].fd = cfd;
        server->clients[slot].state = AGORAHEX_SIGNAL_CONN_CONNECTED;
        agorahex_frame_decoder_reset(&server->clients[slot].decoder);
        server->client_count++;
    }
}

static agorahex_result_t on_signal_frame(void *ctx, const uint8_t *json, size_t json_len) {
    agorahex_signal_dispatch_ctx_t *dispatch = (agorahex_signal_dispatch_ctx_t *)ctx;
    agorahex_message_t msg;
    agorahex_result_t r;

    memset(&msg, 0, sizeof msg);
    r = agorahex_parse_envelope((const char *)json, json_len, &msg);
    if (r != AGORAHEX_OK) {
        return r;
    }
    if (dispatch->cb) {
        dispatch->cb(dispatch->fd, json, (int)json_len, &msg);
    }
    agorahex_message_free(&msg);
    return AGORAHEX_OK;
}

static int handle_server_readable(agorahex_signal_conn_t *conn) {
    uint8_t buf[32u << 10u];
    agorahex_signal_dispatch_ctx_t ctx;

    ctx.fd = conn->fd;
    ctx.cb = g_runtime.server.cb;
    for (;;) {
        ssize_t n = recv(conn->fd, buf, sizeof buf, 0);
        if (n > 0) {
            agorahex_result_t r = agorahex_frame_decoder_append(&conn->decoder, buf, (size_t)n, &ctx, on_signal_frame);
            if (r != AGORAHEX_OK) {
                return (int)r;
            }
            continue;
        }
        if (n == 0) {
            return AGORAHEX_ERR_IO;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return AGORAHEX_OK;
        }
        return AGORAHEX_ERR_IO;
    }
}

static int handle_client_readable(agorahex_signal_conn_t *conn, agorahex_signal_cb_t cb) {
    uint8_t buf[32u << 10u];
    agorahex_signal_dispatch_ctx_t ctx;

    ctx.fd = conn->fd;
    ctx.cb = cb;
    for (;;) {
        ssize_t n = recv(conn->fd, buf, sizeof buf, 0);
        if (n > 0) {
            agorahex_result_t r = agorahex_frame_decoder_append(&conn->decoder, buf, (size_t)n, &ctx, on_signal_frame);
            if (r != AGORAHEX_OK) {
                return (int)r;
            }
            continue;
        }
        if (n == 0) {
            return AGORAHEX_ERR_IO;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return AGORAHEX_OK;
        }
        return AGORAHEX_ERR_IO;
    }
}

static int handle_server_poll(int timeout_ms) {
    struct pollfd pfds[1 + AGORAHEX_SIGNAL_MAX_CLIENTS];
    int slots[1 + AGORAHEX_SIGNAL_MAX_CLIENTS];
    int nfds = 0;
    int i;
    int rc;

    pfds[nfds].fd = g_runtime.server.server_fd;
    pfds[nfds].events = POLLIN;
    pfds[nfds].revents = 0;
    slots[nfds] = -1;
    nfds++;

    for (i = 0; i < AGORAHEX_SIGNAL_MAX_CLIENTS; i++) {
        if (g_runtime.server.clients[i].fd < 0 || g_runtime.server.clients[i].state != AGORAHEX_SIGNAL_CONN_CONNECTED) {
            continue;
        }
        pfds[nfds].fd = g_runtime.server.clients[i].fd;
        pfds[nfds].events = POLLIN;
        pfds[nfds].revents = 0;
        slots[nfds] = i;
        nfds++;
    }

    rc = poll(pfds, (nfds_t)nfds, timeout_ms);
    if (rc < 0) {
        return (errno == EINTR) ? AGORAHEX_OK : AGORAHEX_ERR_IO;
    }
    if (rc == 0) {
        return AGORAHEX_OK;
    }

    if ((pfds[0].revents & POLLIN) != 0) {
        rc = accept_new_clients(&g_runtime.server);
        if (rc != AGORAHEX_OK) {
            return rc;
        }
    }
    if ((pfds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        close_server_mode();
        return AGORAHEX_ERR_IO;
    }

    for (i = 1; i < nfds; i++) {
        int slot = slots[i];
        agorahex_signal_conn_t *conn;
        int read_rc;
        if (slot < 0) {
            continue;
        }
        conn = &g_runtime.server.clients[slot];
        if ((pfds[i].revents & POLLIN) != 0) {
            read_rc = handle_server_readable(conn);
            if (read_rc != AGORAHEX_OK) {
                conn_reset(conn);
                if (g_runtime.server.client_count > 0) {
                    g_runtime.server.client_count--;
                }
                continue;
            }
        }
        if ((pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            conn_reset(conn);
            if (g_runtime.server.client_count > 0) {
                g_runtime.server.client_count--;
            }
        }
    }
    return AGORAHEX_OK;
}

static int check_connect_complete(int fd) {
    int err = 0;
    socklen_t len = (socklen_t)sizeof(err);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) != 0) {
        return -1;
    }
    if (err != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

static int client_begin_connect(agorahex_signal_client_t *client) {
    struct sockaddr_in addr;
    int fd;
    int rc;

    conn_reset(&client->conn);
    fd = create_stream_socket();
    if (fd < 0) {
        client->conn.state = AGORAHEX_SIGNAL_CONN_DISCONNECTED;
        return AGORAHEX_ERR_IO;
    }

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)client->tcp_port);
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        conn_reset(&client->conn);
        return AGORAHEX_ERR_IO;
    }

    client->conn.fd = fd;
    rc = connect(fd, (struct sockaddr *)&addr, sizeof addr);
    if (rc == 0) {
        client->conn.state = AGORAHEX_SIGNAL_CONN_CONNECTED;
        return AGORAHEX_OK;
    }
    if (errno == EINPROGRESS || errno == EALREADY || errno == EWOULDBLOCK) {
        client->conn.state = AGORAHEX_SIGNAL_CONN_CONNECTING;
        return AGORAHEX_OK;
    }

    conn_reset(&client->conn);
    return AGORAHEX_ERR_IO;
}

static int handle_client_poll(int timeout_ms) {
    agorahex_signal_client_t *client = &g_runtime.client;

    if (client->conn.state == AGORAHEX_SIGNAL_CONN_DISCONNECTED) {
        (void)client_begin_connect(client);
        return AGORAHEX_OK;
    }

    if (client->conn.fd < 0) {
        client->conn.state = AGORAHEX_SIGNAL_CONN_DISCONNECTED;
        return AGORAHEX_OK;
    }

    {
        struct pollfd pfd;
        int rc;

        pfd.fd = client->conn.fd;
        pfd.events = (short)((client->conn.state == AGORAHEX_SIGNAL_CONN_CONNECTING) ? POLLOUT : POLLIN);
        pfd.revents = 0;

        rc = poll(&pfd, 1, timeout_ms);
        if (rc < 0) {
            return (errno == EINTR) ? AGORAHEX_OK : AGORAHEX_ERR_IO;
        }
        if (rc == 0) {
            return AGORAHEX_OK;
        }

        if (client->conn.state == AGORAHEX_SIGNAL_CONN_CONNECTING) {
            if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 || check_connect_complete(client->conn.fd) != 0) {
                conn_reset(&client->conn);
                return AGORAHEX_OK;
            }
            client->conn.state = AGORAHEX_SIGNAL_CONN_CONNECTED;
            if ((pfd.revents & POLLIN) == 0) {
                return AGORAHEX_OK;
            }
        }

        if ((pfd.revents & POLLIN) != 0) {
            rc = handle_client_readable(&client->conn, client->cb);
            if (rc != AGORAHEX_OK) {
                conn_reset(&client->conn);
                return AGORAHEX_OK;
            }
        }
        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            conn_reset(&client->conn);
        }
    }
    return AGORAHEX_OK;
}

int agorahex_signal_poll(int timeout_ms) {
    ensure_runtime_initialized();
    if (g_runtime.mode == -1) {
        return AGORAHEX_ERR_NOT_STARTED;
    }
    if (timeout_ms < -1) {
        return AGORAHEX_ERR_INVALID_ARG;
    }
    if (g_runtime.mode == AGORAHEX_SIGNAL_SERVER_MODE) {
        return handle_server_poll(timeout_ms);
    }
    return handle_client_poll(timeout_ms);
}

static void close_server_mode(void) {
    int i;
    if (g_runtime.server.server_fd >= 0) {
        close(g_runtime.server.server_fd);
        g_runtime.server.server_fd = AGORAHEX_SIGNAL_BROADCAST_FD;
    }
    for (i = 0; i < AGORAHEX_SIGNAL_MAX_CLIENTS; i++) {
        conn_reset(&g_runtime.server.clients[i]);
    }
    g_runtime.server.client_count = 0;
    g_runtime.server.running = 0;
    g_runtime.server.cb = NULL;
    g_runtime.server.tcp_port = 0;
}

static void close_client_mode(void) {
    conn_reset(&g_runtime.client.conn);
    g_runtime.client.running = 0;
    g_runtime.client.cb = NULL;
    g_runtime.client.tcp_port = 0;
}

void agorahex_signal_close(void) {
    ensure_runtime_initialized();
    if (g_runtime.mode == AGORAHEX_SIGNAL_SERVER_MODE) {
        close_server_mode();
    } else if (g_runtime.mode == AGORAHEX_SIGNAL_CLIENT_MODE) {
        close_client_mode();
    }
    g_runtime.mode = -1;
}
