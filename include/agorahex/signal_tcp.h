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

/**
 * Callback invoked when a complete JSON signal message is received and parsed.
 * Usage: pass this callback to agorahex_signal_start(), then keep calling agorahex_signal_poll()
 * so incoming frames can be dispatched here.
 * Parameters:
 *   fd    - peer connection fd; in server mode it identifies the client, in client mode it is the server fd.
 *   json  - raw JSON payload bytes for the received envelope.
 *   len   - byte length of json; json is not required to be NUL-terminated.
 *   msg_t - parsed message view for the same payload; caller must not free or retain it after callback returns.
 * Returns: none.
 */
typedef void (*agorahex_signal_cb_t)(int fd, const void *json, int len, agorahex_message_t *msg_t);

/**
 * Starts the local TCP signal runtime in client or server mode.
 * Usage:
 *   1. Provide a non-NULL callback.
 *   2. Call this function once before agorahex_signal_send() or agorahex_signal_poll().
 *   3. After success, repeatedly call agorahex_signal_poll() to drive connect/accept/read events.
 *   4. Call agorahex_signal_close() when finished.
 * Parameters:
 *   server_mode - AGORAHEX_SIGNAL_CLIENT_MODE or AGORAHEX_SIGNAL_SERVER_MODE.
 *   tcp_port    - local TCP port on 127.0.0.1, valid range 1..65535.
 *   cb          - receive callback for decoded JSON messages.
 * Returns:
 *   AGORAHEX_OK on success.
 *   AGORAHEX_ERR_ALREADY_STARTED if the runtime is already active.
 *   AGORAHEX_ERR_INVALID_ARG if mode, port, or callback is invalid.
 *   AGORAHEX_ERR_IO if socket setup, bind, listen, or connect initialization fails.
 */
int agorahex_signal_start(int server_mode, int tcp_port, agorahex_signal_cb_t cb);

/**
 * Sends one JSON signal envelope over the active TCP signal runtime.
 * Usage:
 *   - Call agorahex_signal_start() successfully first.
 *   - In server mode, pass a connected client fd or AGORAHEX_SIGNAL_BROADCAST_FD to broadcast.
 *   - In client mode, fd is ignored and the message is sent to the connected server.
 *   - json/len must describe one complete envelope accepted by agorahex_parse_envelope().
 * Parameters:
 *   fd   - target client fd in server mode, or AGORAHEX_SIGNAL_BROADCAST_FD for broadcast; ignored in client mode.
 *   json - raw JSON payload bytes to frame and send.
 *   len  - byte length of json; may be 0 only if the payload is valid for the parser.
 * Returns:
 *   AGORAHEX_OK on success.
 *   AGORAHEX_ERR_NOT_STARTED if the runtime has not been started.
 *   AGORAHEX_ERR_INVALID_ARG if json/len is invalid.
 *   Parser errors from agorahex_parse_envelope() if the JSON envelope is invalid.
 *   AGORAHEX_ERR_NO_MEMORY if frame allocation fails.
 *   AGORAHEX_ERR_NOT_CONNECTED if client mode is not currently connected.
 *   AGORAHEX_ERR_NOT_FOUND if a server-mode target fd does not exist.
 *   AGORAHEX_ERR_IO if sending fails and the connection is closed.
 */
int agorahex_signal_send(int fd, const void *json, int len);

/**
 * Advances the TCP signal runtime by processing pending socket events.
 * Usage: call this in the main loop after agorahex_signal_start(); both client reconnect progress
 * and server/client receive callbacks depend on polling.
 * Parameters:
 *   timeout_ms - maximum time to wait for socket activity in milliseconds; 0 means non-blocking poll.
 * Returns:
 *   AGORAHEX_OK on success, including when no event is ready before timeout.
 *   AGORAHEX_ERR_NOT_STARTED if the runtime has not been started.
 *   AGORAHEX_ERR_IO if polling or socket processing fails.
 *   AGORAHEX_ERR_NOT_CONNECTED may be returned transiently while the client is still reconnecting.
 */
int agorahex_signal_poll(int timeout_ms);

/**
 * Stops the TCP signal runtime and closes all owned sockets.
 * Usage: call once when signal communication is no longer needed, or before restarting with a new mode/port.
 * Parameters: none.
 * Returns: none.
 */
void agorahex_signal_close(void);

#ifdef __cplusplus
}
#endif

#endif /* AGORAHEX_SIGNAL_TCP_H */
