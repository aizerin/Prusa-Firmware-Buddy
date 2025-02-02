/**
 ******************************************************************************
 * File Name       : net_sockets.c.h
 * Description     : TCP/IP or UDP/IP networking functions implementation based
                    on LwIP API see the file "mbedTLS/library/net_socket_template.c"
                    for the standard implmentation
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */

#include <string.h>
#include <stdint.h>
#include <sys/socket.h>

#include "mbedtls/net_sockets.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

/* FIXME:
 * 2) This source code is generated by cubeMX and the license terms must be clarified.
 *
 */

void mbedtls_net_init(mbedtls_net_context *ctx) {
    ctx->fd = -1;
    ctx->timeout_s = 0;
}

/*
 * Initiate a TCP connection with host:port and the given protocol
 */
int mbedtls_net_connect(mbedtls_net_context *ctx, const char *host, const char *port, int proto) {
    int ret;
    struct addrinfo hints;
    struct addrinfo *addr_list;
    struct addrinfo *cur;
    /* Do name resolution with both IPv6 and IPv4 */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = proto == MBEDTLS_NET_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = proto == MBEDTLS_NET_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP;

    if (lwip_getaddrinfo(host, port, &hints, &addr_list) != 0)
        return (MBEDTLS_ERR_NET_UNKNOWN_HOST);

    /* Try the sockaddrs until a connection succeeds */
    ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
    for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
        ctx->fd = socket(cur->ai_family, cur->ai_socktype,
            cur->ai_protocol);

        if (ctx->fd < 0) {
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        if (ctx->timeout_s != 0) {
            const struct timeval timeout = { ctx->timeout_s, 0 };
            if (setsockopt(ctx->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
                goto ERR;
            }
            if (setsockopt(ctx->fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
                goto ERR;
            }
        }

        if (connect(ctx->fd, cur->ai_addr, cur->ai_addrlen) == 0) {
            ret = 0;
            break;
        }

    ERR:
        close(ctx->fd);

        ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
    }

    lwip_freeaddrinfo(addr_list);

    return (ret);
}

/*
 * Check if the requested operation would be blocking on a non-blocking socket
 * and thus 'failed' with a negative return value.
 *
 * Note: on a blocking socket this function always returns 0!
 */
static int net_would_block(const mbedtls_net_context *ctx) {
    int err = errno;

    /*
     * Never return 'WOULD BLOCK' on a non-blocking socket
     */
    if (lwip_fcntl(ctx->fd, F_GETFL, O_NONBLOCK) != O_NONBLOCK) {
        errno = err;
        return (0);
    }

    switch (errno = err) {
#if defined EAGAIN
    case EAGAIN:
#endif
#if defined EWOULDBLOCK && EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
        return (1);
    }
    return (0);
}

/*
 * Read at most 'len' characters
 */
int mbedtls_net_recv(void *ctx, unsigned char *buf, size_t len) {
    int ret;
    int fd = ((mbedtls_net_context *)ctx)->fd;

    if (fd < 0)
        return (MBEDTLS_ERR_NET_INVALID_CONTEXT);

    ret = (int)read(fd, buf, len);

    if (ret < 0) {
        if (net_would_block(ctx) != 0)
            return (MBEDTLS_ERR_SSL_WANT_READ);

        if (errno == EPIPE || errno == ECONNRESET)
            return (MBEDTLS_ERR_NET_CONN_RESET);

        if (errno == EINTR)
            return (MBEDTLS_ERR_SSL_WANT_READ);

        return (MBEDTLS_ERR_NET_RECV_FAILED);
    }

    return (ret);
}

/*
 * Write at most 'len' characters
 */
int mbedtls_net_send(void *ctx, const unsigned char *buf, size_t len) {
    int ret;
    int fd = ((mbedtls_net_context *)ctx)->fd;

    if (fd < 0)
        return (MBEDTLS_ERR_NET_INVALID_CONTEXT);

    ret = (int)write(fd, buf, len);

    if (ret < 0) {
        if (net_would_block(ctx) != 0)
            return (MBEDTLS_ERR_SSL_WANT_WRITE);

        if (errno == EPIPE || errno == ECONNRESET)
            return (MBEDTLS_ERR_NET_CONN_RESET);

        if (errno == EINTR)
            return (MBEDTLS_ERR_SSL_WANT_WRITE);
        return (MBEDTLS_ERR_NET_SEND_FAILED);
    }

    return (ret);
}

/*
 * Gracefully close the connection
 */
void mbedtls_net_free(mbedtls_net_context *ctx) {
    if (ctx->fd == -1)
        return;
    lwip_shutdown(ctx->fd, 2);
    close(ctx->fd);

    ctx->fd = -1;
}
