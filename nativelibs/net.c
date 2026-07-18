/*
 * net.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Purpose:
 * Native network-adjacent helpers for Raden.
 *
 * Exports:
 * - hostName() -> string
 * - urlEncode(text) -> string
 * - urlDecode(text) -> string
 */

#include <errno.h>
#include <stdbool.h>
#define _POSIX_C_SOURCE 200809L

#include "../include/rdn_native.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#endif

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

static bool suck(RDNApi* api) {
#ifndef _WIN32
    // linux shit
    long domain;
    long type;
    long protocol;

    if (api->stack_size(api) < 3) {
        return api->raise_error(api , "socket requires 3 parameters");
    }

    if (!api->to_integer(api , -1 , &protocol)) {
        return api->raise_error(api , "socket requires integer value at top of the stack and it's protocol");
    }

    if (!api->to_integer(api , -2 , &type)) {
        return api->raise_error(api , "socket requires integer value at under top of the stack and it's type");
    }

    if (!api->to_integer(api , -3 , &domain)) {
        return api->raise_error(api , "socket requires integer value at second under top of the stack and it's domain");
    }

    int sockfd = socket(domain , type , protocol);
    if (sockfd == -1) {
        return api->raise_error(api, strerror(errno));
    }

    api->push_integer(api , (long)sockfd);

#else

    // TODO: later

#endif /* ifndef _WIN32 */
    return true;
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "hostName", hostName)) {
        return false;
    }
    if (!module->register_function(module, "urlEncode", urlEncode)) {
        return false;
    }
    if (!module->register_function(module, "urlDecode", urlDecode)) {
        return false;
    }
    if (!module->register_function(module, "suckit", suck)) {
        return false;
    }
    return true;
}
