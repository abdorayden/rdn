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

static bool writeLines(RDNApi *api) {
    return write_lines_common(api, "w");
}

static bool appendLines(RDNApi *api) {
    return write_lines_common(api, "a");
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "readLines", readLines)) {
        return false;
    }
    if (!module->register_function(module, "writeLines", writeLines)) {
        return false;
    }
    if (!module->register_function(module, "appendLines", appendLines)) {
        return false;
    }
    return true;
}
