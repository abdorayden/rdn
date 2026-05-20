/*
 * path.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Purpose:
 * Native path helpers for Raden.
 *
 * Exports:
 * - joinPath(left, right) -> string
 * - baseName(path) -> string
 * - dirName(path) -> string
 * - extName(path) -> string | null
 * - isAbsolutePath(path) -> boolean
 */

#include "../include/rdn_native.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static bool is_separator(char ch) {
    return ch == '/' || ch == '\\';
}

static bool require_string_arg(RDNApi *api, long index, const char **out_value, const char *message) {
    if (api->type(api, index) != RDN_VALUE_STRING) {
        return api->raise_error(api, message);
    }

    *out_value = api->to_string(api, index);
    if (*out_value == NULL) {
        return api->raise_error(api, message);
    }

    return true;
}

static bool push_owned_string(RDNApi *api, char *value) {
    bool ok = api->push_string(api, value);
    free(value);
    return ok;
}

static bool joinPath(RDNApi *api) {
    const char *left = NULL;
    const char *right = NULL;
    size_t left_len = 0;
    size_t right_len = 0;
    bool need_sep = false;
    char *buffer = NULL;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "joinPath requires 2 params");
    }

    if (!require_string_arg(api, -2, &left, "joinPath requires 2 strings")) {
        return false;
    }
    if (!require_string_arg(api, -1, &right, "joinPath requires 2 strings")) {
        return false;
    }

    left_len = strlen(left);
    right_len = strlen(right);
    need_sep = left_len > 0 && right_len > 0 && !is_separator(left[left_len - 1]) && !is_separator(right[0]);

    buffer = malloc(left_len + right_len + (need_sep ? 2 : 1));
    if (buffer == NULL) {
        return api->raise_error(api, "joinPath failed to allocate buffer");
    }

    memcpy(buffer, left, left_len);
    if (need_sep) {
        buffer[left_len] = '/';
        memcpy(buffer + left_len + 1, right, right_len + 1);
    } else {
        memcpy(buffer + left_len, right, right_len + 1);
    }

    if (!api->pop(api, 2)) {
        free(buffer);
        return false;
    }

    return push_owned_string(api, buffer);
}

static bool baseName(RDNApi *api) {
    const char *path = NULL;
    const char *base = NULL;
    char *buffer = NULL;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "baseName requires 1 param");
    }

    if (!require_string_arg(api, -1, &path, "baseName requires string path")) {
        return false;
    }

    base = path + strlen(path);
    while (base > path && !is_separator(base[-1])) {
        base--;
    }

    buffer = malloc(strlen(base) + 1);
    if (buffer == NULL) {
        return api->raise_error(api, "baseName failed to allocate buffer");
    }
    memcpy(buffer, base, strlen(base) + 1);

    if (!api->pop(api, 1)) {
        free(buffer);
        return false;
    }

    return push_owned_string(api, buffer);
}

static bool dirName(RDNApi *api) {
    const char *path = NULL;
    const char *slash = NULL;
    size_t length = 0;
    char *buffer = NULL;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "dirName requires 1 param");
    }

    if (!require_string_arg(api, -1, &path, "dirName requires string path")) {
        return false;
    }

    slash = strrchr(path, '/');
    {
        const char *backslash = strrchr(path, '\\');
        if (backslash != NULL && (slash == NULL || backslash > slash)) {
            slash = backslash;
        }
    }

    if (slash == NULL) {
        if (!api->pop(api, 1)) {
            return false;
        }
        return api->push_string(api, ".");
    }

    length = (size_t)(slash - path);
    if (length == 0) {
        length = 1;
    }

    buffer = malloc(length + 1);
    if (buffer == NULL) {
        return api->raise_error(api, "dirName failed to allocate buffer");
    }

    memcpy(buffer, path, length);
    buffer[length] = '\0';

    if (!api->pop(api, 1)) {
        free(buffer);
        return false;
    }

    return push_owned_string(api, buffer);
}

static bool extName(RDNApi *api) {
    const char *path = NULL;
    const char *slash = NULL;
    const char *dot = NULL;
    char *buffer = NULL;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "extName requires 1 param");
    }

    if (!require_string_arg(api, -1, &path, "extName requires string path")) {
        return false;
    }

    slash = strrchr(path, '/');
    {
        const char *backslash = strrchr(path, '\\');
        if (backslash != NULL && (slash == NULL || backslash > slash)) {
            slash = backslash;
        }
    }
    dot = strrchr(path, '.');

    if (dot == NULL || (slash != NULL && dot < slash) || dot[1] == '\0') {
        if (!api->pop(api, 1)) {
            return false;
        }
        return api->push_null(api);
    }

    buffer = malloc(strlen(dot + 1) + 1);
    if (buffer == NULL) {
        return api->raise_error(api, "extName failed to allocate buffer");
    }
    memcpy(buffer, dot + 1, strlen(dot + 1) + 1);

    if (!api->pop(api, 1)) {
        free(buffer);
        return false;
    }

    return push_owned_string(api, buffer);
}

static bool isAbsolutePath(RDNApi *api) {
    const char *path = NULL;
    bool absolute = false;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "isAbsolutePath requires 1 param");
    }

    if (!require_string_arg(api, -1, &path, "isAbsolutePath requires string path")) {
        return false;
    }

    if (path[0] == '/' || path[0] == '\\') {
        absolute = true;
    } else if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
               path[1] == ':' && is_separator(path[2])) {
        absolute = true;
    }

    if (!api->pop(api, 1)) {
        return false;
    }

    return api->push_boolean(api, absolute);
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "joinPath", joinPath)) {
        return false;
    }
    if (!module->register_function(module, "baseName", baseName)) {
        return false;
    }
    if (!module->register_function(module, "dirName", dirName)) {
        return false;
    }
    if (!module->register_function(module, "extName", extName)) {
        return false;
    }
    if (!module->register_function(module, "isAbsolutePath", isAbsolutePath)) {
        return false;
    }
    return true;
}
