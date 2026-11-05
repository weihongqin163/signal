/**
 * author: wei
 * date: 2026-11-05
 * copyright 2026 agora.io
 */

#define _POSIX_C_SOURCE 200809L

#include "agorahex/framing.h"
#include "agorahex/result.h"
#include "agorahex/signal_tcp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static const char g_message[] = "{\"MutedIndication\":{\"callId\":\"bind-test\",\"muted\":true}}";
static int g_received;

static void receive_cb(int fd, const void *json, int len, agorahex_message_t *msg) {
    if (fd >= 0 && msg && msg->kind == AGORAHEX_KIND_MUTED_INDICATION &&
        len == (int)(sizeof g_message - 1u) && memcmp(json, g_message, (size_t)len) == 0) {
        g_received++;
    }
}

static int find_local_ipv4(char *address, size_t size) {
    struct ifaddrs *interfaces;
    int found = 0;

    if (getifaddrs(&interfaces) != 0) {
        perror("getifaddrs");
        return -1;
    }
    for (struct ifaddrs *it = interfaces; it; it = it->ifa_next) {
        if (it->ifa_addr && it->ifa_addr->sa_family == AF_INET) {
            const struct sockaddr_in *addr = (const struct sockaddr_in *)it->ifa_addr;
            uint32_t host_addr = ntohl(addr->sin_addr.s_addr);
            if (host_addr != INADDR_ANY && (host_addr >> 24) != 127 &&
                inet_ntop(AF_INET, &addr->sin_addr, address, (socklen_t)size)) {
                found = 1;
                break;
            }
        }
    }
    freeifaddrs(interfaces);
    return found;
}

static int unused_port(void) {
    struct sockaddr_in addr;
    socklen_t len = sizeof addr;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int port = -1;

    if (fd < 0) {
        return -1;
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    (void)inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) == 0 &&
        getsockname(fd, (struct sockaddr *)&addr, &len) == 0) {
        port = (int)ntohs(addr.sin_port);
    }
    close(fd);
    return port;
}

static int connect_to(const char *address, int port) {
    struct sockaddr_in addr;
    struct pollfd pfd;
    socklen_t error_len = sizeof(int);
    int error = 0;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int rc;

    if (fd < 0) {
        return -1;
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, address, &addr.sin_addr) != 1 || fcntl(fd, F_SETFL, O_NONBLOCK) != 0) {
        close(fd);
        return -1;
    }
    rc = connect(fd, (struct sockaddr *)&addr, sizeof addr);
    if (rc == 0) {
        return fd;
    }
    if (errno != EINPROGRESS) {
        error = errno;
    } else {
        pfd.fd = fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        do {
            rc = poll(&pfd, 1, 1000);
        } while (rc < 0 && errno == EINTR);
        if (rc > 0 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &error_len) == 0) {
            if (error == 0) {
                return fd;
            }
        } else {
            error = rc == 0 ? ETIMEDOUT : errno;
        }
    }
    close(fd);
    errno = error;
    return -1;
}

static int check_connection(const char *address, int port, int allowed) {
    uint8_t frame[sizeof g_message + 8u];
    size_t frame_len = agorahex_frame_encoded_size(sizeof g_message - 1u);
    int fd = connect_to(address, port);
    int rc = 1;

    if (!allowed) {
        if (fd < 0 && errno == ECONNREFUSED) {
            return 0;
        }
        fprintf(stderr, "bind test: expected connection to %s to be refused\n", address);
    } else if (fd < 0) {
        fprintf(stderr, "bind test: connect to %s failed: %s\n", address, strerror(errno));
    } else {
        g_received = 0;
        agorahex_frame_encode((const uint8_t *)g_message, sizeof g_message - 1u, frame);
        if (agorahex_signal_poll(0) == AGORAHEX_OK && send(fd, frame, frame_len, 0) == (ssize_t)frame_len) {
            for (int i = 0; i < 20 && g_received == 0; i++) {
                if (agorahex_signal_poll(50) != AGORAHEX_OK) {
                    break;
                }
            }
            if (g_received == 1) {
                rc = 0;
            }
        }
        if (rc != 0) {
            fprintf(stderr, "bind test: message from %s was not dispatched\n", address);
        }
    }
    if (fd >= 0) {
        close(fd);
    }
    return rc;
}

static int check_bind(char *bind_address, const char *local_address, int loopback_allowed,
                      int local_allowed) {
    int port = unused_port();
    int rc;

    if (port < 0) {
        fprintf(stderr, "bind test: unable to allocate a local TCP port\n");
        return 1;
    }
    rc = agorahex_signal_start(AGORAHEX_SIGNAL_SERVER_MODE, bind_address, port, receive_cb, NULL);
    if (rc != AGORAHEX_OK) {
        fprintf(stderr, "bind test: server start on %s failed: %d\n",
                bind_address ? bind_address : "NULL", rc);
        return 1;
    }
    rc = check_connection("127.0.0.1", port, loopback_allowed);
    if (rc == 0 && local_address) {
        rc = check_connection(local_address, port, local_allowed);
    }
    agorahex_signal_close();
    return rc;
}

int main(void) {
    char loopback[] = "127.0.0.1";
    char wildcard[] = "0.0.0.0";
    char local[INET_ADDRSTRLEN];
    int found = find_local_ipv4(local, sizeof local);

    if (found < 0) {
        return 1;
    }
    if (!found) {
        fprintf(stderr, "skip: non-loopback bind checks require a local non-loopback IPv4 address\n");
    }
    if (check_bind(loopback, found ? local : NULL, 1, 0) != 0 ||
        check_bind(wildcard, found ? local : NULL, 1, 1) != 0 ||
        check_bind(NULL, found ? local : NULL, 1, 1) != 0 ||
        (found && check_bind(local, local, 0, 1) != 0)) {
        return 1;
    }
    return 0;
}
