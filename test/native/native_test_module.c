#include "../../include/rdn_native.h"

#include <stdlib.h>
#include <string.h>

static bool native_add(RDNApi *api) {
    double left_number = 0;
    double right_number = 0;
    long left_integer = 0;
    long right_integer = 0;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "native_add requires 2 operands");
    }

    if (api->type(api, -1) == RDN_VALUE_INTEGER && api->type(api, -2) == RDN_VALUE_INTEGER) {
        if (!api->to_integer(api, -1, &right_integer) || !api->to_integer(api, -2, &left_integer)) {
            return api->raise_error(api, "native_add requires integer operands");
        }
        if (!api->pop(api, 2)) {
            return false;
        }
        return api->push_integer(api, left_integer + right_integer);
    }

    if (!api->to_number(api, -1, &right_number) || !api->to_number(api, -2, &left_number)) {
        return api->raise_error(api, "native_add requires numeric operands");
    }

    if (!api->pop(api, 2)) {
        return false;
    }

    return api->push_number(api, left_number + right_number);
}

static bool native_repeat(RDNApi *api) {
    const char *text = NULL;
    long count = 0;
    size_t text_length = 0;
    size_t result_length = 0;
    char *buffer = NULL;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "native_repeat requires 2 operands");
    }

    text = api->to_string(api, -2);
    if (text == NULL || !api->to_integer(api, -1, &count)) {
        return api->raise_error(api, "native_repeat requires string and integer operands");
    }

    if (count < 0) {
        return api->raise_error(api, "native_repeat count must be non-negative");
    }

    text_length = strlen(text);
    result_length = text_length * (size_t)count;
    buffer = malloc(result_length + 1);
    if (buffer == NULL) {
        return api->raise_error(api, "failed to allocate native_repeat buffer");
    }

    for (long index = 0; index < count; index++) {
        memcpy(buffer + ((size_t)index * text_length), text, text_length);
    }
    buffer[result_length] = '\0';

    if (!api->pop(api, 2)) {
        free(buffer);
        return false;
    }

    if (!api->push_string(api, buffer)) {
        free(buffer);
        return api->raise_error(api, "failed to push native_repeat result");
    }

    free(buffer);
    return true;
}

static bool native_not(RDNApi *api) {
    bool value = false;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "native_not requires 1 operand");
    }

    if (!api->to_boolean(api, -1, &value)) {
        return api->raise_error(api, "native_not requires a boolean operand");
    }

    if (!api->pop(api, 1)) {
        return false;
    }

    return api->push_boolean(api, !value);
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "native_add", native_add)) {
        return false;
    }

    if (!module->register_function(module, "native_repeat", native_repeat)) {
        return false;
    }

    if (!module->register_function(module, "native_not", native_not)) {
        return false;
    }

    return true;
}
