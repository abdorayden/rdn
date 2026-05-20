/*
 * strings.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Purpose:
 * Native string helpers inspired by Go's strings package.
 *
 * Exports:
 * - strContains(text, part) -> boolean
 * - strHasPrefix(text, prefix) -> boolean
 * - strHasSuffix(text, suffix) -> boolean
 * - strTrimSpace(text) -> string
 * - strSplit(text, sep) -> list of strings
 * - strReplaceAll(text, old, new) -> string
 */

#define _POSIX_C_SOURCE 200809L

#include "../include/rdn_native.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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

static bool strContains(RDNApi *api) {
    const char *text = NULL;
    const char *part = NULL;
    bool found = false;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "strContains requires 2 params");
    }

    if (!require_string_arg(api, -2, &text, "strContains requires 2 strings")) {
        return false;
    }
    if (!require_string_arg(api, -1, &part, "strContains requires 2 strings")) {
        return false;
    }

    found = strstr(text, part) != NULL;

    if (!api->pop(api, 2)) {
        return false;
    }

    return api->push_boolean(api, found);
}

static bool strHasPrefix(RDNApi *api) {
    const char *text = NULL;
    const char *prefix = NULL;
    size_t prefix_len = 0;
    bool found = false;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "strHasPrefix requires 2 params");
    }

    if (!require_string_arg(api, -2, &text, "strHasPrefix requires 2 strings")) {
        return false;
    }
    if (!require_string_arg(api, -1, &prefix, "strHasPrefix requires 2 strings")) {
        return false;
    }

    prefix_len = strlen(prefix);

    found = strncmp(text, prefix, prefix_len) == 0;

    if (!api->pop(api, 2)) {
        return false;
    }

    return api->push_boolean(api, found);
}

static bool strHasSuffix(RDNApi *api) {
    const char *text = NULL;
    const char *suffix = NULL;
    size_t text_len = 0;
    size_t suffix_len = 0;
    bool found = false;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "strHasSuffix requires 2 params");
    }

    if (!require_string_arg(api, -2, &text, "strHasSuffix requires 2 strings")) {
        return false;
    }
    if (!require_string_arg(api, -1, &suffix, "strHasSuffix requires 2 strings")) {
        return false;
    }

    text_len = strlen(text);
    suffix_len = strlen(suffix);

    if (suffix_len > text_len) {
        found = false;
    } else {
        found = strcmp(text + text_len - suffix_len, suffix) == 0;
    }

    if (!api->pop(api, 2)) {
        return false;
    }

    return api->push_boolean(api, found);
}

static bool strTrimSpace(RDNApi *api) {
    const char *text = NULL;
    const char *start = NULL;
    const char *end = NULL;
    size_t length = 0;
    char *buffer = NULL;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "strTrimSpace requires 1 param");
    }

    if (!require_string_arg(api, -1, &text, "strTrimSpace requires string text")) {
        return false;
    }

    start = text;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    end = text + strlen(text);
    while (end > start && isspace((unsigned char)end[-1])) {
        end--;
    }

    length = (size_t)(end - start);
    buffer = malloc(length + 1);
    if (buffer == NULL) {
        return api->raise_error(api, "strTrimSpace failed to allocate buffer");
    }

    memcpy(buffer, start, length);
    buffer[length] = '\0';

    if (!api->pop(api, 1)) {
        free(buffer);
        return false;
    }

    return push_owned_string(api, buffer);
}

static bool strSplit(RDNApi *api) {
    const char *text = NULL;
    const char *sep = NULL;
    char *owned_text = NULL;
    char *owned_sep = NULL;
    const char *cursor = NULL;
    const char *next = NULL;
    char *part = NULL;
    size_t part_len = 0;
    size_t sep_len = 0;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "strSplit requires 2 params");
    }

    if (!require_string_arg(api, -2, &text, "strSplit requires 2 strings")) {
        return false;
    }
    if (!require_string_arg(api, -1, &sep, "strSplit requires 2 strings")) {
        return false;
    }

    sep_len = strlen(sep);
    if (sep_len == 0) {
        return api->raise_error(api, "strSplit requires non-empty separator");
    }

    owned_text = strdup(text);
    owned_sep = strdup(sep);
    if (owned_text == NULL || owned_sep == NULL) {
        free(owned_text);
        free(owned_sep);
        return api->raise_error(api, "strSplit failed to allocate input copy");
    }

    if (!api->pop(api, 2)) {
        free(owned_text);
        free(owned_sep);
        return false;
    }

    if (!api->push_list(api)) {
        free(owned_text);
        free(owned_sep);
        return false;
    }

    sep_len = strlen(owned_sep);
    cursor = owned_text;
    while (true) {
        next = strstr(cursor, owned_sep);
        part_len = next == NULL ? strlen(cursor) : (size_t)(next - cursor);
        part = malloc(part_len + 1);
        if (part == NULL) {
            free(owned_text);
            free(owned_sep);
            return api->raise_error(api, "strSplit failed to allocate part");
        }

        memcpy(part, cursor, part_len);
        part[part_len] = '\0';

        if (!api->push_string(api, part)) {
            free(part);
            free(owned_text);
            free(owned_sep);
            return false;
        }
        free(part);

        if (!api->list_append(api, -2, -1)) {
            free(owned_text);
            free(owned_sep);
            return false;
        }
        if (!api->pop(api, 1)) {
            free(owned_text);
            free(owned_sep);
            return false;
        }

        if (next == NULL) {
            break;
        }

        cursor = next + sep_len;
    }

    free(owned_text);
    free(owned_sep);
    return true;
}

static bool strReplaceAll(RDNApi *api) {
    const char *text = NULL;
    const char *old = NULL;
    const char *new_text = NULL;
    const char *cursor = NULL;
    const char *match = NULL;
    char *buffer = NULL;
    size_t text_len = 0;
    size_t old_len = 0;
    size_t new_len = 0;
    size_t match_count = 0;
    size_t result_len = 0;

    if (api->stack_size(api) < 3) {
        return api->raise_error(api, "strReplaceAll requires 3 params");
    }

    if (!require_string_arg(api, -3, &text, "strReplaceAll requires 3 strings")) {
        return false;
    }
    if (!require_string_arg(api, -2, &old, "strReplaceAll requires 3 strings")) {
        return false;
    }
    if (!require_string_arg(api, -1, &new_text, "strReplaceAll requires 3 strings")) {
        return false;
    }

    old_len = strlen(old);
    new_len = strlen(new_text);
    text_len = strlen(text);

    if (old_len == 0) {
        return api->raise_error(api, "strReplaceAll requires non-empty old text");
    }

    cursor = text;
    while ((match = strstr(cursor, old)) != NULL) {
        match_count++;
        cursor = match + old_len;
    }

    result_len = text_len + (match_count * new_len) - (match_count * old_len);
    buffer = malloc(result_len + 1);
    if (buffer == NULL) {
        return api->raise_error(api, "strReplaceAll failed to allocate buffer");
    }

    cursor = text;
    {
        char *out = buffer;
        while ((match = strstr(cursor, old)) != NULL) {
            size_t prefix_len = (size_t)(match - cursor);
            memcpy(out, cursor, prefix_len);
            out += prefix_len;
            memcpy(out, new_text, new_len);
            out += new_len;
            cursor = match + old_len;
        }
        strcpy(out, cursor);
    }

    if (!api->pop(api, 3)) {
        free(buffer);
        return false;
    }

    return push_owned_string(api, buffer);
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "strContains", strContains)) {
        return false;
    }
    if (!module->register_function(module, "strHasPrefix", strHasPrefix)) {
        return false;
    }
    if (!module->register_function(module, "strHasSuffix", strHasSuffix)) {
        return false;
    }
    if (!module->register_function(module, "strTrimSpace", strTrimSpace)) {
        return false;
    }
    if (!module->register_function(module, "strSplit", strSplit)) {
        return false;
    }
    if (!module->register_function(module, "strReplaceAll", strReplaceAll)) {
        return false;
    }
    return true;
}
