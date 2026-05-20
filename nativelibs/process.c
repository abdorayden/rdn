/*
 * process.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Purpose:
 * Native process helpers for Raden.
 *
 * Exports:
 * - execStatus(command) -> integer
 * - execOutput(command) -> string
 */

#define _POSIX_C_SOURCE 200809L

#include "../include/rdn_native.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#define RDN_POPEN _popen
#define RDN_PCLOSE _pclose
#else
#include <sys/wait.h>
#define RDN_POPEN popen
#define RDN_PCLOSE pclose
#endif

static bool require_command(RDNApi *api, const char **out_command, const char *context) {
    if (api->stack_size(api) < 1) {
        return api->raise_error(api, context);
    }
    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, context);
    }
    *out_command = api->to_string(api, -1);
    if (*out_command == NULL) {
        return api->raise_error(api, context);
    }
    return true;
}

static bool execStatus(RDNApi *api) {
    const char *command = NULL;
    int status = 0;

    if (!require_command(api, &command, "execStatus requires string command")) {
        return false;
    }

    status = system(command);

    if (!api->pop(api, 1)) {
        return false;
    }

#if defined(_WIN32)
    return api->push_integer(api, (long)status);
#else
    if (status == -1) {
        return api->raise_error(api, "execStatus failed");
    }
    if (WIFEXITED(status)) {
        return api->push_integer(api, (long)WEXITSTATUS(status));
    }
    return api->push_integer(api, (long)status);
#endif
}

static bool execOutput(RDNApi *api) {
    const char *command = NULL;
    FILE *pipe = NULL;
    char chunk[256];
    char *buffer = NULL;
    size_t length = 0;
    size_t chunk_len = 0;
    int close_status = 0;
    bool ok = false;

    if (!require_command(api, &command, "execOutput requires string command")) {
        return false;
    }

    pipe = RDN_POPEN(command, "r");
    if (pipe == NULL) {
        return api->raise_error(api, "execOutput failed to start command");
    }

    buffer = malloc(1);
    if (buffer == NULL) {
        RDN_PCLOSE(pipe);
        return api->raise_error(api, "execOutput failed to allocate buffer");
    }
    buffer[0] = '\0';

    while (fgets(chunk, sizeof(chunk), pipe) != NULL) {
        char *grown = NULL;
        chunk_len = strlen(chunk);
        grown = realloc(buffer, length + chunk_len + 1);
        if (grown == NULL) {
            free(buffer);
            RDN_PCLOSE(pipe);
            return api->raise_error(api, "execOutput failed to grow buffer");
        }
        buffer = grown;
        memcpy(buffer + length, chunk, chunk_len + 1);
        length += chunk_len;
    }

    close_status = RDN_PCLOSE(pipe);
    (void)close_status;

    if (!api->pop(api, 1)) {
        free(buffer);
        return false;
    }

    ok = api->push_string(api, buffer);
    free(buffer);
    return ok;
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "execStatus", execStatus)) {
        return false;
    }
    if (!module->register_function(module, "execOutput", execOutput)) {
        return false;
    }
    return true;
}
