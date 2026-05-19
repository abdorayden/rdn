/*
 * unix.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Purpose:
 * Native working-directory and filesystem helpers for Raden.
 *
 * Exports:
 * - cwd() -> string
 * - pathExists(path) -> boolean
 * - makeDir(path) -> true
 * - removePath(path) -> true
 * - listDir(path) -> list of strings
 *
 * Example Raden usage:
 * "./libs/unix.rdn" load
 * pwd call print
 * "\n" print
 * "/tmp" exists call print
 * "\n" print
 * "/tmp" ls call len print
 */

#include "../include/rdn_native.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#define RDN_GETCWD _getcwd
#define RDN_STAT _stat
#define RDN_MKDIR(path) _mkdir(path)
#define RDN_RMDIR(path) _rmdir(path)
#else
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define RDN_GETCWD getcwd
#define RDN_STAT stat
#define RDN_MKDIR(path) mkdir((path), 0777)
#define RDN_RMDIR(path) rmdir(path)
#endif

static char *copy_string(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length);
    return copy;
}

static bool push_errorf(RDNApi *api, const char *prefix, const char *path) {
    char message[512];
    snprintf(message, sizeof(message), "%s '%s': %s", prefix, path, strerror(errno));
    return api->raise_error(api, message);
}

static bool cwd(RDNApi *api) {
    char buffer[4096];

    if (RDN_GETCWD(buffer, sizeof(buffer)) == NULL) {
        return api->raise_error(api, "cwd failed");
    }

    return api->push_string(api, buffer);
}

static bool pathExists(RDNApi *api) {
    const char *path = NULL;
    char *owned_path = NULL;
    struct stat st;
    bool exists = false;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "pathExists requires 1 param");
    }
    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "pathExists requires string path");
    }

    path = api->to_string(api, -1);
    if (path == NULL) {
        return api->raise_error(api, "pathExists requires string path");
    }

    owned_path = copy_string(path);
    if (owned_path == NULL) {
        return api->raise_error(api, "pathExists failed to allocate path");
    }

    exists = (RDN_STAT(owned_path, &st) == 0);

    if (!api->pop(api, 1)) {
        free(owned_path);
        return false;
    }

    if (exists) {
        free(owned_path);
        return api->push_boolean(api, true);
    }

    if (errno == ENOENT) {
        free(owned_path);
        return api->push_boolean(api, false);
    }

    {
        bool ok = push_errorf(api, "pathExists failed for", owned_path);
        free(owned_path);
        return ok;
    }
}

static bool makeDir(RDNApi *api) {
    const char *path = NULL;
    char *owned_path = NULL;
    int result = 0;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "makeDir requires 1 param");
    }
    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "makeDir requires string path");
    }

    path = api->to_string(api, -1);
    if (path == NULL) {
        return api->raise_error(api, "makeDir requires string path");
    }

    owned_path = copy_string(path);
    if (owned_path == NULL) {
        return api->raise_error(api, "makeDir failed to allocate path");
    }

    result = RDN_MKDIR(owned_path);
    if (result != 0) {
        if (errno == EEXIST) {
            if (!api->pop(api, 1)) {
                free(owned_path);
                return false;
            }
            free(owned_path);
            return api->push_boolean(api, true);
        }
        {
            bool ok = push_errorf(api, "makeDir failed for", owned_path);
            free(owned_path);
            return ok;
        }
    }

    if (!api->pop(api, 1)) {
        free(owned_path);
        return false;
    }

    free(owned_path);
    return api->push_boolean(api, true);
}

static bool removePath(RDNApi *api) {
    const char *path = NULL;
    char *owned_path = NULL;
    struct stat st;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "removePath requires 1 param");
    }
    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "removePath requires string path");
    }

    path = api->to_string(api, -1);
    if (path == NULL) {
        return api->raise_error(api, "removePath requires string path");
    }

    owned_path = copy_string(path);
    if (owned_path == NULL) {
        return api->raise_error(api, "removePath failed to allocate path");
    }

    if (RDN_STAT(owned_path, &st) != 0) {
        bool ok = push_errorf(api, "removePath failed for", owned_path);
        free(owned_path);
        return ok;
    }

    if (S_ISDIR(st.st_mode)) {
        if (RDN_RMDIR(owned_path) != 0) {
            bool ok = push_errorf(api, "removePath failed for", owned_path);
            free(owned_path);
            return ok;
        }
    } else {
        if (remove(owned_path) != 0) {
            bool ok = push_errorf(api, "removePath failed for", owned_path);
            free(owned_path);
            return ok;
        }
    }

    if (!api->pop(api, 1)) {
        free(owned_path);
        return false;
    }

    free(owned_path);
    return api->push_boolean(api, true);
}

static bool listDir(RDNApi *api) {
    const char *path = NULL;
    char *owned_path = NULL;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "listDir requires 1 param");
    }
    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "listDir requires string path");
    }

    path = api->to_string(api, -1);
    if (path == NULL) {
        return api->raise_error(api, "listDir requires string path");
    }

    owned_path = copy_string(path);
    if (owned_path == NULL) {
        return api->raise_error(api, "listDir failed to allocate path");
    }

    if (!api->pop(api, 1)) {
        free(owned_path);
        return false;
    }

    if (!api->push_list(api)) {
        free(owned_path);
        return false;
    }

#if defined(_WIN32)
    {
        char search_path[4096];
        WIN32_FIND_DATAA data;
        HANDLE handle;

        if (snprintf(search_path, sizeof(search_path), "%s\\*", owned_path) >= (int)sizeof(search_path)) {
            free(owned_path);
            return api->raise_error(api, "listDir search path too long");
        }

        handle = FindFirstFileA(search_path, &data);
        if (handle == INVALID_HANDLE_VALUE) {
            bool ok = push_errorf(api, "listDir failed for", owned_path);
            free(owned_path);
            return ok;
        }

        do {
            if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
                continue;
            }
            if (!api->push_string(api, data.cFileName)) {
                FindClose(handle);
                free(owned_path);
                return false;
            }
            if (!api->list_append(api, -2, -1)) {
                FindClose(handle);
                free(owned_path);
                return false;
            }
            if (!api->pop(api, 1)) {
                FindClose(handle);
                free(owned_path);
                return false;
            }
        } while (FindNextFileA(handle, &data) != 0);

        FindClose(handle);
        free(owned_path);
        return true;
    }
#else
    {
        DIR *dir = opendir(owned_path);
        struct dirent *entry = NULL;

        if (dir == NULL) {
            bool ok = push_errorf(api, "listDir failed for", owned_path);
            free(owned_path);
            return ok;
        }

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            if (!api->push_string(api, entry->d_name)) {
                closedir(dir);
                free(owned_path);
                return false;
            }
            if (!api->list_append(api, -2, -1)) {
                closedir(dir);
                free(owned_path);
                return false;
            }
            if (!api->pop(api, 1)) {
                closedir(dir);
                free(owned_path);
                return false;
            }
        }

        closedir(dir);
        free(owned_path);
        return true;
    }
#endif
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "cwd", cwd)) {
        return false;
    }
    if (!module->register_function(module, "pathExists", pathExists)) {
        return false;
    }
    if (!module->register_function(module, "makeDir", makeDir)) {
        return false;
    }
    if (!module->register_function(module, "removePath", removePath)) {
        return false;
    }
    if (!module->register_function(module, "listDir", listDir)) {
        return false;
    }
    return true;
}
