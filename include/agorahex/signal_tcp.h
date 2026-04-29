/**
 * author: wei
 * date: 2026-04-29
 * copyright 2026 agora.io
 */

#ifndef AGORAHEX_SIGNAL_TCP_H
#define AGORAHEX_SIGNAL_TCP_H

#include "envelope.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AGORAHEX_SIGNAL_CLIENT_MODE 0
#define AGORAHEX_SIGNAL_SERVER_MODE 1
#define AGORAHEX_SIGNAL_MAX_CLIENTS 8
#define AGORAHEX_SIGNAL_BROADCAST_FD (-1)

typedef void (*agorahex_signal_cb_t)(int fd, const void *json, int len, agorahex_message_t *msg_t);

int agorahex_signal_start(int server_mode, int tcp_port, agorahex_signal_cb_t cb);
int agorahex_signal_send(int fd, const void *buffer, int len);
int agorahex_signal_poll(int timeout_ms);
void agorahex_signal_close(void);

#ifdef __cplusplus
}
#endif

#endif /* AGORAHEX_SIGNAL_TCP_H */
