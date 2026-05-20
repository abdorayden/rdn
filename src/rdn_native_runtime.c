#include <stdbool.h>
#include <stdlib.h>

#include "../include/rdn_native.h"

static bool set_owned_error_message(char **slot, const char *message) {
    char *copy = NULL;

    free(*slot);
    *slot = NULL;

    if (message == NULL) {
        return true;
    }

    copy = copy_string(message);
    if (copy == NULL) {
        return false;
    }

    *slot = copy;
    return true;
}

static Value *native_get_stack_value(RDNState *stack, long index) {
    long resolved_index = 0;

    if (index == 0) {
        return NULL;
    }

    if (index > 0) {
        resolved_index = index - 1;
    } else {
        resolved_index = (long)stack->count + index;
    }

    if (resolved_index < 0 || (size_t)resolved_index >= stack->count) {
        return NULL;
    }

    return stack->items[resolved_index];
}

static RDNValueType native_value_type_from_value(const Value *value) {
    if (value == NULL) {
        return RDN_VALUE_NONE;
    }

    switch (value->type) {
        case VALUE_NULL:
            return RDN_VALUE_NULL;
        case VALUE_INTEGER:
            return RDN_VALUE_INTEGER;
        case VALUE_DOUBLE:
            return RDN_VALUE_DOUBLE;
        case VALUE_STRING:
            return RDN_VALUE_STRING;
        case VALUE_BOOLEAN:
            return RDN_VALUE_BOOLEAN;
        case VALUE_LIST:
            return RDN_VALUE_LIST;
        case VALUE_AS_VAR:
            return RDN_VALUE_IDENTIFIER;
        default:
            return RDN_VALUE_NONE;
    }
}

static size_t native_api_stack_size(RDNApi *api) {
    NativeCallState *state = api->userdata;
    return state->stack->count;
}

static RDNValueType native_api_type(RDNApi *api, long index) {
    NativeCallState *state = api->userdata;
    return native_value_type_from_value(native_get_stack_value(state->stack, index));
}

static bool native_api_is_number(RDNApi *api, long index) {
    NativeCallState *state = api->userdata;
    Value *value = native_get_stack_value(state->stack, index);
    double number = 0;

    if (value == NULL) {
        return false;
    }

    return value_to_double(value, &number);
}

static bool native_api_to_integer(RDNApi *api, long index, long *out_value) {
    NativeCallState *state = api->userdata;
    Value *value = native_get_stack_value(state->stack, index);

    if (value == NULL) {
        return false;
    }

    return value_to_long(value, out_value);
}

static bool native_api_to_number(RDNApi *api, long index, double *out_value) {
    NativeCallState *state = api->userdata;
    Value *value = native_get_stack_value(state->stack, index);

    if (value == NULL) {
        return false;
    }

    return value_to_double(value, out_value);
}

static bool native_api_to_boolean(RDNApi *api, long index, bool *out_value) {
    NativeCallState *state = api->userdata;
    Value *value = native_get_stack_value(state->stack, index);

    if (value == NULL) {
        return false;
    }

    return value_to_boolean(value, out_value);
}

static const char *native_api_to_string(RDNApi *api, long index) {
    NativeCallState *state = api->userdata;
    Value *value = native_get_stack_value(state->stack, index);

    if (value == NULL || value->type != VALUE_STRING) {
        return NULL;
    }

    return value->as.string;
}

static const char *native_api_to_identifier(RDNApi *api, long index) {
    NativeCallState *state = api->userdata;
    Value *value = native_get_stack_value(state->stack, index);

    if (value == NULL || value->type != VALUE_AS_VAR) {
        return NULL;
    }

    return value->as.string;
}

static bool native_api_pop(RDNApi *api, size_t count) {
    NativeCallState *state = api->userdata;

    if (count > state->stack->count) {
        return native_api_raise_error(api, "native pop exceeds stack size");
    }

    while (count-- > 0) {
        free_value(ray_pop(state->stack));
    }

    return true;
}

static bool native_api_push_null(RDNApi *api) {
    NativeCallState *state = api->userdata;
    return push_value(state->stack, create_null_value());
}

static bool native_api_push_integer(RDNApi *api, long value) {
    NativeCallState *state = api->userdata;
    return push_value(state->stack, create_integer_value(value));
}

static bool native_api_push_number(RDNApi *api, double value) {
    NativeCallState *state = api->userdata;
    return push_value(state->stack, create_double_value(value));
}

static bool native_api_push_boolean(RDNApi *api, bool value) {
    NativeCallState *state = api->userdata;
    return push_value(state->stack, create_boolean_value(value));
}

static bool native_api_push_string(RDNApi *api, const char *value) {
    NativeCallState *state = api->userdata;
    return push_value(state->stack, create_string_value_copy(value));
}

static bool native_api_raise_error(RDNApi *api, const char *message) {
    NativeCallState *state = api->userdata;

    if (!set_owned_error_message(&state->error_message, message)) {
        fprintf(stderr, "failed to allocate native error message\n");
    }
    return false;
}

static NativeModuleReg *create_native_module_reg(const char *name, RDNNativeFunction function) {
    NativeModuleReg *reg = malloc(sizeof(*reg));

    if (reg == NULL) {
        return NULL;
    }

    reg->name = copy_string(name);
    if (reg->name == NULL) {
        free(reg);
        return NULL;
    }

    reg->function = function;
    return reg;
}

static void free_native_module_reg(NativeModuleReg *reg) {
    if (reg == NULL) {
        return;
    }

    free(reg->name);
    free(reg);
}

static void free_native_module_regs(NativeModuleRegs *regs) {
    while (regs->count > 0) {
        free_native_module_reg(ray_pop(regs));
    }

    ray_clear(regs);
}

static bool native_module_register_function(RDNModule *module, const char *name, RDNNativeFunction function) {
    NativeModuleLoadState *state = module->userdata;
    NativeModuleReg *reg = NULL;

    if (name == NULL || name[0] == '\0') {
        return native_module_set_error(module, "native function name must not be empty");
    }

    if (function == NULL) {
        return native_module_set_error(module, "native function callback must not be null");
    }

    for (size_t index = 0; index < state->regs.count; index++) {
        reg = state->regs.items[index];
        if (strcmp(reg->name, name) == 0) {
            reg->function = function;
            return true;
        }
    }

    reg = create_native_module_reg(name, function);
    if (reg == NULL) {
        return native_module_set_error(module, "failed to allocate native registration");
    }

    ray_append(&state->regs, reg);
    return true;
}

static bool native_module_set_error(RDNModule *module, const char *message) {
    NativeModuleLoadState *state = module->userdata;

    if (!set_owned_error_message(&state->error_message, message)) {
        fprintf(stderr, "failed to allocate native module error message\n");
    }
    return false;
}
