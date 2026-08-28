#include "../include/rdn.h"
#include "../include/rdn_native.h"

#include <string.h>

static NativeCallState *native_call_state(RDNApi *api) {
    return (NativeCallState *)api->userdata;
}

static bool coroutine_resolve_target(RDNApi *api, Value *value, Value **out_target) {
    Value *entry = NULL;

    if (out_target == NULL) {
        return api->raise_error(api, "internal coroutine error");
    }

    *out_target = NULL;

    if (value == NULL) {
        return api->raise_error(api, "coroutine requires target");
    }

    if (value->type == VALUE_AS_VAR) {
        entry = api->resolve_variable(api, value->as.string);
        if (entry == NULL) {
            return api->raise_error(api, "unknown coroutine variable");
        }

        if (entry->type != VALUE_LIST) {
            return api->raise_error(api, "coroutine target must be a list");
        }

        *out_target = entry;
        return true;
    }

    if (value->type != VALUE_LIST) {
        return api->raise_error(api, "coroutine target must be a list");
    }

    *out_target = value;
    return true;
}

static bool coCreate(RDNApi *api) {
    NativeCallState *state = native_call_state(api);
    RDNState *stack = state->stack;
    Value *step = NULL;
    Value *initial_state = NULL;

    if (stack == NULL || stack->count < 2) {
        return api->raise_error(api, "coCreate requires 2 params");
    }

    step = rdn_native_get_stack_value(stack, -1);
    initial_state = rdn_native_get_stack_value(stack, -2);
    if (step == NULL || step->type != VALUE_AS_VAR) {
        return api->raise_error(api, "coCreate requires function name and initial state");
    }

    // Build the coroutine as [step, initial_state, true] on an arena-backed
    // list, so it can live for the rest of the program.
    if (!api->pop(api, 2)) {
        return false;
    }
    if (!api->push_list(api)) {
        return false;
    }

    if (!rdn_push_value(stack, step) || !api->list_append(api, -2, -1) || !api->pop(api, 1)) {
        return false;
    }
    if (!rdn_push_value(stack, initial_state) || !api->list_append(api, -2, -1) || !api->pop(api, 1)) {
        return false;
    }
    if (!api->push_boolean(api, true) || !api->list_append(api, -2, -1) || !api->pop(api, 1)) {
        return false;
    }

    return true;
}

static bool coUpdate(RDNApi *api) {
    NativeCallState *state = native_call_state(api);
    RDNState *stack = state->stack;
    Value *target_value = NULL;
    Value *resolved_target = NULL;
    Value *new_state = NULL;
    Value *new_alive = NULL;
    bool alive = false;

    if (stack == NULL || stack->count < 3) {
        return api->raise_error(api, "coUpdate requires 3 params");
    }

    target_value = rdn_native_get_stack_value(stack, -3);
    new_state = rdn_native_get_stack_value(stack, -2);
    new_alive = rdn_native_get_stack_value(stack, -1);
    if (target_value == NULL) {
        return api->raise_error(api, "coUpdate requires coroutine target");
    }

    if (!api->to_boolean(api, -1, &alive)) {
        return api->raise_error(api, "coUpdate requires boolean alive flag");
    }

    if (!coroutine_resolve_target(api, target_value, &resolved_target)) {
        return false;
    }

    if (resolved_target->as.list.count < 3) {
        return api->raise_error(api, "coUpdate requires coroutine list");
    }

    resolved_target->as.list.items[1] = new_state;
    resolved_target->as.list.items[2] = new_alive;

    stack->count -= 3;

    if (target_value->type == VALUE_AS_VAR) { }

    (void)alive;
    return api->push_boolean(api, true);
}

static bool coStatus(RDNApi *api) {
    NativeCallState *state = native_call_state(api);
    Value *target_value = NULL;
    Value *resolved_target = NULL;
    bool alive = false;
    const char *status = NULL;

    if (state->stack == NULL || state->stack->count < 1) {
        return api->raise_error(api, "coStatus requires 1 param");
    }

    target_value = rdn_native_get_stack_value(state->stack, -1);
    if (!coroutine_resolve_target(api, target_value, &resolved_target)) {
        return false;
    }

    if (resolved_target->as.list.count < 3 || resolved_target->as.list.items[2] == NULL ||
        resolved_target->as.list.items[2]->type != VALUE_BOOLEAN) {
        return api->raise_error(api, "coStatus requires coroutine list");
    }

    alive = resolved_target->as.list.items[2]->as.boolean;
    status = alive ? "alive" : "dead";

    if (!api->pop(api, 1)) {
        return false;
    }

    return api->push_string(api, status);
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "coCreate", coCreate)) {
        return false;
    }

    if (!module->register_function(module, "coUpdate", coUpdate)) {
        return false;
    }

    if (!module->register_function(module, "coStatus", coStatus)) {
        return false;
    }

    return true;
}
