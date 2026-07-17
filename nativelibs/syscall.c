/*
 * syscall.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Purpose:
 * Native process and runtime helpers for Raden.
 *
 * Exports:
 * - pid() -> integer
 * - getEnv(name) -> string | null
 * - setEnv(name, value) -> true
 * - listEnv() -> list of strings
 * - sleepMs(milliseconds) -> true
 * - epochTime() -> integer
 * - clockMs() -> integer
 * - randomInt(min, max) -> integer
 * - systm(command) -> integer
 * - gtcwd() -> string
 * - chdr(path) -> integer
 *
 * Example Raden usage:
 * "./libs/os.rdn" load
 * pid call print
 * "\n" print
 * "PATH" getenv call print
 * "\n" print
 * now call print
 * get-cwd call print
 * "\n" print
 * "/" cd call print
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "../include/rdn_native.h"

#include <errno.h>
#include <stdlib.h>
#include <time.h>

#if defined(_WIN32)
#include <processthreadsapi.h>
#include <synchapi.h>
#include <profileapi.h>
#include <direct.h>
#define RDN_GTCWD _getcwd
#define RDN_CHDR _chdir
#define RDN_MAX_PATH 260
#else
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#define RDN_GTCWD getcwd
#define RDN_CHDR chdir
#define RDN_MAX_PATH PATH_MAX
#endif

static bool chdr (RDNApi* api) {
    long result = 0;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "cd requires 1 param (string)");
    }

    const char* new_dir = api->to_string(api, -1);
    if (new_dir == NULL) {
        return api->raise_error(api, "cd requires (string)");
    }

    result = (long)RDN_CHDR(new_dir);

    if (!api->pop(api, 1)) {
        return false;
    }

    return api->push_integer(api, result);
}

static bool gtcwd(RDNApi* api) {
    char buf[RDN_MAX_PATH];
    if (RDN_GTCWD(buf, sizeof(buf)) == NULL ) {
        api->raise_error(api, "getcwd failed");
        return false;
    }
    return api->push_string(api , buf);
}

static bool systm (RDNApi* api) {
    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "system requires 1 param (string)");
    }

    const char *command = NULL;
    command = api->to_string(api, -1);
    if (command == NULL) {
        return api->raise_error(api, "system requires string name");
    }

    long ret = (long)system(command);

    if (!api->pop(api, 1)) {
        return false;
    }

    api->push_integer(api , ret);

    return true;

}

static bool pid(RDNApi *api) {
#if defined(_WIN32)
    return api->push_integer(api, (long)GetCurrentProcessId());
#else
    return api->push_integer(api, (long)getpid());
#endif
}

static bool getEnv(RDNApi *api) {
    const char *name = NULL;
    const char *value = NULL;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "getEnv requires 1 param");
    }

    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "getEnv requires string name");
    }

    name = api->to_string(api, -1);
    if (name == NULL) {
        return api->raise_error(api, "getEnv requires string name");
    }

    value = getenv(name);

    if (!api->pop(api, 1)) {
        return false;
    }

    if (value == NULL) {
        return api->push_null(api);
    }

    return api->push_string(api, value);
}

static bool sleepMs(RDNApi *api) {
    long milliseconds = 0;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "sleepMs requires 1 param");
    }

    if (!api->to_integer(api, -1, &milliseconds)) {
        return api->raise_error(api, "sleepMs requires integer milliseconds");
    }

    if (milliseconds < 0) {
        return api->raise_error(api, "sleepMs requires non-negative milliseconds");
    }

    if (!api->pop(api, 1)) {
        return false;
    }

#if defined(_WIN32)
    Sleep((DWORD)milliseconds);
#else
    {
        struct timespec request;
        struct timespec remaining;

        request.tv_sec = milliseconds / 1000;
        request.tv_nsec = (long)(milliseconds % 1000) * 1000000L;

        while (nanosleep(&request, &remaining) != 0) {
            if (errno != EINTR) {
                return api->raise_error(api, "sleepMs failed");
            }
            request = remaining;
        }
    }
#endif

    return api->push_boolean(api, true);
}

static bool epochTime(RDNApi *api) {
    time_t now = time(NULL);

    if (now == (time_t)-1) {
        return api->raise_error(api, "epochTime failed");
    }

    return api->push_integer(api, (long)now);
}

static bool setEnv(RDNApi *api) {
    const char *name = NULL;
    const char *value = NULL;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "setEnv requires 2 params (name, value)");
    }

    if (api->type(api, -2) != RDN_VALUE_STRING) {
        return api->raise_error(api, "setEnv requires string name");
    }
    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "setEnv requires string value");
    }

    name = api->to_string(api, -2);
    value = api->to_string(api, -1);

    if (name == NULL || value == NULL) {
        return api->raise_error(api, "setEnv requires string params");
    }

#if defined(_WIN32)
    if (_putenv_s(name, value) != 0) {
#else
    if (setenv(name, value, 1) != 0) {
#endif
        return api->raise_error(api, "setEnv failed");
    }

    if (!api->pop(api, 2)) {
        return false;
    }

    return api->push_boolean(api, true);
}

static bool listEnv(RDNApi *api) {
#if defined(_WIN32)
    char **env = _environ;
#else
    extern char **environ;
    char **env = environ;
#endif

    if (!api->push_list(api)) {
        return false;
    }

    while (*env != NULL) {
        if (!api->push_string(api, *env)) {
            return false;
        }
        if (!api->list_append(api, -2, -1)) {
            return false;
        }
        if (!api->pop(api, 1)) {
            return false;
        }
        env++;
    }

    return true;
}

static bool clockMs(RDNApi *api) {
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    if (!QueryPerformanceFrequency(&frequency) ||
        !QueryPerformanceCounter(&counter)) {
        return api->raise_error(api, "clockMs failed");
    }

    return api->push_integer(api, (long)(counter.QuadPart * 1000 / frequency.QuadPart));
#else
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return api->raise_error(api, "clockMs failed");
    }

    return api->push_integer(api, (long)ts.tv_sec * 1000 + (long)ts.tv_nsec / 1000000);
#endif
}

static bool randomInt(RDNApi *api) {
    long min = 0;
    long max = 0;
    long range = 0;
    long result = 0;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "randomInt requires 2 params (min, max)");
    }

    if (!api->to_integer(api, -2, &min)) {
        return api->raise_error(api, "randomInt requires integer min");
    }
    if (!api->to_integer(api, -1, &max)) {
        return api->raise_error(api, "randomInt requires integer max");
    }

    if (min > max) {
        return api->raise_error(api, "randomInt requires min <= max");
    }

    if (!api->pop(api, 2)) {
        return false;
    }

    range = max - min + 1;
    if (range <= 0) {
        return api->push_integer(api, min);
    }

#if defined(_WIN32)
    {
        unsigned int r = 0;
        if (rand_s(&r) != 0) {
            return api->raise_error(api, "randomInt failed");
        }
        result = min + (long)(r % (unsigned int)range);
    }
#else
    result = min + (long)(arc4random_uniform((unsigned int)range));
#endif

    return api->push_integer(api, result);
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "pid", pid)) {
        return false;
    }
    if (!module->register_function(module, "getEnv", getEnv)) {
        return false;
    }
    if (!module->register_function(module, "setEnv", setEnv)) {
        return false;
    }
    if (!module->register_function(module, "listEnv", listEnv)) {
        return false;
    }
    if (!module->register_function(module, "sleepMs", sleepMs)) {
        return false;
    }
    if (!module->register_function(module, "epochTime", epochTime)) {
        return false;
    }
    if (!module->register_function(module, "clockMs", clockMs)) {
        return false;
    }
    if (!module->register_function(module, "randomInt", randomInt)) {
        return false;
    }
    if (!module->register_function(module, "systm", systm)) {
        return false;
    }
    if (!module->register_function(module, "gtcwd", gtcwd)) {
        return false;
    }
    if (!module->register_function(module, "chdr", chdr)) {
        return false;
    }
    return true;
}
