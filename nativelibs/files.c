/*
 * files.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Purpose:
 * Native text-file helpers for Raden.
 *
 * Exports:
 * - readLines(path) -> list of strings
 * - readText(path) -> string
 * - writeLines(path, lines) -> true
 * - appendLines(path, lines) -> true
 * - writeText(path, text) -> true
 * - appendText(path, text) -> true
 *
 * Example Raden usage:
 * "./libs/files.rdn" load
 * "/tmp/demo.txt" ("a" "b") writeLines call pop
 * "/tmp/demo.txt" readLines call print
 * "\n" print
 * "/tmp/demo.txt" "hello" spit call pop
 * "/tmp/demo.txt" slurp call print
 */

#define _POSIX_C_SOURCE 200809L

#include "../include/rdn_native.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static bool readLines(RDNApi *api) {
    const char *filepath = NULL;
    FILE *file = NULL;
    char *line = NULL;
    size_t capacity = 0;
    ssize_t line_length = 0;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "readLines requires 1 param");
    }

    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "readLines requires string path");
    }

    filepath = api->to_string(api, -1);
    if (filepath == NULL) {
        return api->raise_error(api, "readLines requires string path");
    }

    file = fopen(filepath, "r");
    if (file == NULL) {
        char message[512];
        snprintf(message, sizeof(message), "readLines failed to open '%s': %s", filepath, strerror(errno));
        return api->raise_error(api, message);
    }

    if (!api->pop(api, 1)) {
        fclose(file);
        return false;
    }

    if (!api->push_list(api)) {
        fclose(file);
        return false;
    }

    while ((line_length = getline(&line, &capacity, file)) != -1) {
        if (line_length > 0 && line[line_length - 1] == '\n') {
            line[--line_length] = '\0';
        }

        if (!api->push_string(api, line)) {
            free(line);
            fclose(file);
            return false;
        }

        if (!api->list_append(api, -2, -1)) {
            free(line);
            fclose(file);
            return false;
        }

        if (!api->pop(api, 1)) {
            free(line);
            fclose(file);
            return false;
        }
    }

    free(line);
    fclose(file);
    return true;
}

static bool readText(RDNApi *api) {
    const char *filepath = NULL;
    FILE *file = NULL;
    char *buffer = NULL;
    long size = 0;
    bool ok = false;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "readText requires 1 param");
    }

    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "readText requires string path");
    }

    filepath = api->to_string(api, -1);
    if (filepath == NULL) {
        return api->raise_error(api, "readText requires string path");
    }

    file = fopen(filepath, "rb");
    if (file == NULL) {
        char message[512];
        snprintf(message, sizeof(message), "readText failed to open '%s': %s", filepath, strerror(errno));
        return api->raise_error(api, message);
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        char message[512];
        snprintf(message, sizeof(message), "readText failed to seek '%s': %s", filepath, strerror(errno));
        fclose(file);
        return api->raise_error(api, message);
    }

    size = ftell(file);
    if (size < 0) {
        char message[512];
        snprintf(message, sizeof(message), "readText failed to size '%s': %s", filepath, strerror(errno));
        fclose(file);
        return api->raise_error(api, message);
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        char message[512];
        snprintf(message, sizeof(message), "readText failed to rewind '%s': %s", filepath, strerror(errno));
        fclose(file);
        return api->raise_error(api, message);
    }

    buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(file);
        return api->raise_error(api, "readText failed to allocate buffer");
    }

    if (size > 0 && fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        char message[512];
        snprintf(message, sizeof(message), "readText failed to read '%s': %s", filepath, strerror(errno));
        free(buffer);
        fclose(file);
        return api->raise_error(api, message);
    }

    buffer[size] = '\0';

    if (!api->pop(api, 1)) {
        free(buffer);
        fclose(file);
        return false;
    }

    ok = api->push_string(api, buffer);
    free(buffer);
    fclose(file);
    return ok;
}

static bool write_lines_common(RDNApi *api, const char *mode) {
    const char *filepath = NULL;
    FILE *file = NULL;
    size_t line_count = 0;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "writeLines/appendLines requires 2 params");
    }

    if (api->type(api, -2) != RDN_VALUE_STRING || api->type(api, -1) != RDN_VALUE_LIST) {
        return api->raise_error(api, "writeLines/appendLines requires string path and list");
    }

    filepath = api->to_string(api, -2);
    if (filepath == NULL) {
        return api->raise_error(api, "writeLines/appendLines requires string path");
    }

    if (!api->list_len(api, -1, &line_count)) {
        return api->raise_error(api, "writeLines/appendLines requires list input");
    }

    file = fopen(filepath, mode);
    if (file == NULL) {
        char message[512];
        snprintf(message, sizeof(message), "%s failed to open '%s': %s",
                 strcmp(mode, "a") == 0 ? "appendLines" : "writeLines",
                 filepath,
                 strerror(errno));
        return api->raise_error(api, message);
    }

    for (size_t index = 0; index < line_count; index++) {
        const char *line = NULL;

        if (!api->list_index(api, -1, (long)index)) {
            fclose(file);
            return false;
        }

        line = api->to_string(api, -1);
        if (line == NULL) {
            api->pop(api, 1);
            fclose(file);
            return api->raise_error(api, "writeLines/appendLines requires a list of strings");
        }

        if (fputs(line, file) == EOF || fputc('\n', file) == EOF) {
            char message[512];
            api->pop(api, 1);
            snprintf(message, sizeof(message), "failed writing to '%s': %s", filepath, strerror(errno));
            fclose(file);
            return api->raise_error(api, message);
        }

        if (!api->pop(api, 1)) {
            fclose(file);
            return false;
        }
    }

    fclose(file);

    if (!api->pop(api, 2)) {
        return false;
    }

    return api->push_boolean(api, true);
}

static bool write_text_common(RDNApi *api, const char *mode) {
    const char *filepath = NULL;
    const char *content = NULL;
    FILE *file = NULL;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "writeText/appendText requires 2 params");
    }

    if (api->type(api, -2) != RDN_VALUE_STRING || api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "writeText/appendText requires string path and string content");
    }

    filepath = api->to_string(api, -2);
    content = api->to_string(api, -1);
    if (filepath == NULL || content == NULL) {
        return api->raise_error(api, "writeText/appendText requires string path and string content");
    }

    file = fopen(filepath, mode);
    if (file == NULL) {
        char message[512];
        snprintf(message, sizeof(message), "%s failed to open '%s': %s",
                 strcmp(mode, "ab") == 0 ? "appendText" : "writeText",
                 filepath,
                 strerror(errno));
        return api->raise_error(api, message);
    }

    if (fputs(content, file) == EOF) {
        char message[512];
        snprintf(message, sizeof(message), "failed writing to '%s': %s", filepath, strerror(errno));
        fclose(file);
        return api->raise_error(api, message);
    }

    fclose(file);

    if (!api->pop(api, 2)) {
        return false;
    }

    return api->push_boolean(api, true);
}

static bool writeLines(RDNApi *api) {
    return write_lines_common(api, "w");
}

static bool appendLines(RDNApi *api) {
    return write_lines_common(api, "a");
}

static bool writeText(RDNApi *api) {
    return write_text_common(api, "wb");
}

static bool appendText(RDNApi *api) {
    return write_text_common(api, "ab");
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "readLines", readLines)) {
        return false;
    }
    if (!module->register_function(module, "readText", readText)) {
        return false;
    }
    if (!module->register_function(module, "writeLines", writeLines)) {
        return false;
    }
    if (!module->register_function(module, "appendLines", appendLines)) {
        return false;
    }
    if (!module->register_function(module, "writeText", writeText)) {
        return false;
    }
    if (!module->register_function(module, "appendText", appendText)) {
        return false;
    }
    return true;
}
