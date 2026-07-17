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
 * - sleepMs(milliseconds) -> true
 * - epochTime() -> integer
 *
 * Example Raden usage:
 * "./libs/os.rdn" load
 * pid call print
 * "\n" print
 * "PATH" env call type print
 * "\n" print
 * now call print
 */

#include <linux/limits.h>
#define _POSIX_C_SOURCE 200809L

#include "../include/rdn_native.h"

#include <errno.h>
#include <stdlib.h>
#include <time.h>

#if defined(_WIN32)
#include <processthreadsapi.h>
#include <synchapi.h>
#include <direct.h>
#define RDN_GTCWD _getcwd
#define RDN_CHDR _chdir
#define RDN_MAX_PATH MAX_PATH
#else
#include <sys/types.h>
#include <unistd.h>
#define RDN_GTCWD getcwd
#define RDN_CHDR chdir
#define RDN_MAX_PATH PATH_MAX
#endif

static bool chdr (RDNApi* api) {
    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "cd requires 1 param (string)");
    }

    const char* new_dir = api->to_string(api, -1);
    if (new_dir == NULL) {
        api->raise_error(api, "cd requires (string)");
        return false;
    }
    
    return api->push_integer(api , (long)RDN_CHDR(new_dir));
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

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "pid", pid)) {
        return false;
    }
    if (!module->register_function(module, "getEnv", getEnv)) {
        return false;
    }
    if (!module->register_function(module, "sleepMs", sleepMs)) {
        return false;
    }
    if (!module->register_function(module, "epochTime", epochTime)) {
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
