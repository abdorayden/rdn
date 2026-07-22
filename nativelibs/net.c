/*
 * net.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Purpose:
 * Native network helpers for Raden.
 *
 * Exports:
 * - hostName() -> string
 * - urlEncode(text) -> string
 * - urlDecode(text) -> string
 * - socket_new(protocol) -> integer
 * - socket_connect(id, host, port) -> boolean
 * - socket_bind(id, host, port) -> boolean
 * - socket_listen(id, backlog) -> boolean
 * - socket_accept(id) -> (id, ip, port)
 * - socket_send(id, data) -> integer
 * - socket_recv(id, size) -> string
 * - socket_sendto(id, data, host, port) -> integer
 * - socket_recvfrom(id, size) -> (data, ip, port)
 * - socket_close(id) -> boolean
 * - socket_setnonblock(id, flag) -> boolean
 * - socket_setnodelay(id, flag) -> boolean
 * - socket_setbroadcast(id, flag) -> boolean
 * - socket_getprotocol(id) -> string
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/rdn_native.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef int socklen_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#define PROTO_TCP 0
#define PROTO_UDP 1
#define PROTO_ICMP 2
#define PROTO_RAW 3

#define MAX_SOCKETS 256
#define SOCKET_BUFFER_SIZE 8192

typedef struct {
    SOCKET sock;
    int is_open;
    int protocol;
} RSocket;

static RSocket g_sockets[MAX_SOCKETS];
static int g_socket_count = 0;

static int register_socket(SOCKET sock, int protocol) {
    int i;
    for (i = 0; i < g_socket_count; i++) {
        if (!g_sockets[i].is_open) {
            g_sockets[i].sock = sock;
            g_sockets[i].is_open = 1;
            g_sockets[i].protocol = protocol;
            return i + 1;
        }
    }
    if (g_socket_count >= MAX_SOCKETS) return -1;
    g_sockets[g_socket_count].sock = sock;
    g_sockets[g_socket_count].is_open = 1;
    g_sockets[g_socket_count].protocol = protocol;
    g_socket_count++;
    return g_socket_count;
}

static RSocket *get_socket(int id) {
    RSocket *s = NULL;
    if (id < 1 || id > g_socket_count) return NULL;
    s = &g_sockets[id - 1];
    return s->is_open ? s : NULL;
}

static void close_socket(SOCKET s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}


static int set_nonblocking(SOCKET s, int nb) {
#ifdef _WIN32
    u_long mode = nb ? 1 : 0;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(s, F_GETFL, 0);
    if (flags == -1) return 0;
    flags = nb ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(s, F_SETFL, flags) == 0;
#endif
}

/* ---- host / url helpers ---- */

static bool hostName(RDNApi *api) {
    char buffer[256];

#if defined(_WIN32)
    DWORD size = (DWORD)sizeof(buffer);
    if (!GetComputerNameA(buffer, &size)) {
        return api->raise_error(api, "hostName failed");
    }
#else
    if (gethostname(buffer, sizeof(buffer)) != 0) {
        return api->raise_error(api, "hostName failed");
    }
    buffer[sizeof(buffer) - 1] = '\0';
#endif

    return api->push_string(api, buffer);
}

static bool require_text(RDNApi *api, const char **out_text, const char *context) {
    if (api->stack_size(api) < 1) {
        return api->raise_error(api, context);
    }
    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, context);
    }
    *out_text = api->to_string(api, -1);
    if (*out_text == NULL) {
        return api->raise_error(api, context);
    }
    return true;
}

static bool urlEncode(RDNApi *api) {
    const char *text = NULL;
    size_t in_len = 0;
    size_t out_len = 0;
    char *buffer = NULL;
    size_t i = 0;
    bool ok = false;
    static const char hex[] = "0123456789ABCDEF";

    if (!require_text(api, &text, "urlEncode requires string text")) {
        return false;
    }

    in_len = strlen(text);
    buffer = malloc(in_len * 3 + 1);
    if (buffer == NULL) {
        return api->raise_error(api, "urlEncode failed to allocate buffer");
    }

    for (i = 0; i < in_len; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            buffer[out_len++] = (char)ch;
        } else {
            buffer[out_len++] = '%';
            buffer[out_len++] = hex[(ch >> 4) & 0x0F];
            buffer[out_len++] = hex[ch & 0x0F];
        }
    }
    buffer[out_len] = '\0';

    if (!api->pop(api, 1)) {
        free(buffer);
        return false;
    }

    ok = api->push_string(api, buffer);
    free(buffer);
    return ok;
}

static int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

static bool urlDecode(RDNApi *api) {
    const char *text = NULL;
    size_t in_len = 0;
    char *buffer = NULL;
    size_t out_len = 0;
    size_t i = 0;
    bool ok = false;

    if (!require_text(api, &text, "urlDecode requires string text")) {
        return false;
    }

    in_len = strlen(text);
    buffer = malloc(in_len + 1);
    if (buffer == NULL) {
        return api->raise_error(api, "urlDecode failed to allocate buffer");
    }

    for (i = 0; i < in_len; i++) {
        if (text[i] == '%' && i + 2 < in_len) {
            int hi = hex_value(text[i + 1]);
            int lo = hex_value(text[i + 2]);
            if (hi >= 0 && lo >= 0) {
                buffer[out_len++] = (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        buffer[out_len++] = text[i];
    }
    buffer[out_len] = '\0';

    if (!api->pop(api, 1)) {
        free(buffer);
        return false;
    }

    ok = api->push_string(api, buffer);
    free(buffer);
    return ok;
}

/* ---- socket functions ---- */

static int resolve_proto(const char *name) {
    if (strcmp(name, "tcp") == 0) return PROTO_TCP;
    if (strcmp(name, "udp") == 0) return PROTO_UDP;
    if (strcmp(name, "icmp") == 0) return PROTO_ICMP;
    if (strcmp(name, "raw") == 0) return PROTO_RAW;
    return -1;
}

static bool resolve_host(const char *host, struct sockaddr_in *addr) {
    if (inet_pton(AF_INET, host, &addr->sin_addr) == 1) {
        return 1;
    }
#ifdef _WIN32
    {
        struct addrinfo hints, *result = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host, NULL, &hints, &result) == 0 && result) {
            memcpy(&addr->sin_addr, &result->ai_addr, sizeof(struct sockaddr_in));
            freeaddrinfo(result);
            return 1;
        }
        if (result) freeaddrinfo(result);
    }
#else
    {
        struct hostent *he = gethostbyname(host);
        if (he) {
            memcpy(&addr->sin_addr, he->h_addr_list[0], he->h_length);
            return 1;
        }
    }
#endif
    return 0;
}

static bool socket_new(RDNApi *api) {
    const char *proto_str = NULL;
    int protocol, sock_type, sock_proto;
    SOCKET sock;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "socket_new requires a protocol string");
    }
    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "socket_new requires a protocol string");
    }
    proto_str = api->to_string(api, -1);
    if (proto_str == NULL) {
        return api->raise_error(api, "socket_new requires a protocol string");
    }

    protocol = resolve_proto(proto_str);
    if (protocol < 0) {
        return api->raise_error(api, "socket_new: invalid protocol (use tcp, udp, icmp, or raw)");
    }

    switch (protocol) {
        case PROTO_TCP:  sock_type = SOCK_STREAM; sock_proto = IPPROTO_TCP;  break;
        case PROTO_UDP:  sock_type = SOCK_DGRAM;  sock_proto = IPPROTO_UDP;  break;
        case PROTO_ICMP: sock_type = SOCK_RAW;    sock_proto = IPPROTO_ICMP; break;
        case PROTO_RAW:  sock_type = SOCK_RAW;    sock_proto = IPPROTO_RAW;  break;
        default:         sock_type = SOCK_STREAM; sock_proto = IPPROTO_TCP;  break;
    }

    sock = socket(AF_INET, sock_type, sock_proto);
    if (sock == INVALID_SOCKET) {
        return api->raise_error(api, "socket_new: socket() failed");
    }

    if (!api->pop(api, 1)) {
        close_socket(sock);
        return false;
    }

    {
        int id = register_socket(sock, protocol);
        if (id < 0) {
            close_socket(sock);
            return api->raise_error(api, "socket_new: too many open sockets");
        }
        return api->push_integer(api, (long)id);
    }
}

static bool socket_connect(RDNApi *api) {
    long id = 0, port = 0;
    const char *host = NULL;
    RSocket *rs = NULL;
    struct sockaddr_in addr;

    if (api->stack_size(api) < 3) {
        return api->raise_error(api, "socket_connect requires (id, host, port)");
    }

    if (!api->to_integer(api, -3, &id)) {
        return api->raise_error(api, "socket_connect: first argument must be socket id");
    }
    if (api->type(api, -2) != RDN_VALUE_STRING) {
        return api->raise_error(api, "socket_connect: second argument must be host string");
    }
    host = api->to_string(api, -2);
    if (!api->to_integer(api, -1, &port)) {
        return api->raise_error(api, "socket_connect: third argument must be port integer");
    }

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_connect: invalid or closed socket");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);

    if (!resolve_host(host, &addr)) {
        return api->raise_error(api, "socket_connect: host not found");
    }

    if (connect(rs->sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        return api->raise_error(api, "socket_connect: connect failed");
    }

    if (!api->pop(api, 3)) {
        return false;
    }
    return api->push_boolean(api, true);
}

static bool socket_bind(RDNApi *api) {
    long id = 0, port = 0;
    const char *host = NULL;
    RSocket *rs = NULL;
    struct sockaddr_in addr;
    int reuse = 1;

    if (api->stack_size(api) < 3) {
        return api->raise_error(api, "socket_bind requires (id, host, port)");
    }

    if (!api->to_integer(api, -3, &id)) {
        return api->raise_error(api, "socket_bind: first argument must be socket id");
    }
    if (api->type(api, -2) != RDN_VALUE_STRING) {
        return api->raise_error(api, "socket_bind: second argument must be host string");
    }
    host = api->to_string(api, -2);
    if (!api->to_integer(api, -1, &port)) {
        return api->raise_error(api, "socket_bind: third argument must be port integer");
    }

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_bind: invalid or closed socket");
    }

    setsockopt(rs->sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);

    if (strcmp(host, "*") == 0 || strcmp(host, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (!resolve_host(host, &addr)) {
            return api->raise_error(api, "socket_bind: invalid address");
        }
    }

    if (bind(rs->sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        return api->raise_error(api, "socket_bind: bind failed");
    }

    if (!api->pop(api, 3)) {
        return false;
    }
    return api->push_boolean(api, true);
}

static bool socket_listen(RDNApi *api) {
    long id = 0, backlog = 5;
    RSocket *rs = NULL;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "socket_listen requires (id, backlog)");
    }

    if (!api->to_integer(api, -2, &id)) {
        return api->raise_error(api, "socket_listen: first argument must be socket id");
    }
    if (!api->to_integer(api, -1, &backlog)) {
        return api->raise_error(api, "socket_listen: second argument must be backlog integer");
    }

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_listen: invalid or closed socket");
    }

    if (listen(rs->sock, (int)backlog) == SOCKET_ERROR) {
        return api->raise_error(api, "socket_listen: listen failed");
    }

    if (!api->pop(api, 2)) {
        return false;
    }
    return api->push_boolean(api, true);
}

static bool socket_accept(RDNApi *api) {
    long id = 0;
    RSocket *rs = NULL;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    SOCKET client_sock;
    int client_id;
    char ip[INET_ADDRSTRLEN];

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "socket_accept requires socket id");
    }

    if (!api->to_integer(api, -1, &id)) {
        return api->raise_error(api, "socket_accept: argument must be socket id");
    }

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_accept: invalid or closed socket");
    }

    client_sock = accept(rs->sock, (struct sockaddr *)&client_addr, &addr_len);
    if (client_sock == INVALID_SOCKET) {
        return api->raise_error(api, "socket_accept: accept failed");
    }

    client_id = register_socket(client_sock, PROTO_TCP);
    if (client_id < 0) {
        close_socket(client_sock);
        return api->raise_error(api, "socket_accept: too many open sockets");
    }

    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));

    if (!api->pop(api, 1)) {
        return false;
    }

    api->push_integer(api, (long)client_id);
    api->push_string(api, ip);
    api->push_integer(api, (long)ntohs(client_addr.sin_port));
    return true;
}

static bool socket_send(RDNApi *api) {
    long id = 0;
    const char *data = NULL;
    size_t data_len = 0;
    RSocket *rs = NULL;
    int sent;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "socket_send requires (id, data)");
    }

    if (!api->to_integer(api, -2, &id)) {
        return api->raise_error(api, "socket_send: first argument must be socket id");
    }
    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "socket_send: second argument must be data string");
    }
    data = api->to_string(api, -1);
    if (data == NULL) {
        return api->raise_error(api, "socket_send: second argument must be data string");
    }
    data_len = strlen(data);

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_send: invalid or closed socket");
    }

    sent = send(rs->sock, data, (int)data_len, 0);
    if (sent == SOCKET_ERROR) {
        return api->raise_error(api, "socket_send: send failed");
    }

    if (!api->pop(api, 2)) {
        return false;
    }
    return api->push_integer(api, (long)sent);
}

static bool socket_recv(RDNApi *api) {
    long id = 0, size = SOCKET_BUFFER_SIZE;
    RSocket *rs = NULL;
    char buffer[SOCKET_BUFFER_SIZE];
    int received;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "socket_recv requires (id [, size])");
    }

    if (!api->to_integer(api, -1, &id)) {
        return api->raise_error(api, "socket_recv: first argument must be socket id");
    }
    if (api->stack_size(api) >= 2) {
        api->to_integer(api, -2, &size);
    }
    if (size <= 0 || size > SOCKET_BUFFER_SIZE) {
        size = SOCKET_BUFFER_SIZE;
    }

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_recv: invalid or closed socket");
    }

    received = recv(rs->sock, buffer, (int)size, 0);
    if (received == 0) {
        return api->raise_error(api, "socket_recv: connection closed");
    }
    if (received == SOCKET_ERROR) {
        return api->raise_error(api, "socket_recv: recv failed");
    }

    {
        char *copy = malloc(received + 1);
        if (copy == NULL) {
            return api->raise_error(api, "socket_recv: allocation failed");
        }
        memcpy(copy, buffer, received);
        copy[received] = '\0';

        if (!api->pop(api, api->stack_size(api) >= 2 ? 2 : 1)) {
            free(copy);
            return false;
        }

        {
            bool ok = api->push_string(api, copy);
            free(copy);
            return ok;
        }
    }
}

static bool socket_sendto(RDNApi *api) {
    long id = 0, port = 0;
    const char *data = NULL, *host = NULL;
    size_t data_len = 0;
    RSocket *rs = NULL;
    struct sockaddr_in addr;
    int sent;

    if (api->stack_size(api) < 4) {
        return api->raise_error(api, "socket_sendto requires (id, data, host, port)");
    }

    if (!api->to_integer(api, -4, &id)) {
        return api->raise_error(api, "socket_sendto: first argument must be socket id");
    }
    if (api->type(api, -3) != RDN_VALUE_STRING) {
        return api->raise_error(api, "socket_sendto: second argument must be data string");
    }
    data = api->to_string(api, -3);
    if (data == NULL) {
        return api->raise_error(api, "socket_sendto: second argument must be data string");
    }
    data_len = strlen(data);
    if (api->type(api, -2) != RDN_VALUE_STRING) {
        return api->raise_error(api, "socket_sendto: third argument must be host string");
    }
    host = api->to_string(api, -2);
    if (!api->to_integer(api, -1, &port)) {
        return api->raise_error(api, "socket_sendto: fourth argument must be port integer");
    }

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_sendto: invalid or closed socket");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);

    if (!resolve_host(host, &addr)) {
        return api->raise_error(api, "socket_sendto: host not found");
    }

    sent = sendto(rs->sock, data, (int)data_len, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (sent == SOCKET_ERROR) {
        return api->raise_error(api, "socket_sendto: sendto failed");
    }

    if (!api->pop(api, 4)) {
        return false;
    }
    return api->push_integer(api, (long)sent);
}

static bool socket_recvfrom(RDNApi *api) {
    long id = 0, size = SOCKET_BUFFER_SIZE;
    RSocket *rs = NULL;
    char buffer[SOCKET_BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    int received;
    char ip[INET_ADDRSTRLEN];

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "socket_recvfrom requires (id [, size])");
    }

    if (!api->to_integer(api, -1, &id)) {
        return api->raise_error(api, "socket_recvfrom: first argument must be socket id");
    }
    if (api->stack_size(api) >= 2) {
        api->to_integer(api, -2, &size);
    }
    if (size <= 0 || size > SOCKET_BUFFER_SIZE) {
        size = SOCKET_BUFFER_SIZE;
    }

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_recvfrom: invalid or closed socket");
    }

    received = recvfrom(rs->sock, buffer, (int)size, 0, (struct sockaddr *)&from_addr, &from_len);
    if (received == SOCKET_ERROR) {
        return api->raise_error(api, "socket_recvfrom: recvfrom failed");
    }

    inet_ntop(AF_INET, &from_addr.sin_addr, ip, sizeof(ip));

    {
        char *copy = malloc(received + 1);
        if (copy == NULL) {
            return api->raise_error(api, "socket_recvfrom: allocation failed");
        }
        memcpy(copy, buffer, received);
        copy[received] = '\0';

        if (!api->pop(api, api->stack_size(api) >= 2 ? 2 : 1)) {
            free(copy);
            return false;
        }

        api->push_string(api, copy);
        free(copy);
        api->push_string(api, ip);
        api->push_integer(api, (long)ntohs(from_addr.sin_port));
        return true;
    }
}

static bool socket_close(RDNApi *api) {
    long id = 0;
    RSocket *rs = NULL;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "socket_close requires socket id");
    }

    if (!api->to_integer(api, -1, &id)) {
        return api->raise_error(api, "socket_close: argument must be socket id");
    }

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_close: invalid or closed socket");
    }

    close_socket(rs->sock);
    rs->is_open = 0;

    if (!api->pop(api, 1)) {
        return false;
    }
    return api->push_boolean(api, true);
}

static bool socket_setnonblock(RDNApi *api) {
    long id = 0;
    bool flag = false;
    RSocket *rs = NULL;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "socket_setnonblock requires (id, flag)");
    }

    if (!api->to_integer(api, -2, &id)) {
        return api->raise_error(api, "socket_setnonblock: first argument must be socket id");
    }
    if (!api->to_boolean(api, -1, &flag)) {
        return api->raise_error(api, "socket_setnonblock: second argument must be boolean");
    }

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_setnonblock: invalid or closed socket");
    }

    if (!set_nonblocking(rs->sock, flag ? 1 : 0)) {
        return api->raise_error(api, "socket_setnonblock: failed");
    }

    if (!api->pop(api, 2)) {
        return false;
    }
    return api->push_boolean(api, true);
}

static bool socket_setnodelay(RDNApi *api) {
    long id = 0;
    bool flag = false;
    RSocket *rs = NULL;
    int val;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "socket_setnodelay requires (id, flag)");
    }

    if (!api->to_integer(api, -2, &id)) {
        return api->raise_error(api, "socket_setnodelay: first argument must be socket id");
    }
    if (!api->to_boolean(api, -1, &flag)) {
        return api->raise_error(api, "socket_setnodelay: second argument must be boolean");
    }

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_setnodelay: invalid or closed socket");
    }

    if (rs->protocol != PROTO_TCP) {
        return api->raise_error(api, "socket_setnodelay: only works with TCP sockets");
    }

    val = flag ? 1 : 0;
    if (setsockopt(rs->sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&val, sizeof(val)) == SOCKET_ERROR) {
        return api->raise_error(api, "socket_setnodelay: setsockopt failed");
    }

    if (!api->pop(api, 2)) {
        return false;
    }
    return api->push_boolean(api, true);
}

static bool socket_setbroadcast(RDNApi *api) {
    long id = 0;
    bool flag = false;
    RSocket *rs = NULL;
    int val;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "socket_setbroadcast requires (id, flag)");
    }

    if (!api->to_integer(api, -2, &id)) {
        return api->raise_error(api, "socket_setbroadcast: first argument must be socket id");
    }
    if (!api->to_boolean(api, -1, &flag)) {
        return api->raise_error(api, "socket_setbroadcast: second argument must be boolean");
    }

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_setbroadcast: invalid or closed socket");
    }

    val = flag ? 1 : 0;
    if (setsockopt(rs->sock, SOL_SOCKET, SO_BROADCAST, (const char *)&val, sizeof(val)) == SOCKET_ERROR) {
        return api->raise_error(api, "socket_setbroadcast: setsockopt failed");
    }

    if (!api->pop(api, 2)) {
        return false;
    }
    return api->push_boolean(api, true);
}

static bool socket_getprotocol(RDNApi *api) {
    long id = 0;
    RSocket *rs = NULL;
    const char *proto_str = NULL;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "socket_getprotocol requires socket id");
    }

    if (!api->to_integer(api, -1, &id)) {
        return api->raise_error(api, "socket_getprotocol: argument must be socket id");
    }

    rs = get_socket((int)id);
    if (rs == NULL) {
        return api->raise_error(api, "socket_getprotocol: invalid or closed socket");
    }

    switch (rs->protocol) {
        case PROTO_TCP:  proto_str = "tcp";  break;
        case PROTO_UDP:  proto_str = "udp";  break;
        case PROTO_ICMP: proto_str = "icmp"; break;
        case PROTO_RAW:  proto_str = "raw";  break;
        default:         proto_str = "unknown"; break;
    }

    if (!api->pop(api, 1)) {
        return false;
    }
    return api->push_string(api, proto_str);
}

/* ---- module init ---- */

bool rdn_module_init(RDNModule *module) {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return module->set_error(module, "WSAStartup failed");
    }
#endif

    if (!module->register_function(module, "hostName", hostName)) {
        return false;
    }
    if (!module->register_function(module, "urlEncode", urlEncode)) {
        return false;
    }
    if (!module->register_function(module, "urlDecode", urlDecode)) {
        return false;
    }
    if (!module->register_function(module, "socket_new", socket_new)) {
        return false;
    }
    if (!module->register_function(module, "socket_connect", socket_connect)) {
        return false;
    }
    if (!module->register_function(module, "socket_bind", socket_bind)) {
        return false;
    }
    if (!module->register_function(module, "socket_listen", socket_listen)) {
        return false;
    }
    if (!module->register_function(module, "socket_accept", socket_accept)) {
        return false;
    }
    if (!module->register_function(module, "socket_send", socket_send)) {
        return false;
    }
    if (!module->register_function(module, "socket_recv", socket_recv)) {
        return false;
    }
    if (!module->register_function(module, "socket_sendto", socket_sendto)) {
        return false;
    }
    if (!module->register_function(module, "socket_recvfrom", socket_recvfrom)) {
        return false;
    }
    if (!module->register_function(module, "socket_close", socket_close)) {
        return false;
    }
    if (!module->register_function(module, "socket_setnonblock", socket_setnonblock)) {
        return false;
    }
    if (!module->register_function(module, "socket_setnodelay", socket_setnodelay)) {
        return false;
    }
    if (!module->register_function(module, "socket_setbroadcast", socket_setbroadcast)) {
        return false;
    }
    if (!module->register_function(module, "socket_getprotocol", socket_getprotocol)) {
        return false;
    }
    return true;
}
