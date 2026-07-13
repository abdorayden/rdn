#include "../include/rdn_native.h"
#include "../src/stack.h"

#include <stdlib.h>
#include <string.h>

// redefine all this shit to avoid some unused declarations in rdn.h
typedef enum ValueType {
    VALUE_NULL,
    VALUE_INTEGER,
    VALUE_DOUBLE,
    VALUE_STRING,
    VALUE_BOOLEAN,
    VALUE_LIST,
    VALUE_AS_VAR
} ValueType;

typedef struct Value Value;

typedef struct Vars_t {
    char *var_name;
    Value *var_value;
    bool is_scope_marker;
    bool is_const;
} Vars_t;

struct Value {
    ValueType type;
    union {
        long integer;
        double number;
        char *string;
        bool boolean;
        RLList(Value *) list;
    } as;
};

typedef RLStack(Value *) RDNState;
typedef RLList(Vars_t *) Vars;

typedef struct NativeCallState {
    RDNState *stack;
    Vars *vars;
    char *error_message;
} NativeCallState;

static NativeCallState *native_call_state(RDNApi *api) {
    return (NativeCallState *)api->userdata;
}

static Value *native_stack_value(RDNState *stack, long index) {
    long resolved_index = 0;

    if (stack == NULL || index == 0) {
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

static Vars_t *native_find_var_entry(const Vars *vars, const char *name) {
    size_t index = 0;

    if (vars == NULL || name == NULL) {
        return NULL;
    }

    for (index = 0; index < vars->count; index++) {
        Vars_t *entry = vars->items[index];

        if (entry != NULL && entry->var_name != NULL && strcmp(entry->var_name, name) == 0) {
            return entry;
        }
    }

    return NULL;
}

static Value *native_make_list_value(void) {
    Value *value = malloc(sizeof(*value));

    if (value == NULL) {
        return NULL;
    }

    value->type = VALUE_LIST;
    value->as.list.items = NULL;
    value->as.list.count = 0;
    value->as.list.capacity = 0;
    return value;
}

static Value *native_make_boolean_value(bool boolean) {
    Value *value = malloc(sizeof(*value));

    if (value == NULL) {
        return NULL;
    }

    value->type = VALUE_BOOLEAN;
    value->as.boolean = boolean;
    return value;
}

static void native_free_value(Value *value) {
    size_t index = 0;

    if (value == NULL) {
        return;
    }

    if (value->type == VALUE_STRING || value->type == VALUE_AS_VAR) {
        free(value->as.string);
    } else if (value->type == VALUE_LIST) {
        for (index = 0; index < value->as.list.count; index++) {
            native_free_value(value->as.list.items[index]);
        }
        free(value->as.list.items);
    }

    free(value);
}

static bool coroutine_resolve_target(RDNApi *api, Value *value, Value **out_target) {
    NativeCallState *state = native_call_state(api);
    Vars_t *entry = NULL;

    if (out_target == NULL) {
        return api->raise_error(api, "internal coroutine error");
    }

    *out_target = NULL;

    if (value == NULL) {
        return api->raise_error(api, "coroutine requires target");
    }

    if (value->type == VALUE_AS_VAR) {
        entry = native_find_var_entry(state->vars, value->as.string);
        if (entry == NULL) {
            return api->raise_error(api, "unknown coroutine variable");
        }

        if (entry->var_value == NULL || entry->var_value->type != VALUE_LIST) {
            return api->raise_error(api, "coroutine target must be a list");
        }

        *out_target = entry->var_value;
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
    Value *coro = NULL;
    Value *alive = NULL;

    if (stack == NULL || stack->count < 2) {
        return api->raise_error(api, "coCreate requires 2 params");
    }

    step = native_stack_value(stack, -1);
    initial_state = native_stack_value(stack, -2);
    if (step == NULL || step->type != VALUE_AS_VAR) {
        return api->raise_error(api, "coCreate requires function name and initial state");
    }

    coro = native_make_list_value();
    alive = native_make_boolean_value(true);
    if (coro == NULL || alive == NULL) {
        native_free_value(coro);
        native_free_value(alive);
        return api->raise_error(api, "coCreate failed to allocate coroutine");
    }

    step = ray_pop(stack);
    initial_state = ray_pop(stack);

    ray_append(&coro->as.list, step);
    ray_append(&coro->as.list, initial_state);
    ray_append(&coro->as.list, alive);

    ray_append(stack, coro);
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

    target_value = native_stack_value(stack, -3);
    new_state = native_stack_value(stack, -2);
    new_alive = native_stack_value(stack, -1);
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

    native_free_value(resolved_target->as.list.items[1]);
    native_free_value(resolved_target->as.list.items[2]);
    resolved_target->as.list.items[1] = new_state;
    resolved_target->as.list.items[2] = new_alive;

    stack->count -= 3;

    if (target_value->type == VALUE_AS_VAR) {
        native_free_value(target_value);
    }

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

    target_value = native_stack_value(state->stack, -1);
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
