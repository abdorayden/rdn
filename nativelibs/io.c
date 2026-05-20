/*
 * io.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Purpose:
 * Native stdin helpers for Raden.
 *
 * Exports:
 * - readLine() -> string | null
 * - readAllInput() -> string
 *
 * Notes:
 * - readLine strips a trailing '\n' and optional preceding '\r'.
 * - readLine returns null when stdin is already at EOF.
 * - readAllInput returns the remaining stdin content, possibly empty.
 */

#define _POSIX_C_SOURCE 200809L

#include "../include/rdn_native.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool append_char(char **buffer, size_t *length, size_t *capacity, char ch) {
    char *grown = NULL;

    if (*length + 1 >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 64 : (*capacity * 2);
        grown = realloc(*buffer, new_capacity);
        if (grown == NULL) {
            free(*buffer);
            *buffer = NULL;
            *length = 0;
            *capacity = 0;
            return false;
        }
        *buffer = grown;
        *capacity = new_capacity;
    }

    (*buffer)[(*length)++] = ch;
    (*buffer)[*length] = '\0';
    return true;
}

static bool readLine(RDNApi *api) {
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    int ch = 0;

    if (api->stack_size(api) != 0) {
        return api->raise_error(api, "readLine requires 0 params");
    }

    while ((ch = fgetc(stdin)) != EOF) {
        if (ch == '\n') {
            break;
        }
        if (!append_char(&buffer, &length, &capacity, (char)ch)) {
            return api->raise_error(api, "readLine failed to allocate buffer");
        }
    }

    if (ch == EOF && length == 0) {
        free(buffer);
        return api->push_null(api);
    }

    if (length > 0 && buffer[length - 1] == '\r') {
        buffer[length - 1] = '\0';
    }

    if (buffer == NULL) {
        buffer = malloc(1);
        if (buffer == NULL) {
            return api->raise_error(api, "readLine failed to allocate buffer");
        }
        buffer[0] = '\0';
    }

    if (!api->push_string(api, buffer)) {
        free(buffer);
        return false;
    }

    free(buffer);
    return true;
}

static bool readAllInput(RDNApi *api) {
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    int ch = 0;

    if (api->stack_size(api) != 0) {
        return api->raise_error(api, "readAllInput requires 0 params");
    }

    while ((ch = fgetc(stdin)) != EOF) {
        if (!append_char(&buffer, &length, &capacity, (char)ch)) {
            return api->raise_error(api, "readAllInput failed to allocate buffer");
        }
    }

    if (buffer == NULL) {
        buffer = malloc(1);
        if (buffer == NULL) {
            return api->raise_error(api, "readAllInput failed to allocate buffer");
        }
        buffer[0] = '\0';
    }

    if (!api->push_string(api, buffer)) {
        free(buffer);
        return false;
    }

    free(buffer);
    return true;
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "readLine", readLine)) {
        return false;
    }
    if (!module->register_function(module, "readAllInput", readAllInput)) {
        return false;
    }
    return true;
}
