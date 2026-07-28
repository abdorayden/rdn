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
 * - openfd(mode, path) -> file handle
 * - readfd(file handle, byte count) -> string
 * - writefd(file handle, text) -> true
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

#include <stddef.h>
#define _POSIX_C_SOURCE 200809L

#include "../include/rdn_native.h"

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/*
 *  "r" "file.txt" open call
 * */
static bool openfd(RDNApi* api) {
    const char *filepath = NULL;
    const char *mode = NULL;
    FILE *fdrdn = NULL;

    if (api->stack_size(api) < 2) {
        api->push_null(api);
        return api->raise_error(api, "openfd requires 2 params");
    }

    if (api->type(api, -2) != RDN_VALUE_STRING || api->type(api, -1) != RDN_VALUE_STRING) {
        api->push_null(api);
        return api->raise_error(api, "openfd requires string mode and string path");
    }

    mode = api->to_string(api, -2);
    filepath = api->to_string(api, -1);
    if (mode == NULL || filepath == NULL) {
        api->push_null(api);
        return api->raise_error(api, "openfd requires string mode and string path");
    }

    fdrdn = fopen(filepath, mode);
    if (fdrdn == NULL) {
        char message[512];
        snprintf(message, sizeof(message), "openfd failed to open '%s': %s", filepath, strerror(errno));
        api->push_null(api);
        return api->raise_error(api, message);
    }

    if (!api->pop(api, 2)) {
        fclose(fdrdn);
        api->push_null(api);
        return false;
    }

    return api->push_integer(api, (long)(intptr_t)fdrdn);
}

static bool my_stdin(RDNApi* api) {
    return api->push_integer(api, (long)(intptr_t)stdin);
}

static bool my_stdout(RDNApi* api) {
    return api->push_integer(api, (long)(intptr_t)stdout);
}

static bool my_stderr(RDNApi* api) {
    return api->push_integer(api, (long)(intptr_t)stderr);
}

/*
 *  fds 10 read call
 * */
static bool readfd(RDNApi* api) {
    long byteread = 0;
    long handle = 0;
    FILE *fdrdn = NULL;
    char *buffer = NULL;
    size_t bytes_read = 0;
    size_t request = 0;
    bool ok = false;

    if (api->stack_size(api) < 2) {
        api->push_null(api);
        return api->raise_error(api, "readfd requires 2 params");
    }

    if (!api->to_integer(api, -2, &handle)) {
        api->push_null(api);
        return api->raise_error(api, "readfd requires file handle and byte count");
    }

    if (!api->to_integer(api, -1, &byteread)) {
        api->push_null(api);
        return api->raise_error(api, "readfd requires file handle and byte count");
    }

    if (byteread < 0) {
        api->push_null(api);
        return api->raise_error(api, "readfd requires non-negative byte count");
    }

    fdrdn = (FILE *)(intptr_t)handle;
    if (fdrdn == NULL) {
        api->push_null(api);
        return api->raise_error(api, "readfd requires valid file handle");
    }

    request = (size_t)byteread;
    buffer = malloc(request + 1);
    if (buffer == NULL) {
        api->push_null(api);
        return api->raise_error(api, "readfd failed to allocate buffer");
    }

    bytes_read = fread(buffer, 1, request, fdrdn);
    if (ferror(fdrdn)) {
        free(buffer);
        api->push_null(api);
        return api->raise_error(api, "readfd failed to read from file");
    }
    buffer[bytes_read] = '\0';

    if (!api->pop(api, 2)) {
        free(buffer);
        return false;
    }

    ok = api->push_string(api, buffer);
    free(buffer);
    return ok;
}

static bool writefd(RDNApi* api) {
    long handle = 0;
    const char *content = NULL;
    FILE *fdrdn = NULL;
    size_t content_len = 0;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "writefd requires 2 params");
    }

    if (!api->to_integer(api, -2, &handle)) {
        return api->raise_error(api, "writefd requires file handle and string content");
    }

    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "writefd requires file handle and string content");
    }

    content = api->to_string(api, -1);
    if (content == NULL) {
        return api->raise_error(api, "writefd requires file handle and string content");
    }

    fdrdn = (FILE *)(intptr_t)handle;
    if (fdrdn == NULL) {
        return api->raise_error(api, "writefd requires valid file handle");
    }

    content_len = strlen(content);
    if (content_len > 0 && fwrite(content, 1, content_len, fdrdn) != content_len) {
        char message[512];
        snprintf(message, sizeof(message), "writefd failed to write: %s", strerror(errno));
        return api->raise_error(api, message);
    }

    if (!api->pop(api, 2)) {
        return false;
    }

    return api->push_boolean(api, true);
}

static bool closefd(RDNApi* api) {
    if(api->stack_size(api) < 1) {
        return api->raise_error(api, "closeHandle requires 1 params");
    }

    long handle = 0;
    FILE *fdrdn = NULL;

    if (!api->to_integer(api, -2, &handle)) {
        api->push_boolean(api , false);
        return api->raise_error(api, "closeHandle requires file handle");
    }

    fdrdn = (FILE *)(intptr_t)handle;

    fclose(fdrdn);

    if (!api->pop(api, 2)) {
        return false;
    }

    api->push_boolean(api , true);
    return true;
}

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

static bool seekfile(RDNApi *api) {
    if (api->stack_size(api) < 3) {
        return api->raise_error(api , "seek accept 3 values on the stack");
    }

    long mode;
    long how_many_bytes;
    long fstream_as_long = 0;
    FILE* fstream = NULL;

    if (!api->to_integer(api ,-1, &mode)) {
        return api->raise_error(api , "seek accept integer seek type at top of the stack");
    }

    if (mode != SEEK_CUR || mode != SEEK_END || mode != SEEK_SET) {
        return api->raise_error(api , "seek mode are not SET or END or CUR wtff");
    }

    if (!api->to_integer(api ,-2, &how_many_bytes)) {
        return api->raise_error(api , "seek accept integer seek type at top of the stack");
    }

    if (!api->to_integer(api ,-3, &fstream_as_long)) {
        return api->raise_error(api , "seek accept integer seek type at top of the stack");
    }
    fstream = (FILE *)(intptr_t)fstream_as_long;

    if (fseek(fstream, how_many_bytes, mode) == -1) {
        return api->raise_error(api , strerror(errno));
    }
    
    return true;
}

static bool file_error(RDNApi* api) {

    if (api->stack_size(api) < 1) {
        return api->raise_error(api , "fis-error requires param file stream");
    }

    long fstream_as_long = 0;
    FILE* fstream = NULL;

    if (!api->to_integer(api ,-1, &fstream_as_long)) {
        return api->raise_error(api , "seek accept integer seek type at top of the stack");
    }
    fstream = (FILE *)(intptr_t)fstream_as_long;

    api->push_boolean(api , ferror(fstream) != 0);

    return true;
}

static bool get_byte(RDNApi* api) {
    if (api->stack_size(api) < 1) {
        return api->raise_error(api , "fget-byte requires param file stream");
    }

    long fstream_as_long = 0;
    FILE* fstream = NULL;

    if (!api->to_integer(api ,-1, &fstream_as_long)) {
        return api->raise_error(api , "seek accept integer seek type at top of the stack");
    }
    fstream = (FILE *)(intptr_t)fstream_as_long;

    api->push_integer(api , fgetc(fstream));

    return true;
}

bool rdn_module_init(RDNModule *module) {

    if (!module->register_function(module, "get_byte", get_byte)) {
        return false;
    }
    if (!module->register_function(module, "file_error", file_error)) {
        return false;
    }
    if (!module->register_function(module, "seek_file", seekfile)) {
        return false;
    }
    if (!module->register_function(module, "openFileHandle", openfd)) {
        return false;
    }
    if (!module->register_function(module, "readFromFileHandle", readfd)) {
        return false;
    }
    if (!module->register_function(module, "writeFromFileHandle", writefd)) {
        return false;
    }
    if (!module->register_function(module, "closeHandle", closefd)) {
        return false;
    }
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

    if (!module->register_function(module, "nativestderr", my_stderr)) {
        return false;
    }

    if (!module->register_function(module, "nativestdout", my_stdout)) {
        return false;
    }

    if (!module->register_function(module, "nativestdin", my_stdin)) {
        return false;
    }

    return true;
}
