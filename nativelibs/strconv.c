/*
 * strconv.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Purpose:
 * Native conversion helpers inspired by Go's strconv package.
 *
 * Exports:
 * - atoi(text) -> integer
 * - atof(text) -> double
 * - parseBool(text) -> boolean
 * - itoa(integer) -> string
 * - formatBool(boolean) -> string
 */

#define _POSIX_C_SOURCE 200809L

#include "../include/rdn_native.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
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

static bool atoi_native(RDNApi *api) {
    const char *text = NULL;
    char *end = NULL;
    long value = 0;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "atoi requires 1 param");
    }

    if (!require_string_arg(api, -1, &text, "atoi requires string text")) {
        return false;
    }

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0') {
        return api->raise_error(api, "atoi requires base-10 integer text");
    }

    if (!api->pop(api, 1)) {
        return false;
    }

    return api->push_integer(api, value);
}

static bool atof_native(RDNApi *api) {
    const char *text = NULL;
    char *end = NULL;
    double value = 0;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "atof requires 1 param");
    }

    if (!require_string_arg(api, -1, &text, "atof requires string text")) {
        return false;
    }

    errno = 0;
    value = strtod(text, &end);
    if (errno == ERANGE || end == text || *end != '\0') {
        return api->raise_error(api, "atof requires floating-point text");
    }

    if (!api->pop(api, 1)) {
        return false;
    }

    return api->push_number(api, value);
}

static bool parseBool_native(RDNApi *api) {
    const char *text = NULL;
    bool value = false;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "parseBool requires 1 param");
    }

    if (!require_string_arg(api, -1, &text, "parseBool requires string text")) {
        return false;
    }

    if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0 || strcmp(text, "t") == 0) {
        value = true;
    } else if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0 || strcmp(text, "f") == 0) {
        value = false;
    } else {
        return api->raise_error(api, "parseBool requires true/false text");
    }

    if (!api->pop(api, 1)) {
        return false;
    }

    return api->push_boolean(api, value);
}

static bool itoa_native(RDNApi *api) {
    long value = 0;
    char buffer[64];

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "itoa requires 1 param");
    }

    if (!api->to_integer(api, -1, &value)) {
        return api->raise_error(api, "itoa requires integer input");
    }

    snprintf(buffer, sizeof(buffer), "%ld", value);

    if (!api->pop(api, 1)) {
        return false;
    }

    return api->push_string(api, buffer);
}

static bool formatBool_native(RDNApi *api) {
    bool value = false;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "formatBool requires 1 param");
    }

    if (!api->to_boolean(api, -1, &value)) {
        return api->raise_error(api, "formatBool requires boolean input");
    }

    if (!api->pop(api, 1)) {
        return false;
    }

    return api->push_string(api, value ? "true" : "false");
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "_atoi", atoi_native)) {
        return false;
    }
    if (!module->register_function(module, "_atof", atof_native)) {
        return false;
    }
    if (!module->register_function(module, "parseBool", parseBool_native)) {
        return false;
    }
    if (!module->register_function(module, "_itoa", itoa_native)) {
        return false;
    }
    if (!module->register_function(module, "formatBool", formatBool_native)) {
        return false;
    }
    return true;
}
