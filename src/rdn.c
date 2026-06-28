#include <dlfcn.h>
#include <stdbool.h>
#include <stddef.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../include/rdn_native.h"
#include "stack.h"
#include "../include/rdn.h"


static bool is_token(const char *value, const char *expected) {
    return strcmp(value, expected) == 0;
}

static bool is_operator_token(const char *value) {
    return is_token(value, "+") || is_token(value, "-") || is_token(value, "*") ||
           is_token(value, "/") || is_token(value, "<<") || is_token(value, ">>") ||
           is_token(value, "|") || is_token(value, "&") || is_token(value, "^") ||
           is_token(value, "<") || is_token(value, ">") || is_token(value, "<=") ||
           is_token(value, ">=") || is_token(value, "!=") || is_token(value, "=") ||
           is_token(value, "!") || is_token(value, "%") || is_token(value, "//");
}

static char *copy_string(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length);
    return copy;
}

static void diagnostic_set_source(const char *path, const char *source, size_t base_line, size_t base_column) {
    g_diagnostic_context.path = path;
    g_diagnostic_context.source = source;
    g_diagnostic_context.base_line = base_line == 0 ? 1 : base_line;
    g_diagnostic_context.base_column = base_column == 0 ? 1 : base_column;
    g_diagnostic_context.last_token_start = source;
    g_diagnostic_context.last_token_end = source;
}

static void diagnostic_set_last_token(const char *start, const char *end) {
    g_diagnostic_context.last_token_start = start;
    g_diagnostic_context.last_token_end = end;
}

static void diagnostic_compute_location(const char *pointer, size_t *out_line, size_t *out_column,
                                        const char **out_line_start, const char **out_line_end) {
    const char *source = g_diagnostic_context.source;
    const char *scan = NULL;
    const char *line_start = NULL;
    const char *line_end = NULL;
    size_t line = g_diagnostic_context.base_line == 0 ? 1 : g_diagnostic_context.base_line;
    size_t column = g_diagnostic_context.base_column == 0 ? 1 : g_diagnostic_context.base_column;

    if (source == NULL) {
        if (out_line != NULL) {
            *out_line = 1;
        }
        if (out_column != NULL) {
            *out_column = 1;
        }
        if (out_line_start != NULL) {
            *out_line_start = NULL;
        }
        if (out_line_end != NULL) {
            *out_line_end = NULL;
        }
        return;
    }

    if (pointer == NULL) {
        pointer = g_diagnostic_context.last_token_start != NULL ? g_diagnostic_context.last_token_start : source;
    }

    if (pointer < source) {
        pointer = source;
    }

    line_start = source;
    scan = source;
    while (*scan != '\0' && scan < pointer) {
        if (*scan == '\n') {
            line++;
            column = 1;
            line_start = scan + 1;
        } else {
            column++;
        }
        scan++;
    }

    line_end = line_start;
    while (*line_end != '\0' && *line_end != '\n') {
        line_end++;
    }

    if (out_line != NULL) {
        *out_line = line;
    }
    if (out_column != NULL) {
        *out_column = column;
    }
    if (out_line_start != NULL) {
        *out_line_start = line_start;
    }
    if (out_line_end != NULL) {
        *out_line_end = line_end;
    }
}

static bool diagnostic_emitv(const char *kind, const char *pointer, const char *fmt, va_list args) {
    char message[1024];
    size_t line = 1;
    size_t column = 1;
    const char *line_start = NULL;
    const char *line_end = NULL;
    const char *path = g_diagnostic_context.path != NULL ? g_diagnostic_context.path : "<repl>";

    vsnprintf(message, sizeof(message), fmt, args);
    diagnostic_compute_location(pointer, &line, &column, &line_start, &line_end);

    {
        char *trace_copy = copy_string(message);
        if (trace_copy != NULL) {
            ray_append(&g_stack_trace_protected, trace_copy);
        }
    }

    if (!g_diagnostics_suppressed) {
        fprintf(stderr, "%s:%zu:%zu: %s: %s\n", path, line, column, kind, message);
        if (line_start != NULL && line_end != NULL) {
            fprintf(stderr, "%.*s\n", (int)(line_end - line_start), line_start);
            for (size_t i = 1; i < column; i++) {
                fputc(' ', stderr);
            }
            fprintf(stderr, "^\n");
        }
    }
    return false;
}

static bool diagnostic_error_at(const char *pointer, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    diagnostic_emitv("error", pointer, fmt, args);
    va_end(args);
    return false;
}

static bool diagnostic_error_current(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    diagnostic_emitv("error", g_diagnostic_context.last_token_start, fmt, args);
    va_end(args);
    return false;
}

static bool diagnostic_note_current(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    diagnostic_emitv("note", g_diagnostic_context.last_token_start, fmt, args);
    va_end(args);
    return false;
}

static Value *create_null_value(void) {
    Value *value = malloc(sizeof(*value));

    if (value == NULL) {
        return NULL;
    }

    value->type = VALUE_NULL;
    value->as.integer = 0;
    return value;
}

static Value *create_integer_value(long integer) {
    Value *value = malloc(sizeof(*value));

    if (value == NULL) {
        return NULL;
    }

    value->type = VALUE_INTEGER;
    value->as.integer = integer;
    return value;
}

static Value *create_double_value(double number) {
    Value *value = malloc(sizeof(*value));

    if (value == NULL) {
        return NULL;
    }

    value->type = VALUE_DOUBLE;
    value->as.number = number;
    return value;
}

static Value *create_boolean_value(bool boolean) {
    Value *value = malloc(sizeof(*value));
    if (value == NULL) {
        return NULL;
    }

    value->type = VALUE_BOOLEAN;
    value->as.boolean = boolean;
    return value;
}

static Value *create_string_value_owned(char *string) {
    Value *value = malloc(sizeof(*value));

    if (value == NULL) {
        free(string);
        return NULL;
    }

    value->type = VALUE_STRING;
    value->as.string = string;
    return value;
}

static Value *create_string_value_copy(const char *string) {
    char *copy = copy_string(string);

    if (copy == NULL) {
        return NULL;
    }

    return create_string_value_owned(copy);
}

static Value *create_list_value(void) {
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

static Value *create_var_name_value(const char *name) {
    Value *value = malloc(sizeof(*value));

    if (value == NULL) {
        return NULL;
    }

    value->type = VALUE_AS_VAR;
    value->as.string = copy_string(name);
    if (value->as.string == NULL) {
        free(value);
        return NULL;
    }

    return value;
}

static Value *clone_value(const Value *value) {
    size_t index = 0;
    Value *copy = NULL;

    if (value == NULL) {
        return NULL;
    }

    switch (value->type) {
        case VALUE_NULL:
            return create_null_value();
        case VALUE_INTEGER:
            return create_integer_value(value->as.integer);
        case VALUE_DOUBLE:
            return create_double_value(value->as.number);
        case VALUE_BOOLEAN:
            return create_boolean_value(value->as.boolean);
        case VALUE_STRING:
            return create_string_value_copy(value->as.string);
        case VALUE_AS_VAR:
            return create_var_name_value(value->as.string);
        case VALUE_LIST:
            copy = create_list_value();
            if (copy == NULL) {
                return NULL;
            }

            for (index = 0; index < value->as.list.count; index++) {
                Value *item_copy = clone_value(value->as.list.items[index]);
                if (item_copy == NULL) {
                    free_value(copy);
                    return NULL;
                }
                ray_append(&copy->as.list, item_copy);
            }
            return copy;
        default:
            return NULL;
    }
}

static Vars_t *create_scope_marker(void) {
    Vars_t *entry = malloc(sizeof(*entry));

    if (entry == NULL) {
        return NULL;
    }

    entry->var_name = NULL;
    entry->var_value = NULL;
    entry->is_scope_marker = true;
    entry->is_const = false;
    return entry;
}

static Vars_t *create_var_entry(const char *name, Value *value, bool is_const) {
    Vars_t *entry = malloc(sizeof(*entry));

    if (entry == NULL) {
        return NULL;
    }

    entry->var_name = copy_string(name);
    if (entry->var_name == NULL) {
        free(entry);
        return NULL;
    }

    entry->var_value = value;
    entry->is_scope_marker = false;
    entry->is_const = is_const;
    return entry;
}

static Funcs_t *create_func_entry(const char *name, char *body, const char *source_path, size_t source_line, size_t source_column) {
    Funcs_t *entry = malloc(sizeof(*entry));

    if (entry == NULL) {
        free(body);
        return NULL;
    }

    entry->func_name = copy_string(name);
    if (entry->func_name == NULL) {
        free(body);
        free(entry);
        return NULL;
    }

    entry->type = FUNC_SCRIPT;
    entry->as.func_body = body;
    entry->source_path = copy_string(source_path == NULL ? "<repl>" : source_path);
    if (entry->source_path == NULL) {
        free(entry->func_name);
        free(body);
        free(entry);
        return NULL;
    }
    entry->source_line = source_line;
    entry->source_column = source_column;
    entry->native_library_handle = NULL;
    return entry;
}

static Funcs_t *create_native_func_entry(const char *name, RDNNativeFunction native_function, void *native_library_handle) {
    Funcs_t *entry = malloc(sizeof(*entry));

    if (entry == NULL) {
        return NULL;
    }

    entry->func_name = copy_string(name);
    if (entry->func_name == NULL) {
        free(entry);
        return NULL;
    }

    entry->type = FUNC_NATIVE;
    entry->as.native_function = native_function;
    entry->source_path = NULL;
    entry->source_line = 0;
    entry->source_column = 0;
    entry->native_library_handle = native_library_handle;
    return entry;
}

static void free_var_entry(Vars_t *entry) {
    if (entry == NULL) {
        return;
    }

    free(entry->var_name);
    free_value(entry->var_value);
    free(entry);
}

static void free_vars(Vars *vars) {
    while (vars->count > 0) {
        free_var_entry(ray_pop(vars));
    }

    ray_clear(vars);
}

static void free_func_entry(Funcs_t *entry) {
    if (entry == NULL) {
        return;
    }

    free(entry->func_name);
    if (func_entry_has_body(entry)) {
        free(entry->as.func_body);
    }
    free(entry->source_path);
    free(entry);
}

static void free_funcs(Funcs *funcs) {
    while (funcs->count > 0) {
        Funcs_t *entry = ray_pop(funcs);
        void *library_handle = entry->native_library_handle;
        bool seen = false;

        if (library_handle != NULL) {
            for (size_t index = 0; index < funcs->count; index++) {
                if (funcs->items[index]->native_library_handle == library_handle) {
                    seen = true;
                    break;
                }
            }
        }

        free_func_entry(entry);
        if (library_handle != NULL && !seen) {
            dlclose(library_handle);
        }
    }

    ray_clear(funcs);
}

static bool vars_push_scope(Vars *vars) {
    Vars_t *marker = create_scope_marker();

    if (marker == NULL) {
        return diagnostic_error_current("failed to allocate scope marker");
    }

    ray_append(vars, marker);
    return true;
}

static void vars_pop_scope(Vars *vars) {
    while (vars->count > 0) {
        Vars_t *entry = ray_pop(vars);
        bool is_marker = entry->is_scope_marker;

        free_var_entry(entry);
        if (is_marker) {
            return;
        }
    }
}

static Vars_t *find_var_entry(const Vars *vars, const char *name) {
    size_t index = vars->count;

    while (index > 0) {
        Vars_t *entry = vars->items[--index];

        if (entry->is_scope_marker) {
            continue;
        }

        if (strcmp(entry->var_name, name) == 0) {
            return entry;
        }
    }

    return NULL;
}

static Vars_t *find_current_scope_var_entry(const Vars *vars, const char *name) {
    size_t index = vars->count;

    while (index > 0) {
        Vars_t *entry = vars->items[--index];

        if (entry->is_scope_marker) {
            break;
        }

        if (strcmp(entry->var_name, name) == 0) {
            return entry;
        }
    }

    return NULL;
}

static Funcs_t *find_func_entry(const Funcs *funcs, const char *name) {
    size_t index = funcs->count;

    while (index > 0) {
        Funcs_t *entry = funcs->items[--index];

        if (strcmp(entry->func_name, name) == 0) {
            return entry;
        }
    }

    return NULL;
}

static bool func_entry_has_body(const Funcs_t *entry) {
    return entry->type == FUNC_SCRIPT || entry->type == FUNC_APPLY || entry->type == FUNC_DEMAC;
}

static bool funcs_define(Funcs *funcs, const char *name, char *body, const char *source_path, size_t source_line, size_t source_column) {
    Funcs_t *entry = find_func_entry(funcs, name);

    if (entry != NULL) {
        if (func_entry_has_body(entry)) {
            free(entry->as.func_body);
        }
        entry->type = FUNC_SCRIPT;
        entry->as.func_body = body;
        free(entry->source_path);
        entry->source_path = copy_string(source_path == NULL ? "<repl>" : source_path);
        if (entry->source_path == NULL) {
            free(body);
            return diagnostic_error_current("failed to allocate function source path");
        }
        entry->source_line = source_line;
        entry->source_column = source_column;
        entry->native_library_handle = NULL;
        return true;
    }

    entry = create_func_entry(name, body, source_path, source_line, source_column);
    if (entry == NULL) {
        return diagnostic_error_current("failed to allocate function entry");
    }

    ray_append(funcs, entry);
    return true;
}

static bool funcs_define_apply(Funcs *funcs, const char *name, char *body, const char *source_path, size_t source_line, size_t source_column) {
    Funcs_t *entry = find_func_entry(funcs, name);

    if (entry != NULL) {
        if (func_entry_has_body(entry)) {
            free(entry->as.func_body);
        }
        entry->type = FUNC_APPLY;
        entry->as.func_body = body;
        free(entry->source_path);
        entry->source_path = copy_string(source_path == NULL ? "<repl>" : source_path);
        if (entry->source_path == NULL) {
            free(body);
            return diagnostic_error_current("failed to allocate function source path");
        }
        entry->source_line = source_line;
        entry->source_column = source_column;
        entry->native_library_handle = NULL;
        return true;
    }

    entry = create_func_entry(name, body, source_path, source_line, source_column);
    if (entry == NULL) {
        return diagnostic_error_current("failed to allocate function entry");
    }

    entry->type = FUNC_APPLY;
    ray_append(funcs, entry);
    return true;
}

static bool funcs_define_demac(Funcs *funcs, const char *name, char *body, const char *source_path, size_t source_line, size_t source_column) {
    Funcs_t *entry = find_func_entry(funcs, name);

    if (entry != NULL) {
        if (func_entry_has_body(entry)) {
            free(entry->as.func_body);
        }
        entry->type = FUNC_DEMAC;
        entry->as.func_body = body;
        free(entry->source_path);
        entry->source_path = copy_string(source_path == NULL ? "<repl>" : source_path);
        if (entry->source_path == NULL) {
            free(body);
            return diagnostic_error_current("failed to allocate macro source path");
        }
        entry->source_line = source_line;
        entry->source_column = source_column;
        entry->native_library_handle = NULL;
        return true;
    }

    entry = create_func_entry(name, body, source_path, source_line, source_column);
    if (entry == NULL) {
        return diagnostic_error_current("failed to allocate macro entry");
    }

    entry->type = FUNC_DEMAC;
    ray_append(funcs, entry);
    return true;
}

static bool funcs_define_native(Funcs *funcs, const char *name, RDNNativeFunction native_function, void *native_library_handle) {
    Funcs_t *entry = find_func_entry(funcs, name);

    if (entry != NULL) {
        if (func_entry_has_body(entry)) {
            free(entry->as.func_body);
        }
        entry->type = FUNC_NATIVE;
        entry->as.native_function = native_function;
        entry->native_library_handle = native_library_handle;
        return true;
    }

    entry = create_native_func_entry(name, native_function, native_library_handle);
    if (entry == NULL) {
        fprintf(stderr, "failed to allocate native function entry\n");
        return false;
    }

    ray_append(funcs, entry);
    return true;
}

static bool vars_let(Vars *vars, const char *name, const Value *value) {
    Vars_t *entry = find_current_scope_var_entry(vars, name);
    Value *copy = clone_value(value);

    if (copy == NULL) {
        return diagnostic_error_current("failed to clone variable value");
    }

    if (entry != NULL) {
        if (entry->is_const) {
            free_value(copy);
            return diagnostic_error_current("cannot change constant '%s'", name);
        }
        free_value(entry->var_value);
        entry->var_value = copy;
        return true;
    }

    entry = create_var_entry(name, copy, false);
    if (entry == NULL) {
        free_value(copy);
        return diagnostic_error_current("failed to allocate variable entry");
    }

    ray_append(vars, entry);
    return true;
}

static bool vars_set(Vars *vars, const char *name, const Value *value) {
    Vars_t *entry = find_var_entry(vars, name);
    Value *copy = clone_value(value);

    if (entry == NULL) {
        return diagnostic_error_current("unknown variable '%s'", name);
    }

    if (copy == NULL) {
        return diagnostic_error_current("failed to clone variable value");
    }

    if (entry->is_const) {
        free_value(copy);
        return diagnostic_error_current("cannot change constant '%s'", name);
    }

    free_value(entry->var_value);
    entry->var_value = copy;
    return true;
}

static bool vars_const(Vars *vars, const char *name, const Value *value) {
    Vars_t *entry = find_current_scope_var_entry(vars, name);
    Value *copy = clone_value(value);

    if (entry != NULL) {
        return diagnostic_error_current("'%s' already exists in current scope", name);
    }

    if (copy == NULL) {
        return diagnostic_error_current("failed to clone constant value");
    }

    entry = create_var_entry(name, copy, true);
    if (entry == NULL) {
        free_value(copy);
        return diagnostic_error_current("failed to allocate constant entry");
    }

    ray_append(vars, entry);
    return true;
}

static void free_value(Value *value) {
    size_t index = 0;

    if (value == NULL) {
        return;
    }

    if (value->type == VALUE_STRING || value->type == VALUE_AS_VAR) {
        free(value->as.string);
    } else if (value->type == VALUE_LIST) {
        for (index = 0; index < value->as.list.count; index++) {
            free_value(value->as.list.items[index]);
        }
        free(value->as.list.items);
    }

    free(value);
}

static void free_stack_values(RDNState *stack) {
    while (!(ray_is_empty(stack))) {
        free_value(ray_pop(stack));
    }

    ray_clear(stack);
}

static bool push_value(RDNState *stack, Value *value) {
    if (value == NULL) {
        fprintf(stderr, "failed to allocate value\n");
        return false;
    }

    ray_append(stack, value);
    return true;
}

static bool parse_integer_token(const char *text, long *out_value) {
    char *end = NULL;
    const char *digits = text;
    long value = 0;
    int base = 10;
    bool negative = false;

    if (text == NULL || *text == '\0') {
        return false;
    }

    if (*digits == '+') {
        digits++;
    } else if (*digits == '-') {
        negative = true;
        digits++;
    }

    if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
        base = 16;
        digits += 2;
    } else if (digits[0] == '0' && (digits[1] == 'b' || digits[1] == 'B')) {
        base = 2;
        digits += 2;
    } else if (digits[0] == '0' && (digits[1] == 'o' || digits[1] == 'O')) {
        base = 8;
        digits += 2;
    }

    if (*digits == '\0') {
        return false;
    }

    errno = 0;
    value = strtol(digits, &end, base);
    if (errno == ERANGE || *end != '\0') {
        return false;
    }

    if (negative) {
        value = -value;
    }

    *out_value = value;
    return true;
}

static bool parse_double_token(const char *text, double *out_value) {
    char *end = NULL;
    double value = 0;

    if (text == NULL || *text == '\0') {
        return false;
    }

    errno = 0;
    value = strtod(text, &end);
    if (errno == ERANGE || *end != '\0') {
        return false;
    }

    *out_value = value;
    return true;
}

static bool value_to_double(const Value *value, double *out_value) {
    if (value->type == VALUE_INTEGER) {
        *out_value = (double)value->as.integer;
        return true;
    }

    if (value->type == VALUE_DOUBLE) {
        *out_value = value->as.number;
        return true;
    }

    return false;
}

static bool value_to_long(const Value *value, long *out_value) {
    if (value->type != VALUE_INTEGER) {
        return false;
    }

    *out_value = value->as.integer;
    return true;
}

static bool value_to_boolean(const Value *value, bool *out_value) {
    if (value->type != VALUE_BOOLEAN) {
        return false;
    }

    *out_value = value->as.boolean;
    return true;
}

static Value *resolve_value_if_var(const Vars *vars, Value *value, const char *context) {
    Vars_t *entry = NULL;

    if (value == NULL || value->type != VALUE_AS_VAR) {
        return value;
    }

    entry = find_var_entry(vars, value->as.string);
    if (entry == NULL) {
        diagnostic_error_current("%s requires known variable '%s'", context == NULL ? "(UNKNOWN)" : context, value->as.string);
        free_value(value);
        return NULL;
    }

    free_value(value);
    return clone_value(entry->var_value);
}

static bool append_value_repr(char **buffer, size_t *length, const Value *value) {
    char tmp[64];
    size_t index = 0;

    if (value->type == VALUE_INTEGER) {
        snprintf(tmp, sizeof(tmp), "%ld", value->as.integer);
        return append_text(buffer, length, tmp);
    }

    if (value->type == VALUE_NULL) {
        return append_text(buffer, length, "null");
    }

    if (value->type == VALUE_DOUBLE) {
        snprintf(tmp, sizeof(tmp), "%.15g", value->as.number);
        return append_text(buffer, length, tmp);
    }

    if (value->type == VALUE_BOOLEAN) {
        return append_text(buffer, length, value->as.boolean ? "true" : "false");
    }

    if (value->type == VALUE_STRING || value->type == VALUE_AS_VAR) {
        return append_text(buffer, length, value->as.string);
    }

    if (value->type == VALUE_LIST) {
        if (!append_text(buffer, length, "(")) {
            return false;
        }
        for (index = 0; index < value->as.list.count; index++) {
            if (index > 0 && !append_text(buffer, length, " ")) {
                return false;
            }
            if (!append_value_repr(buffer, length, value->as.list.items[index])) {
                return false;
            }
        }
        return append_text(buffer, length, ")");
    }

    return false;
}

static bool values_equal(const Value *left, const Value *right) {
    double left_double = 0;
    double right_double = 0;

    if ((left->type == VALUE_INTEGER || left->type == VALUE_DOUBLE) &&
        (right->type == VALUE_INTEGER || right->type == VALUE_DOUBLE)) {
        value_to_double(left, &left_double);
        value_to_double(right, &right_double);
        return left_double == right_double;
    }

    if (left->type != right->type) {
        return false;
    }

    if (left->type == VALUE_NULL) {
        return true;
    }

    if (left->type == VALUE_BOOLEAN) {
        return left->as.boolean == right->as.boolean;
    }

    if (left->type == VALUE_STRING) {
        return strcmp(left->as.string, right->as.string) == 0;
    }

    return left->as.integer == right->as.integer;
}

static bool values_not_equal(const Value *left, const Value *right) {
    return !values_equal(left, right);
}

static bool values_compare(const Value *left, const Value *right, const char *operator_token, bool *out_value) {
    double left_double = 0;
    double right_double = 0;

    if (!value_to_double(left, &left_double) || !value_to_double(right, &right_double)) {
        return false;
    }

    if (is_token(operator_token, "<")) {
        *out_value = left_double < right_double;
    } else if (is_token(operator_token, ">")) {
        *out_value = left_double > right_double;
    } else if (is_token(operator_token, "<=")) {
        *out_value = left_double <= right_double;
    } else {
        *out_value = left_double >= right_double;
    }

    return true;
}

static void print_value(const Value *value) {
    size_t index = 0;

    if (value->type == VALUE_INTEGER) {
        printf("%ld", value->as.integer);
        return;
    }

    if (value->type == VALUE_NULL) {
        printf("null");
        return;
    }

    if (value->type == VALUE_DOUBLE) {
        printf("%.15g", value->as.number);
        return;
    }

    if (value->type == VALUE_BOOLEAN) {
        printf("%s", value->as.boolean ? "true" : "false");
        return;
    }

    if (value->type == VALUE_LIST) {
        putchar('(');
        for (index = 0; index < value->as.list.count; index++) {
            if (index > 0) {
                putchar(' ');
            }
            print_value(value->as.list.items[index]);
        }
        putchar(')');
        return;
    }

    printf("%s", value->as.string);
}

static char* exit_value(const Value* value , int* out_exit) {
    if (value->type == VALUE_INTEGER) {
        if (out_exit) *out_exit = value->as.integer;
        return NULL;
    }
    return "ERROR: exit expect integer";
}

static bool apply_binary_operator(RDNState *stack, Vars *vars, const char *operator_token) {
    Value *left = NULL;
    Value *right = NULL;
    Value *result = NULL;
    double left_double = 0;
    double right_double = 0;
    long left_long = 0;
    long right_long = 0;
    bool left_bool = false;
    bool right_bool = false;

    if (is_token(operator_token, "!")) {
        if (stack->count < 1) {
            return diagnostic_error_current("operator '%s' requires 1 operand, got %zu", operator_token, stack->count);
        }

        right = resolve_value_if_var(vars, ray_pop(stack), operator_token);
        if (right == NULL) {
            return false;
        }
        if (!value_to_boolean(right, &right_bool)) {
            diagnostic_error_current("operator '%s' requires a boolean operand", operator_token);
            ray_append(stack, right);
            return false;
        }

        result = create_boolean_value(!right_bool);
        if (result == NULL) {
            fprintf(stderr, "failed to allocate result\n");
            ray_append(stack, right);
            return false;
        }

        free_value(right);
        ray_append(stack, result);
        return true;
    }

    if (stack->count < 2) {
        return diagnostic_error_current("operator '%s' requires 2 operands, got %zu", operator_token, stack->count);
    }

    right = resolve_value_if_var(vars, ray_pop(stack), operator_token);
    left = resolve_value_if_var(vars, ray_pop(stack), operator_token);
    if (left == NULL || right == NULL) {
        free_value(left);
        free_value(right);
        return false;
    }

    if (is_token(operator_token, "=")) {
        result = create_boolean_value(values_equal(left, right));
    } else if (is_token(operator_token, "!=")) {
        result = create_boolean_value(values_not_equal(left, right));
    } else if (is_token(operator_token, "<") || is_token(operator_token, ">") ||
               is_token(operator_token, "<=") || is_token(operator_token, ">=")) {
        if (!values_compare(left, right, operator_token, &right_bool)) {
            diagnostic_error_current("operator '%s' requires numeric operands", operator_token);
            ray_append(stack, left);
            ray_append(stack, right);
            return false;
        }
        result = create_boolean_value(right_bool);
    } else if (is_token(operator_token, "|") || is_token(operator_token, "&")) {
        if (value_to_boolean(left, &left_bool) && value_to_boolean(right, &right_bool)) {
            if (is_token(operator_token, "|")) {
                result = create_boolean_value(left_bool || right_bool);
            } else {
                result = create_boolean_value(left_bool && right_bool);
            }
        } else if (left->type == VALUE_BOOLEAN || right->type == VALUE_BOOLEAN) {
            diagnostic_error_current("operator '%s' requires both operands to be boolean or integer", operator_token);
            ray_append(stack, left);
            ray_append(stack, right);
            return false;
        } else {
            if (!value_to_long(left, &left_long) || !value_to_long(right, &right_long)) {
                diagnostic_error_current("operator '%s' requires integer operands", operator_token);
                ray_append(stack, left);
                ray_append(stack, right);
                return false;
            }

            if (is_token(operator_token, "|")) {
                result = create_integer_value(left_long | right_long);
            } else {
                result = create_integer_value(left_long & right_long);
            }
        }
    } else if (is_token(operator_token, "<<") || is_token(operator_token, ">>") ||
               is_token(operator_token, "^")) {
        if (!value_to_long(left, &left_long) || !value_to_long(right, &right_long)) {
            diagnostic_error_current("operator '%s' requires integer operands", operator_token);
            ray_append(stack, left);
            ray_append(stack, right);
            return false;
        }

        if ((is_token(operator_token, "<<") || is_token(operator_token, ">>")) && right_long < 0) {
            diagnostic_error_current("shift operators require a non-negative count");
            ray_append(stack, left);
            ray_append(stack, right);
            return false;
        }

        if (is_token(operator_token, "<<")) {
            result = create_integer_value(left_long << right_long);
        } else if (is_token(operator_token, ">>")) {
            result = create_integer_value(left_long >> right_long);
        } else {
            result = create_integer_value(left_long ^ right_long);
        }
    } else {
        if (!value_to_double(left, &left_double) || !value_to_double(right, &right_double)) {
            diagnostic_error_current("operator '%s' requires numeric operands", operator_token);
            ray_append(stack, left);
            ray_append(stack, right);
            return false;
        }

        if (is_token(operator_token, "/") && right_double == 0.0) {
            diagnostic_error_current("division by zero");
            ray_append(stack, left);
            ray_append(stack, right);
            return false;
        }

        if (left->type == VALUE_INTEGER && right->type == VALUE_INTEGER && !is_token(operator_token, "/")) {
            if (is_token(operator_token, "+")) {
                result = create_integer_value(left->as.integer + right->as.integer);
            } else if (is_token(operator_token, "-")) {
                result = create_integer_value(left->as.integer - right->as.integer);
            } else if (is_token(operator_token, "*")) {
                result = create_integer_value(left->as.integer * right->as.integer);
            }else if (is_token(operator_token, "%")) {
                result = create_integer_value(left->as.integer % right->as.integer);
            }else if (is_token(operator_token, "//")) {
                result = create_integer_value((long)(left->as.integer / right->as.integer));
            }
        } else if (left->type == VALUE_INTEGER && right->type == VALUE_INTEGER && is_token(operator_token, "/") &&
                   left->as.integer % right->as.integer == 0) {
            result = create_integer_value(left->as.integer / right->as.integer);
        } else {
            if (is_token(operator_token, "+")) {
                result = create_double_value(left_double + right_double);
            } else if (is_token(operator_token, "-")) {
                result = create_double_value(left_double - right_double);
            } else if (is_token(operator_token, "*")) {
                result = create_double_value(left_double * right_double);
            } else if (is_token(operator_token, "/")) {
                result = create_double_value(left_double / right_double);
            }
        }
    }

    if (result == NULL) {
        fprintf(stderr, "failed to allocate result\n");
        ray_append(stack, left);
        ray_append(stack, right);
        return false;
    }

    free_value(left);
    free_value(right);
    ray_append(stack, result);
    return true;
}

static bool apply_print(RDNState *stack, Vars *vars) {
    Value *value = NULL;

    if (stack->count < 1) {
        return diagnostic_error_current("print requires 1 operand");
    }

    value = resolve_value_if_var(vars, ray_pop(stack), "print");
    if (value == NULL) {
        return false;
    }
    print_value(value);
    free_value(value);
    return true;
}

static bool apply_exit(RDNState *stack , Vars *vars, int* exit_status) {

    if (stack->count < 1) {
        return diagnostic_error_current("type requires 1 operand");
    }

    Value *value = NULL;
    value = resolve_value_if_var(vars, ray_pop(stack), "exit");
    if (value == NULL) {
        return false;
    }

    char* ret = exit_value(value, exit_status);

    free_value(value);

    if (ret){
        diagnostic_error_current("%s", ret);
        return false;
    }
    return true;
}

static bool apply_stack_size(RDNState *stack){
    Value* size = create_integer_value((long)stack->count);
    return push_value(stack, size);
}

static bool apply_func_name(RDNState *stack, Funcs *funcs){
    Value *fn = NULL;
    Value *result = NULL;
    Funcs_t *entry = NULL;

    if (stack->count < 1) {
        return diagnostic_error_current("__func_name requires 1 function operand");
    }

    fn = ray_pop(stack);
    if (fn->type != VALUE_AS_VAR) {
        diagnostic_error_current("__func_name requires function name");
        ray_append(stack, fn);
        return false;
    }

    entry = find_func_entry(funcs, fn->as.string);
    if (entry == NULL) {
        diagnostic_error_current("unknown function: %s", fn->as.string);
        ray_append(stack, fn);
        return false;
    }

    result = create_string_value_copy(entry->func_name);
    free_value(fn);
    return push_value(stack, result);
}

static bool apply_type(RDNState *stack, Vars *vars, Funcs *funcs) {
    Value *value = NULL;
    Value *result = NULL;
    Vars_t *var_entry = NULL;

    if (stack->count < 1) {
        return diagnostic_error_current("type requires 1 operand");
    }

    value = ray_pop(stack);
    if (value->type == VALUE_AS_VAR) {
        var_entry = find_var_entry(vars, value->as.string);
        if (var_entry == NULL && find_func_entry(funcs, value->as.string) != NULL) {
            result = create_string_value_copy("function");
            free_value(value);
            value = NULL;
        } else {
            value = resolve_value_if_var(vars, value, "type");
            if (value == NULL) {
                return false;
            }
        }
    }

    if (result == NULL) {
        if (value->type == VALUE_NULL) {
            result = create_string_value_copy("null");
        } else
        if (value->type == VALUE_INTEGER) {
            result = create_string_value_copy("integer");
        } else if (value->type == VALUE_DOUBLE) {
            result = create_string_value_copy("double");
        } else if (value->type == VALUE_BOOLEAN) {
            result = create_string_value_copy("boolean");
        } else if (value->type == VALUE_LIST) {
            result = create_string_value_copy("list");
        } else {
            result = create_string_value_copy("string");
        }
    }

    if (value != NULL) {
        free_value(value);
    }

    if (result == NULL) {
        fprintf(stderr, "failed to allocate type result\n");
        return false;
    }

    ray_append(stack, result);
    return true;
}

static bool apply_swap(RDNState *stack) {
    if (stack->count < 2) {
        return diagnostic_error_current("swap requires 2 operands");
    }
    Value *value1 = NULL;
    Value *value2 = NULL;
    value1 = ray_pop(stack);
    value2 = ray_pop(stack);
    ray_append(stack, value1);
    ray_append(stack, value2);
    return true;
}

static bool apply_pop(RDNState *stack) {
    if (stack->count < 1) {
        return diagnostic_error_current("pop requires 1 operand");
    }

    Value *value = NULL;
    value = ray_pop(stack);
    free_value(value);
    return true;
}

static bool apply_dup(RDNState *stack, Vars *vars) {
    if (stack->count < 1) {
        return diagnostic_error_current("dup requires 1 operand");
    }

    Value *value = NULL;
    value = resolve_value_if_var(vars, ray_pop(stack), "dup");
    if (value == NULL) {
        return false;
    }

    Value* dup1;
    Value* dup2;

    switch (value->type) {
        case VALUE_NULL:{
            dup1 = create_null_value();
            dup2 = create_null_value();
        }break;
        case VALUE_BOOLEAN:{
            dup1 = create_boolean_value(value->as.boolean);
            dup2 = create_boolean_value(value->as.boolean);
        }break;
        case VALUE_DOUBLE:{
            dup1 = create_double_value(value->as.number);
            dup2 = create_double_value(value->as.number);
        }break;
        case VALUE_INTEGER:{
            dup1 = create_integer_value(value->as.integer);
            dup2 = create_integer_value(value->as.integer);
        }break;
        case VALUE_STRING:{
            dup1 = create_string_value_copy(value->as.string);
            dup2 = create_string_value_copy(value->as.string);
        }break;
        case VALUE_AS_VAR:{
            dup1 = create_var_name_value(value->as.string);
            dup2 = create_var_name_value(value->as.string);
        }break;
        case VALUE_LIST:{
            dup1 = clone_value(value);
            dup2 = clone_value(value);
        }break;
        default: {
            diagnostic_error_current("dup requires 1 operand");
            free_value(value);
            return false;
        }
    }

    ray_append(stack, dup1);
    ray_append(stack, dup2);
    free_value(value);
    return true;
}

// to_string builtin function convert value from the top stack to string without remove it
static bool apply_to_string(RDNState *stack, Vars *vars) {
    if (stack->count < 1) {
        return diagnostic_error_current("to_string requires 1 operand");
    }

    Value *value = NULL;
    value = resolve_value_if_var(vars, ray_pop(stack), "to_string");
    if (value == NULL) {
        return false;
    }

    Value* converted;

    switch (value->type) {
        case VALUE_NULL:{
            converted = create_string_value_copy("null");
        }break;
        case VALUE_BOOLEAN:{
            if (value->as.boolean) {
                converted = create_string_value_copy("true");
            }else {
                converted = create_string_value_copy("false");
            }
        }break;
        case VALUE_DOUBLE:{
            char *forStore = malloc(16);
            snprintf(forStore, 16, "%lf", value->as.number);
            converted = create_string_value_owned(forStore);
        }break;
        case VALUE_INTEGER:{
            char *forStore = malloc(16);
            snprintf(forStore, 16, "%ld", value->as.integer);
            converted = create_string_value_owned(forStore);
        }break;
        case VALUE_STRING:{
            converted = create_string_value_copy(value->as.string);
        }break;
        case VALUE_AS_VAR:{
            converted = create_string_value_copy(value->as.string);
        }break;
        case VALUE_LIST:{
            char *buffer = copy_string("(");
            size_t length = 1;

            if (buffer == NULL) {
                free_value(value);
                return false;
            }

            buffer[0] = '\0';
            length = 0;
            if (!append_value_repr(&buffer, &length, value)) {
                free(buffer);
                free_value(value);
                return false;
            }
            converted = create_string_value_owned(buffer);
        }break;
        default: {
            diagnostic_error_current("to_string requires 1 operand");
            free_value(value);
            return false;
        }
    }

    ray_append(stack, value);
    ray_append(stack, converted);
    return true;
}

static bool append_string_repr(char **target_string, const Value *value) {
    char *buffer = NULL;
    size_t length = 0;

    if (target_string == NULL || *target_string == NULL) {
        return false;
    }

    buffer = copy_string(*target_string);
    if (buffer == NULL) {
        return false;
    }

    length = strlen(buffer);
    if (!append_value_repr(&buffer, &length, value)) {
        free(buffer);
        return false;
    }

    free(*target_string);
    *target_string = buffer;
    return true;
}

static Value *create_string_char_value(char ch) {
    char *buffer = malloc(2);

    if (buffer == NULL) {
        return NULL;
    }

    buffer[0] = ch;
    buffer[1] = '\0';
    return create_string_value_owned(buffer);
}

static bool apply_append(RDNState *stack, Vars *vars) {
    Value *item = NULL;
    Value *target = NULL;
    Vars_t *entry = NULL;
    Value *item_copy = NULL;

    if (stack->count < 2) {
        return diagnostic_error_current("append requires 2 operands");
    }

    item = ray_pop(stack);
    target = ray_pop(stack);

    if (target->type == VALUE_AS_VAR && (entry = find_var_entry(vars, target->as.string)) != NULL) {
        if (entry->var_value->type == VALUE_LIST) {
            item_copy = clone_value(item);
            if (item_copy == NULL) {
                fprintf(stderr, "failed to clone appended item\n");
                ray_append(stack, target);
                ray_append(stack, item);
                return false;
            }

            ray_append(&entry->var_value->as.list, item_copy);
            free_value(target);
            free_value(item);
            return true;
        }

        if (entry->var_value->type == VALUE_STRING) {
            if (!append_string_repr(&entry->var_value->as.string, item)) {
                fprintf(stderr, "failed to append string value\n");
                ray_append(stack, target);
                ray_append(stack, item);
                return false;
            }

            free_value(target);
            free_value(item);
            return true;
        }

        diagnostic_error_current("append requires list or string target");
        ray_append(stack, target);
        ray_append(stack, item);
        return false;
    }

    target = resolve_value_if_var(vars, target, "append");
    if (target == NULL) {
        free_value(item);
        return false;
    }

    if (target->type == VALUE_LIST) {
        item_copy = clone_value(item);
        if (item_copy == NULL) {
            fprintf(stderr, "failed to clone appended item\n");
            ray_append(stack, target);
            ray_append(stack, item);
            return false;
        }

        ray_append(&target->as.list, item_copy);
        free_value(item);
        ray_append(stack, target);
        return true;
    }

    if (target->type == VALUE_STRING) {
        if (!append_string_repr(&target->as.string, item)) {
            fprintf(stderr, "failed to append string value\n");
            ray_append(stack, target);
            ray_append(stack, item);
            return false;
        }

        free_value(item);
        ray_append(stack, target);
        return true;
    }

    diagnostic_error_current("append requires list or string target");
    ray_append(stack, target);
    ray_append(stack, item);
    return false;
}

static bool apply_index(RDNState *stack, Vars *vars) {
    Value *index_value = NULL;
    Value *target = NULL;
    Vars_t *entry = NULL;
    Value *resolved_target = NULL;
    long index = 0;

    if (stack->count < 2) {
        return diagnostic_error_current("index requires 2 operands");
    }

    index_value = resolve_value_if_var(vars, ray_pop(stack), "index");
    if (index_value == NULL) {
        return false;
    }
    target = ray_pop(stack);

    if (!value_to_long(index_value, &index)) {
        diagnostic_error_current("index requires integer index");
        ray_append(stack, target);
        ray_append(stack, index_value);
        return false;
    }

    if (target->type == VALUE_AS_VAR && (entry = find_var_entry(vars, target->as.string)) != NULL) {
        resolved_target = entry->var_value;
    } else {
        target = resolve_value_if_var(vars, target, "index");
        if (target == NULL) {
            free_value(index_value);
            return false;
        }
        resolved_target = target;
    }

    if (resolved_target->type == VALUE_LIST) {
        Value *result = NULL;

        if (index < 0 || (size_t)index >= resolved_target->as.list.count) {
            diagnostic_error_current("index out of range");
            ray_append(stack, target);
            ray_append(stack, index_value);
            return false;
        }

        result = clone_value(resolved_target->as.list.items[index]);
        free_value(index_value);
        free_value(target);

        if (result == NULL) {
            return diagnostic_error_current("failed to clone indexed value");
        }

        ray_append(stack, result);
        return true;
    }

    if (resolved_target->type == VALUE_STRING) {
        Value *result = NULL;
        size_t length = strlen(resolved_target->as.string);

        if (index < 0 || (size_t)index >= length) {
            diagnostic_error_current("index out of range");
            ray_append(stack, target);
            ray_append(stack, index_value);
            return false;
        }

        result = create_string_char_value(resolved_target->as.string[index]);
        free_value(index_value);
        free_value(target);

        if (result == NULL) {
            return diagnostic_error_current("failed to create indexed string value");
        }

        ray_append(stack, result);
        return true;
    }

    return diagnostic_error_current("index requires list or string target");
}

static bool apply_remove(RDNState *stack, Vars *vars) {
    Value *index_value = NULL;
    Value *target = NULL;
    Vars_t *entry = NULL;
    Value *resolved_target = NULL;
    long index = 0;
    size_t i = 0;

    if (stack->count < 2) {
        return diagnostic_error_current("remove requires 2 operands");
    }

    index_value = resolve_value_if_var(vars, ray_pop(stack), "remove");
    if (index_value == NULL) {
        return false;
    }
    target = ray_pop(stack);

    if (!value_to_long(index_value, &index)) {
        diagnostic_error_current("remove requires integer index");
        ray_append(stack, target);
        ray_append(stack, index_value);
        return false;
    }

    if (target->type == VALUE_AS_VAR && (entry = find_var_entry(vars, target->as.string)) != NULL) {
        resolved_target = entry->var_value;
    } else {
        target = resolve_value_if_var(vars, target, "remove");
        if (target == NULL) {
            free_value(index_value);
            return false;
        }
        resolved_target = target;
    }

    if (resolved_target->type == VALUE_LIST) {
        if (index < 0 || (size_t)index >= resolved_target->as.list.count) {
            diagnostic_error_current("remove index out of range");
            ray_append(stack, target);
            ray_append(stack, index_value);
            return false;
        }

        free_value(resolved_target->as.list.items[index]);
        for (i = (size_t)index + 1; i < resolved_target->as.list.count; i++) {
            resolved_target->as.list.items[i - 1] = resolved_target->as.list.items[i];
        }
        resolved_target->as.list.count--;

        free_value(index_value);
        if (resolved_target == target) {
            ray_append(stack, target);
        } else {
            free_value(target);
        }
        return true;
    }

    if (resolved_target->type == VALUE_STRING) {
        size_t length = strlen(resolved_target->as.string);

        if (index < 0 || (size_t)index >= length) {
            diagnostic_error_current("remove index out of range");
            ray_append(stack, target);
            ray_append(stack, index_value);
            return false;
        }

        memmove(&resolved_target->as.string[index],
                &resolved_target->as.string[index + 1],
                length - (size_t)index);

        free_value(index_value);
        if (resolved_target == target) {
            ray_append(stack, target);
        } else {
            free_value(target);
        }
        return true;
    }

    diagnostic_error_current("remove requires list or string target");
    ray_append(stack, target);
    ray_append(stack, index_value);
    return false;
}

static bool apply_len(RDNState *stack, Vars *vars) {
    Value *target = NULL;
    Vars_t *entry = NULL;
    Value *resolved_target = NULL;
    Value *result = NULL;

    if (stack->count < 1) {
        return diagnostic_error_current("len requires 1 operand");
    }

    target = ray_pop(stack);
    if (target->type == VALUE_AS_VAR && (entry = find_var_entry(vars, target->as.string)) != NULL) {
        resolved_target = entry->var_value;
    } else {
        target = resolve_value_if_var(vars, target, "len");
        if (target == NULL) {
            return false;
        }
        resolved_target = target;
    }

    if (resolved_target->type == VALUE_LIST) {
        result = create_integer_value((long)resolved_target->as.list.count);
    } else if (resolved_target->type == VALUE_STRING) {
        result = create_integer_value((long)strlen(resolved_target->as.string));
    } else {
        diagnostic_error_current("len requires list or string target");
        ray_append(stack, target);
        return false;
    }

    free_value(target);

    if (result == NULL) {
        fprintf(stderr, "failed to create len result\n");
        return false;
    }

    ray_append(stack, result);
    return true;
}

// TODO: rayden was here
static bool apply_add_load_path(RDNState *stack, Vars *vars) {
    Value *target = NULL;
    char *path = NULL;
    char *resolved = NULL;
    char *canonical = NULL;
    bool ok = false;

    if (!pop_string_path_operand(stack, vars, "add_load_path", &target, &path)) {
        return false;
    }

    resolved = resolve_path_from_current_source(path);
    if (resolved == NULL) {
        free_value(target);
        return diagnostic_error_current("failed to resolve search path '%s'", path);
    }

    canonical = canonicalize_existing_path(resolved);
    if (canonical != NULL) {
        free(resolved);
        resolved = canonical;
    }

    ok = push_search_path(&g_script_search_paths, resolved);
    free(resolved);
    free_value(target);

    if (!ok) {
        return diagnostic_error_current("failed to add load search path");
    }

    return true;
}

static bool apply_add_native_path(RDNState *stack, Vars *vars) {
    Value *target = NULL;
    char *path = NULL;
    char *resolved = NULL;
    char *canonical = NULL;
    bool ok = false;

    if (!pop_string_path_operand(stack, vars, "add_native_path", &target, &path)) {
        return false;
    }

    resolved = resolve_path_from_current_source(path);
    if (resolved == NULL) {
        free_value(target);
        return diagnostic_error_current("failed to resolve native search path '%s'", path);
    }

    canonical = canonicalize_existing_path(resolved);
    if (canonical != NULL) {
        free(resolved);
        resolved = canonical;
    }

    ok = push_search_path(&g_native_search_paths, resolved);
    free(resolved);
    free_value(target);

    if (!ok) {
        return diagnostic_error_current("failed to add native search path");
    }

    return true;
}

static bool apply_load(RDNState *stack, Vars *vars, Funcs *funcs){
    char *source = NULL;
    char *resolved_path = NULL;
    char *canonical_path = NULL;
    char *canonical_current_path = NULL;
    char *path = NULL;
    char *path_copy = NULL;
    Value *target = NULL;
    bool ok = false;

    if (!pop_string_path_operand(stack, vars, "load", &target, &path)) {
        return false;
    }

    if (strstr(path, ".rdn") == NULL){
        char* buffer = malloc(256);
        sprintf(buffer, "%s.rdn", path);
        path_copy = copy_string(buffer);
        free(buffer);
    }else {
        path_copy = copy_string(path);
    }

    if (path_copy == NULL) {
        free_value(target);
        return diagnostic_error_current("failed to allocate load path");
    }

    resolved_path = resolve_load_path_candidate(path_copy, &g_script_search_paths);
    if (resolved_path == NULL) {
        free_value(target);
        ok = diagnostic_error_current("failed to resolve path '%s'", path_copy);
        free(path_copy);
        return ok;
    }

    canonical_path = canonicalize_existing_path(resolved_path);
    if (canonical_path != NULL) {
        free(resolved_path);
        resolved_path = canonical_path;
    }

    canonical_current_path = canonicalize_existing_path(g_current_source_path);
    if (canonical_current_path != NULL && strcmp(canonical_current_path, resolved_path) == 0) {
        free(canonical_current_path);
        free_value(target);
        ok = diagnostic_error_current("recursive load detected for '%s'", resolved_path);
        free(resolved_path);
        free(path_copy);
        return ok;
    }
    free(canonical_current_path);

    if (load_path_stack_contains(resolved_path)) {
        free_value(target);
        ok = diagnostic_error_current("recursive load detected for '%s'", resolved_path);
        free(resolved_path);
        free(path_copy);
        return ok;
    }

    source = read_file(resolved_path);
    if (source == NULL) {
        free(resolved_path);
        free_value(target);
        free(path_copy);
        return false;
    }

    if (!push_load_path(resolved_path)) {
        free(source);
        free(resolved_path);
        free_value(target);
        ok = diagnostic_error_current("failed to track loaded path '%s'", path_copy);
        free(path_copy);
        return ok;
    }

    {
        const char *previous_path = g_current_source_path;
        g_current_source_path = resolved_path;
        ok = evaluate_source(stack, vars, funcs, source);
        g_current_source_path = previous_path;
    }

    pop_load_path();
    free(source);
    free(resolved_path);
    free_value(target);
    free(path_copy);
    return ok;
}

static bool apply_loadnative(RDNState *stack, Vars *vars, Funcs *funcs) {
    char *resolved_path = NULL;
    char *canonical_path = NULL;
    char *path = NULL;
    char *path_copy = NULL;
    Value *target = NULL;
    void *handle = NULL;
    RDNModuleInit init_function = NULL;
    RDNModule module = {0};
    NativeModuleLoadState module_state = {0};
    bool ok = false;

    if (!pop_string_path_operand(stack, vars, "loadnative", &target, &path)) {
        return false;
    }

    // TODO: handle the extension for other platforms (dll , ...)
    if (strstr(path, ".so") == NULL){
        char* buffer = malloc(256);
        sprintf(buffer, "%s.so", path);
        path_copy = copy_string(buffer);
        free(buffer);
    }else {
        path_copy = copy_string(path);
    }

    if (path_copy == NULL) {
        free_value(target);
        return diagnostic_error_current("failed to allocate native load path");
    }

    resolved_path = resolve_load_path_candidate(path_copy, &g_native_search_paths);
    if (resolved_path == NULL) {
        free_value(target);
        ok = diagnostic_error_current("failed to resolve path '%s'", path_copy);
        free(path_copy);
        return ok;
    }

    canonical_path = canonicalize_existing_path(resolved_path);
    if (canonical_path != NULL) {
        free(resolved_path);
        resolved_path = canonical_path;
    }

    handle = dlopen(resolved_path, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        char *diagnostic_path = copy_string(resolved_path);
        free(resolved_path);
        free_value(target);
        free(path_copy);
        if (diagnostic_path == NULL) {
            return diagnostic_error_current("failed to load native module: %s", dlerror());
        }
        {
            bool ok = diagnostic_error_current("failed to load native module '%s': %s", diagnostic_path, dlerror());
            free(diagnostic_path);
            return ok;
        }
    }

    init_function = (RDNModuleInit)dlsym(handle, "rdn_module_init");
    if (init_function == NULL) {
        char *diagnostic_path = copy_string(resolved_path);
        dlclose(handle);
        free(resolved_path);
        free_value(target);
        free(path_copy);
        if (diagnostic_path == NULL) {
            return diagnostic_error_current("native module is missing rdn_module_init: %s", dlerror());
        }
        {
            bool ok = diagnostic_error_current("native module '%s' is missing rdn_module_init: %s", diagnostic_path, dlerror());
            free(diagnostic_path);
            return ok;
        }
    }

    module_state.regs.items = NULL;
    module_state.regs.count = 0;
    module_state.regs.capacity = 0;
    module_state.error_message = NULL;

    module.userdata = &module_state;
    module.register_function = native_module_register_function;
    module.set_error = native_module_set_error;

    ok = init_function(&module);
    if (!ok) {
        diagnostic_error_current("%s", module_state.error_message == NULL ? "native module initialization failed" : module_state.error_message);
        free_native_module_regs(&module_state.regs);
        free(module_state.error_message);
        dlclose(handle);
        free(resolved_path);
        free_value(target);
        free(path_copy);
        return false;
    }

    for (size_t index = 0; index < module_state.regs.count; index++) {
        NativeModuleReg *reg = module_state.regs.items[index];
        if (!funcs_define_native(funcs, reg->name, reg->function, handle)) {
            free_native_module_regs(&module_state.regs);
            free(module_state.error_message);
            dlclose(handle);
            free(resolved_path);
            free_value(target);
            free(path_copy);
            return false;
        }
    }

    free_native_module_regs(&module_state.regs);
    free(module_state.error_message);
    free(resolved_path);
    free_value(target);
    free(path_copy);
    return true;
}

static bool typecheck_signature_types_valid(const Value *types) {
    size_t index = 0;

    if (types->type != VALUE_LIST) {
        return false;
    }

    for (index = 0; index < types->as.list.count; index++) {
        const Value *item = types->as.list.items[index];

        if (item->type != VALUE_INTEGER || item->as.integer < 0 || item->as.integer > RDN_TYPECHECK_ANY_CODE) {
            return false;
        }
    }

    return true;
}

static bool append_typecheck_signature(Vars *vars, const char *name, const Value *params, const Value *returns) {
    Vars_t *state_entry = find_var_entry(vars, "State");
    Value *signature = NULL;
    Value *name_value = NULL;
    Value *params_copy = NULL;
    Value *returns_copy = NULL;

    if (state_entry == NULL || state_entry->var_value->type != VALUE_LIST) {
        return diagnostic_error_current("signed defun requires typecheck State list; load libs/typecheck.rdn first");
    }

    signature = create_list_value();
    name_value = create_string_value_copy(name);
    params_copy = clone_value(params);
    returns_copy = clone_value(returns);

    if (signature == NULL || name_value == NULL || params_copy == NULL || returns_copy == NULL) {
        free_value(signature);
        free_value(name_value);
        free_value(params_copy);
        free_value(returns_copy);
        return diagnostic_error_current("failed to allocate typecheck signature");
    }

    ray_append(&signature->as.list, name_value);
    ray_append(&signature->as.list, params_copy);
    ray_append(&signature->as.list, returns_copy);
    ray_append(&state_entry->var_value->as.list, signature);
    return true;
}

static bool apply_defun(RDNState *stack, Vars *vars, Funcs *funcs, char **cursor) {
    Value *name = NULL;
    Value *params = NULL;
    Value *returns = NULL;
    char *body = NULL;
    char *body_start = *cursor;
    char *scan = *cursor;
    char *token = NULL;
    bool is_string = false;
    int depth = 1;
    bool signed_defun = false;
    size_t source_line = 1;
    size_t source_column = 1;

    if (stack->count < 1) {
        return diagnostic_error_current("defun requires function name");
    }

    if (stack->count >= 3 &&
        stack->items[stack->count - 1]->type == VALUE_LIST &&
        stack->items[stack->count - 2]->type == VALUE_LIST &&
        stack->items[stack->count - 3]->type == VALUE_AS_VAR) {
        signed_defun = true;
        returns = ray_pop(stack);
        params = ray_pop(stack);
    }

    name = ray_pop(stack);
    if (name->type != VALUE_AS_VAR) {
        diagnostic_error_current("defun requires function name");
        ray_append(stack, name);
        if (signed_defun) {
            ray_append(stack, params);
            ray_append(stack, returns);
        }
        return false;
    }

    if (signed_defun && (!typecheck_signature_types_valid(params) || !typecheck_signature_types_valid(returns))) {
        diagnostic_error_current("defun signature contains an unknown type");
        ray_append(stack, name);
        ray_append(stack, params);
        ray_append(stack, returns);
        return false;
    }

    diagnostic_compute_location(body_start, &source_line, &source_column, NULL, NULL);

    while (true) {
        char *token_start = scan;

        if (!next_token(&scan, &token, &is_string)) {
            free_value(name);
            free_value(params);
            free_value(returns);
            return false;
        }

        if (token == NULL) {
            diagnostic_error_at(scan, "defun missing end");
            free_value(name);
            free_value(params);
            free_value(returns);
            return false;
        }

        if (!is_string && is_token(token, "demac")) {
            if (!skip_demac(&scan)) {
                free(token);
                free_value(name);
                free_value(params);
                free_value(returns);
                return false;
            }
        } else if (!is_string && (is_token(token, "if") || is_token(token, "loop") || is_token(token, "defun") ||
                                  is_token(token, "apply"))) {
            depth++;
        } else if (!is_string && is_token(token, "end")) {
            depth--;
            if (depth == 0) {
                size_t body_length = (size_t)(token_start - body_start);
                body = malloc(body_length + 1);
                if (body == NULL) {
                    diagnostic_error_at(token_start, "failed to allocate function body");
                    free(token);
                    free_value(name);
                    free_value(params);
                    free_value(returns);
                    return false;
                }

                memcpy(body, body_start, body_length);
                body[body_length] = '\0';

                if (!funcs_define(funcs, name->as.string, body, g_current_source_path, source_line, source_column)) {
                    free(token);
                    free_value(name);
                    free_value(params);
                    free_value(returns);
                    return false;
                }

                if (signed_defun && !append_typecheck_signature(vars, name->as.string, params, returns)) {
                    free(token);
                    free_value(name);
                    free_value(params);
                    free_value(returns);
                    return false;
                }

                free(token);
                free_value(name);
                free_value(params);
                free_value(returns);
                *cursor = scan;
                return true;
            }
        }

        free(token);
    }
}

static bool apply_apply(RDNState *stack, Funcs *funcs, char **cursor) {
    Value *name = NULL;
    char *body = NULL;
    char *body_start = *cursor;
    char *scan = *cursor;
    char *token = NULL;
    bool is_string = false;
    int depth = 1;
    size_t source_line = 1;
    size_t source_column = 1;

    if (stack->count < 1) {
        return diagnostic_error_current("apply requires function name");
    }

    name = ray_pop(stack);
    if (name->type != VALUE_AS_VAR) {
        diagnostic_error_current("apply requires function name");
        ray_append(stack, name);
        return false;
    }

    diagnostic_compute_location(body_start, &source_line, &source_column, NULL, NULL);

    while (true) {
        char *token_start = scan;

        if (!next_token(&scan, &token, &is_string)) {
            free_value(name);
            return false;
        }

        if (token == NULL) {
            diagnostic_error_at(scan, "apply missing end");
            free_value(name);
            return false;
        }

        if (!is_string && is_token(token, "demac")) {
            if (!skip_demac(&scan)) {
                free(token);
                free_value(name);
                return false;
            }
        } else if (!is_string && (is_token(token, "if") || is_token(token, "loop") || is_token(token, "defun") ||
                                  is_token(token, "apply"))) {
            depth++;
        } else if (!is_string && is_token(token, "end")) {
            depth--;
            if (depth == 0) {
                size_t body_length = (size_t)(token_start - body_start);
                body = malloc(body_length + 1);
                if (body == NULL) {
                    diagnostic_error_at(token_start, "failed to allocate apply body");
                    free(token);
                    free_value(name);
                    return false;
                }

                memcpy(body, body_start, body_length);
                body[body_length] = '\0';

                if (!funcs_define_apply(funcs, name->as.string, body, g_current_source_path, source_line, source_column)) {
                    free(token);
                    free_value(name);
                    return false;
                }

                free(token);
                free_value(name);
                *cursor = scan;
                return true;
            }
        }

        free(token);
    }
}

static bool apply_demac(RDNState *stack, Funcs *funcs, char **cursor) {
    Value *name = NULL;
    char *body = NULL;
    char *body_start = *cursor;
    char *scan = *cursor;
    char *token = NULL;
    bool is_string = false;
    size_t source_line = 1;
    size_t source_column = 1;

    if (stack->count < 1) {
        return diagnostic_error_current("demac requires macro name");
    }

    name = ray_pop(stack);
    if (name->type != VALUE_AS_VAR) {
        diagnostic_error_current("demac requires macro name");
        ray_append(stack, name);
        return false;
    }

    diagnostic_compute_location(body_start, &source_line, &source_column, NULL, NULL);

    while (true) {
        char *token_start = scan;

        if (!next_token(&scan, &token, &is_string)) {
            free_value(name);
            return false;
        }

        if (token == NULL) {
            diagnostic_error_at(scan, "demac missing end");
            free_value(name);
            return false;
        }

        if (!is_string && is_token(token, "end")) {
            size_t body_length = (size_t)(token_start - body_start);
            body = malloc(body_length + 1);
            if (body == NULL) {
                diagnostic_error_at(token_start, "failed to allocate macro body");
                free(token);
                free_value(name);
                return false;
            }

            memcpy(body, body_start, body_length);
            body[body_length] = '\0';

            if (!funcs_define_demac(funcs, name->as.string, body, g_current_source_path, source_line, source_column)) {
                free(token);
                free_value(name);
                return false;
            }

            free(token);
            free_value(name);
            *cursor = scan;
            return true;
        }

        free(token);
    }
}

static bool expand_demac(Funcs_t *entry, char **cursor) {
    size_t body_length = strlen(entry->as.func_body);
    size_t rest_length = strlen(*cursor);
    char *expanded = malloc(body_length + rest_length + 1);

    if (expanded == NULL) {
        return diagnostic_error_current("failed to allocate macro expansion");
    }

    memcpy(expanded, entry->as.func_body, body_length);
    memcpy(expanded + body_length, *cursor, rest_length + 1);

    ray_append(&g_macro_expansions, expanded);
    *cursor = expanded;
    return true;
}

static void free_macro_expansion_stack(MacroExpansionStack *expansions) {
    while (expansions->count > 0) {
        free(ray_pop(expansions));
    }

    ray_clear(expansions);
}

static void free_macro_expansions(void) {
    free_macro_expansion_stack(&g_macro_expansions);
    g_macro_expansions = (MacroExpansionStack){0};
}

static bool execute_named_entry(RDNState *stack, Vars *vars, Funcs *funcs, Funcs_t *entry, const char *context_kind, const char *context_name) {
    BlockStop stop_reason = BLOCK_STOP_EOF;
    char *cursor = NULL;

    if (entry->type == FUNC_NATIVE) {
        RDNApi api = {0};
        NativeCallState call_state = {0};
        bool ok = false;

        call_state.stack = stack;
        call_state.vars = vars;
        call_state.error_message = NULL;

        api.userdata = &call_state;
        api.stack_size = native_api_stack_size;
        api.type = native_api_type;
        api.is_number = native_api_is_number;
        api.to_integer = native_api_to_integer;
        api.to_number = native_api_to_number;
        api.to_boolean = native_api_to_boolean;
        api.to_string = native_api_to_string;
        api.to_identifier = native_api_to_identifier;
        api.pop = native_api_pop;
        api.push_null = native_api_push_null;
        api.push_integer = native_api_push_integer;
        api.push_number = native_api_push_number;
        api.push_boolean = native_api_push_boolean;
        api.push_string = native_api_push_string;
        api.push_list = native_api_push_list;
        api.list_len = native_api_list_len;
        api.list_append = native_api_list_append;
        api.list_index = native_api_list_index;
        api.list_remove = native_api_list_remove;
        api.raise_error = native_api_raise_error;

        ok = entry->as.native_function(&api);
        if (!ok) {
            diagnostic_error_current("%s", call_state.error_message == NULL ? "native function call failed" : call_state.error_message);
            free(call_state.error_message);
            return false;
        }

        free(call_state.error_message);
        return true;
    }

    if (!vars_push_scope(vars)) {
        return false;
    }

    cursor = entry->as.func_body;
    {
        DiagnosticContext previous_context = g_diagnostic_context;
        diagnostic_set_source(entry->source_path, entry->as.func_body, entry->source_line, entry->source_column);
        if (!execute_block(stack, vars, funcs, &cursor, &stop_reason, false)) {
            g_diagnostic_context = previous_context;
            diagnostic_note_current("while %s '%s'", context_kind, context_name);
            vars_pop_scope(vars);
            return false;
        }
        g_diagnostic_context = previous_context;
    }

    if (!materialize_scope_references(stack, vars)) {
        vars_pop_scope(vars);
        return false;
    }

    vars_pop_scope(vars);

    if (stop_reason == BLOCK_STOP_BREAK) {
        return diagnostic_error_current("unexpected break");
    }

    if (stop_reason == BLOCK_STOP_CONTINUE) {
        return diagnostic_error_current("unexpected continue");
    }

    if (stop_reason != BLOCK_STOP_END && stop_reason != BLOCK_STOP_EOF && stop_reason != BLOCK_STOP_RETURN) {
        return diagnostic_error_current("function body terminated unexpectedly");
    }

    return true;
}

static bool apply_pcall(RDNState *stack, Vars *vars, Funcs *funcs) {
    Value *name = NULL;
    Value *resolved_name = NULL;
    Vars_t *var_entry = NULL;
    Funcs_t *entry = NULL;
    bool ok = false;
    size_t saved_trace_count = 0;
    size_t saved_stack_count = 0;
    DiagnosticContext saved_context = {0};
    bool was_suppressed = false;

    if (stack->count < 1) {
        return diagnostic_error_current("pcall requires function name");
    }

    name = ray_pop(stack);
    if (name->type != VALUE_AS_VAR) {
        diagnostic_error_current("pcall requires function name");
        ray_append(stack, name);
        return false;
    }

    var_entry = find_var_entry(vars, name->as.string);
    if (var_entry != NULL) {
        resolved_name = clone_value(var_entry->var_value);
        if (resolved_name == NULL) {
            diagnostic_error_current("failed to resolve function variable '%s'", name->as.string);
            free_value(name);
            return false;
        }

        if (resolved_name->type != VALUE_AS_VAR) {
            diagnostic_error_current("pcall requires function name");
            free_value(name);
            ray_append(stack, resolved_name);
            return false;
        }

        entry = find_func_entry(funcs, resolved_name->as.string);
    } else {
        entry = find_func_entry(funcs, name->as.string);
    }

    if (entry == NULL) {
        diagnostic_error_current("unknown function: %s",
                                 resolved_name != NULL ? resolved_name->as.string : name->as.string);
        free_value(resolved_name);
        ray_append(stack, name);
        return false;
    }

    saved_trace_count = g_stack_trace_protected.count;
    saved_stack_count = stack->count;
    saved_context = g_diagnostic_context;
    was_suppressed = g_diagnostics_suppressed;
    g_diagnostics_suppressed = true;

    ok = execute_named_entry(stack, vars, funcs, entry, "calling function", entry->func_name);

    if (ok) {
        g_diagnostics_suppressed = was_suppressed;
        if (!push_value(stack, create_boolean_value(true))) {
            free_value(resolved_name);
            free_value(name);
            return false;
        }
    } else {
        char *error_text = NULL;
        size_t error_length = 0;
        size_t i = 0;

        for (i = saved_trace_count; i < g_stack_trace_protected.count; i++) {
            if (i > saved_trace_count) {
                append_text(&error_text, &error_length, "\n");
            }
            append_text(&error_text, &error_length, g_stack_trace_protected.items[i]);
            free(g_stack_trace_protected.items[i]);
        }
        g_stack_trace_protected.count = saved_trace_count;

        g_diagnostic_context = saved_context;
        g_diagnostics_suppressed = was_suppressed;

        while (stack->count > saved_stack_count) {
            free_value(ray_pop(stack));
        }

        if (error_text != NULL && error_length > 0) {
            if (!push_value(stack, create_string_value_owned(error_text))) {
                free(error_text);
                free_value(resolved_name);
                free_value(name);
                return false;
            }
        } else {
            if (!push_value(stack, create_string_value_copy("pcall caught an error"))) {
                free_value(resolved_name);
                free_value(name);
                return false;
            }
        }

        if (!push_value(stack, create_boolean_value(false))) {
            free_value(resolved_name);
            free_value(name);
            return false;
        }
    }

    free_value(resolved_name);
    free_value(name);
    return true;
}

static bool apply_call(RDNState *stack, Vars *vars, Funcs *funcs) {
    Value *name = NULL;
    Value *resolved_name = NULL;
    Vars_t *var_entry = NULL;
    Funcs_t *entry = NULL;
    bool ok = false;

    if (stack->count < 1) {
        return diagnostic_error_current("call requires function name");
    }

    name = ray_pop(stack);
    if (name->type != VALUE_AS_VAR) {
        diagnostic_error_current("call requires function name");
        ray_append(stack, name);
        return false;
    }

    var_entry = find_var_entry(vars, name->as.string);
    if (var_entry != NULL) {
        resolved_name = clone_value(var_entry->var_value);
        if (resolved_name == NULL) {
            diagnostic_error_current("failed to resolve function variable '%s'", name->as.string);
            free_value(name);
            return false;
        }

        if (resolved_name->type != VALUE_AS_VAR) {
            diagnostic_error_current("call requires function name");
            free_value(name);
            ray_append(stack, resolved_name);
            return false;
        }

        entry = find_func_entry(funcs, resolved_name->as.string);
    } else {
        entry = find_func_entry(funcs, name->as.string);
    }

    if (entry == NULL) {
        diagnostic_error_current("unknown function: %s",
                                 resolved_name != NULL ? resolved_name->as.string : name->as.string);
        free_value(resolved_name);
        ray_append(stack, name);
        return false;
    }

    ok = execute_named_entry(stack, vars, funcs, entry, "calling function", entry->func_name);
    free_value(resolved_name);
    free_value(name);
    return ok;
}

static bool materialize_scope_references(RDNState *stack, Vars *vars) {
    size_t index = 0;

    for (index = 0; index < stack->count; index++) {
        Value *value = stack->items[index];
        Vars_t *entry = NULL;
        Value *resolved = NULL;

        if (value == NULL || value->type != VALUE_AS_VAR) {
            continue;
        }

        entry = find_current_scope_var_entry(vars, value->as.string);
        if (entry == NULL) {
            continue;
        }

        resolved = clone_value(entry->var_value);
        if (resolved == NULL) {
            return diagnostic_error_current("failed to materialize scoped value '%s'", value->as.string);
        }

        free_value(value);
        stack->items[index] = resolved;
    }

    return true;
}

static bool skip_comment(char **cursor) {
    const char *comment_start = *cursor;
    *cursor += 2;

    while (**cursor != '\0') {
        if ((*cursor)[0] == '*' && (*cursor)[1] == ']') {
            *cursor += 2;
            return true;
        }
        (*cursor)++;
    }

    return diagnostic_error_at(comment_start, "unterminated comment");
}

static bool append_char(char **buffer, size_t *length, size_t *capacity, char ch) {
    char *grown = NULL;

    if (*length + 1 >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        grown = realloc(*buffer, new_capacity);
        if (grown == NULL) {
            return false;
        }
        *buffer = grown;
        *capacity = new_capacity;
    }

    (*buffer)[(*length)++] = ch;
    return true;
}

static bool read_string_token(char **cursor, char **out_token) {
    const char *string_start = *cursor;
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    char escaped = '\0';

    (*cursor)++;

    while (**cursor != '\0') {
        if (**cursor == '"') {
            (*cursor)++;
            if (!append_char(&buffer, &length, &capacity, '\0')) {
                free(buffer);
                return false;
            }
            *out_token = buffer;
            return true;
        }

        if (**cursor == '\\') {
            (*cursor)++;
            if (**cursor == '\0') {
                break;
            }

            if (**cursor == 'n') {
                escaped = '\n';
            } else if (**cursor == 't') {
                escaped = '\t';
            } else if (**cursor == 'r') {
                escaped = '\r';
            } else if (**cursor == '\\') {
                escaped = '\\';
            } else if (**cursor == '"') {
                escaped = '"';
            } else if (**cursor == 'e') {
                escaped = '\x1b';
            } else {
                escaped = **cursor;
            }

            if (!append_char(&buffer, &length, &capacity, escaped)) {
                free(buffer);
                return false;
            }

            (*cursor)++;
            continue;
        }

        if (!append_char(&buffer, &length, &capacity, **cursor)) {
            free(buffer);
            return false;
        }

        (*cursor)++;
    }

    diagnostic_error_at(string_start, "unterminated string literal");
    free(buffer);
    return false;
}

static bool read_plain_token(char **cursor, char **out_token) {
    const char *start = *cursor;
    size_t length = 0;
    char *token = NULL;

    if (**cursor == '(' || **cursor == ')') {
        token = malloc(2);
        if (token == NULL) {
            return false;
        }
        token[0] = **cursor;
        token[1] = '\0';
        (*cursor)++;
        *out_token = token;
        return true;
    }

    while ((*cursor)[length] != '\0' && !isspace((unsigned char)(*cursor)[length])) {
        if ((*cursor)[length] == ',' || (*cursor)[length] == '(' || (*cursor)[length] == ')') {
            break;
        }
        if ((*cursor)[length] == '[' && (*cursor)[length + 1] == '*') {
            break;
        }
        length++;
    }

    token = malloc(length + 1);
    if (token == NULL) {
        return false;
    }

    memcpy(token, start, length);
    token[length] = '\0';
    *cursor += length;
    *out_token = token;
    return true;
}

static bool next_token(char **cursor, char **out_token, bool *out_is_string) {
    *out_token = NULL;
    *out_is_string = false;

    while (**cursor != '\0') {
        if (isspace((unsigned char)**cursor)) {
            (*cursor)++;
            continue;
        }

        if (**cursor == ',') {
            (*cursor)++;
            continue;
        }

        if ((*cursor)[0] == '[' && (*cursor)[1] == '*') {
            if (!skip_comment(cursor)) {
                return false;
            }
            continue;
        }

        break;
    }

    if (**cursor == '\0') {
        return true;
    }

    diagnostic_set_last_token(*cursor, *cursor);

    if (**cursor == '"') {
        *out_is_string = true;
        return read_string_token(cursor, out_token);
    }

    return read_plain_token(cursor, out_token);
}

static bool push_token_value(RDNState *stack, const char *token, bool is_string) {
    long integer_value = 0;
    double double_value = 0;

    if (is_string) {
        return push_value(stack, create_string_value_copy(token));
    }

    if (parse_integer_token(token, &integer_value)) {
        return push_value(stack, create_integer_value(integer_value));
    }

    if (parse_double_token(token, &double_value)) {
        return push_value(stack, create_double_value(double_value));
    }

    if (is_token(token, "null")) {
        return push_value(stack, create_null_value());
    }

    if (is_token(token, "true")) {
        return push_value(stack, create_boolean_value(true));
    }

    if (is_token(token, "false")) {
        return push_value(stack, create_boolean_value(false));
    }

    return diagnostic_error_current("unknown token: %s", token);
}

static bool is_value_token(const char *token, bool is_string) {
    long integer_value = 0;
    double double_value = 0;

    if (is_string) {
        return true;
    }

    if (parse_integer_token(token, &integer_value)) {
        return true;
    }

    if (parse_double_token(token, &double_value)) {
        return true;
    }

    return is_token(token, "null") || is_token(token, "true") || is_token(token, "false");
}

static bool execute_list_literal(RDNState *stack, Vars *vars, Funcs *funcs, char **cursor) {
    char *token = NULL;
    bool is_string = false;

    while (true) {
        if (!next_token(cursor, &token, &is_string)) {
            return false;
        }

        if (token == NULL) {
            fprintf(stderr, "unterminated list literal\n");
            return false;
        }

        if (!is_string && is_token(token, ")")) {
            free(token);
            return true;
        }

        if (!is_string && is_token(token, "(")) {
            Value *list_value = NULL;

            free(token);
            list_value = parse_list_literal(cursor, vars, funcs);
            if (list_value == NULL) {
                return false;
            }
            ray_append(stack, list_value);
            continue;
        } else if (is_token(token, "else")) {
            fprintf(stderr, "unexpected else\n");
            free(token);
            return false;
        } else if (is_token(token, "end")) {
            fprintf(stderr, "unexpected end\n");
            free(token);
            return false;
        } else if (is_token(token, "if")) {
            BlockStop stop_reason = BLOCK_STOP_EOF;

            free(token);
            if (!apply_if(stack, vars, funcs, cursor, &stop_reason)) {
                return false;
            }
            if (stop_reason == BLOCK_STOP_BREAK) {
                fprintf(stderr, "unexpected break\n");
                return false;
            }
            if (stop_reason == BLOCK_STOP_CONTINUE) {
                fprintf(stderr, "unexpected continue\n");
                return false;
            }
            if (stop_reason == BLOCK_STOP_RETURN) {
                fprintf(stderr, "unexpected ret\n");
                return false;
            }
            continue;
        } else if (is_token(token, "loop")) {
            BlockStop stop_reason = BLOCK_STOP_EOF;

            free(token);
            if (!apply_loop(stack, vars, funcs, cursor, &stop_reason)) {
                return false;
            }
            if (stop_reason == BLOCK_STOP_RETURN) {
                fprintf(stderr, "unexpected ret\n");
                return false;
            }
            continue;
        } else if (is_token(token, "defun")) {
            free(token);
            if (!apply_defun(stack, vars, funcs, cursor)) {
                return false;
            }
            continue;
        } else if (is_token(token, "apply")) {
            free(token);
            if (!apply_apply(stack, funcs, cursor)) {
                return false;
            }
            continue;
        } else if (is_token(token, "demac")) {
            free(token);
            if (!apply_demac(stack, funcs, cursor)) {
                return false;
            }
            continue;
        } else if (is_token(token, "call")) {
            free(token);
            if (!apply_call(stack, vars, funcs)) {
                return false;
            }
            continue;
        } else if (is_token(token, "pcall")) {
            free(token);
            if (!apply_pcall(stack, vars, funcs)) {
                return false;
            }
            continue;
        } else if (is_token(token, "break")) {
            fprintf(stderr, "unexpected break\n");
            free(token);
            return false;
        } else if (is_token(token, "continue")) {
            fprintf(stderr, "unexpected continue\n");
            free(token);
            return false;
        } else if (is_token(token, "ret")) {
            fprintf(stderr, "unexpected ret\n");
            free(token);
            return false;
        } else if (is_value_token(token, is_string)) {
            if (!push_token_value(stack, token, is_string)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_operator_token(token)) {
            if (!apply_binary_operator(stack, vars, token)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "print")) {
            if (!apply_print(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "type")) {
            if (!apply_type(stack, vars, funcs)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "__func_name")) {
            if (!apply_func_name(stack, funcs)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "__stack_size")) {
            if (!apply_stack_size(stack)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "exit")) {
            int ret = 0;

            if (!apply_exit(stack, vars, &ret)) {
                free(token);
                return false;
            }
            free(token);
            free_stack_values(stack);
            exit(ret);
        } else if (is_token(token, "pop")) {
            if (!apply_pop(stack)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "swap")) {
            if (!apply_swap(stack)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "dup")) {
            if (!apply_dup(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "let")) {
            if (!apply_let(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "set")) {
            if (!apply_set(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "enum")) {
            if (!apply_enum(stack, vars, false)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "reset")) {
            if (!apply_enum(stack, vars, true)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "const")) {
            if (!apply_const(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "to_string")) {
            if (!apply_to_string(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "append")) {
            if (!apply_append(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "remove")) {
            if (!apply_remove(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "index")) {
            if (!apply_index(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "len")) {
            if (!apply_len(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "add_load_path")) {
            if (!apply_add_load_path(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "add_native_path")) {
            if (!apply_add_native_path(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "load")) {
            if (!apply_load(stack, vars, funcs)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "loadnative")) {
            if (!apply_loadnative(stack, vars, funcs)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_identifier_token(token)) {
            Value *resolved = NULL;
            Vars_t *var_entry = NULL;
            Funcs_t *func_entry = NULL;

            if (identifier_is_name_target(*cursor)) {
                resolved = create_var_name_value(token);
            } else if ((var_entry = find_var_entry(vars, token)) != NULL &&
                       (var_entry->var_value->type == VALUE_LIST || var_entry->var_value->type == VALUE_STRING)) {
                resolved = create_var_name_value(token);
            } else if ((var_entry = find_var_entry(vars, token)) != NULL) {
                resolved = clone_value(var_entry->var_value);
            } else if ((func_entry = find_func_entry(funcs, token)) != NULL && func_entry->type == FUNC_DEMAC) {
                if (!expand_demac(func_entry, cursor)) {
                    free(token);
                    return false;
                }
                free(token);
                continue;
            } else if (func_entry != NULL && func_entry->type == FUNC_APPLY) {
                bool ok = execute_named_entry(stack, vars, funcs, func_entry, "applying body", func_entry->func_name);
                free(token);
                if (!ok) {
                    return false;
                }
                continue;
            } else {
                resolved = create_var_name_value(token);
            }

            if (!push_value(stack, resolved)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else {
            fprintf(stderr, "unknown token: %s\n", token);
            free(token);
            return false;
        }
    }
}

static Value *parse_list_literal(char **cursor, Vars *vars, Funcs *funcs) {
    Value *list = create_list_value();
    RDNState list_stack = {0};
    size_t index = 0;

    if (list == NULL) {
        return NULL;
    }

    if (!execute_list_literal(&list_stack, vars, funcs, cursor)) {
        free_stack_values(&list_stack);
        free_value(list);
        return NULL;
    }

    for (index = 0; index < list_stack.count; index++) {
        Vars_t *entry = NULL;
        Value *item = list_stack.items[index];

        if (item->type == VALUE_AS_VAR && (entry = find_var_entry(vars, item->as.string)) != NULL) {
            Value *resolved_item = clone_value(entry->var_value);

            free_value(item);
            item = resolved_item;
            if (item == NULL) {
                fprintf(stderr, "failed to allocate list item\n");
                list_stack.items[index] = NULL;
                free_stack_values(&list_stack);
                free_value(list);
                return NULL;
            }
        }

        ray_append(&list->as.list, item);
        list_stack.items[index] = NULL;
    }

    free(list_stack.items);
    return list;
}

static bool is_identifier_token(const char *token) {
    size_t index = 0;

    if (token == NULL || token[0] == '\0') {
        return false;
    }

    if (!(isalpha((unsigned char)token[0]) || 
                token[0] == '_' || 
                token[index] == '?' || 
                token[index] == '@' || 
                token[index] == '#' || 
                token[index] == '$' || 
                token[index] == '!' || 
                token[index] == '-' || 
                token[index] == '+' || 
                isdigit((unsigned char)token[0])
                )) {
        return false;
    }

    for (index = 1; token[index] != '\0'; index++) {
        if (!(isalnum((unsigned char)token[index]) || 
                    token[index] == '_' || 
                    token[index] == '-' || 
                    token[index] == '?' || 
                    token[index] == '@' || 
                    token[index] == '$' || 
                    token[index] == '+' || 
                    token[index] == '!' || 
                    token[index] == '#'
                    )) {
            return false;
        }
    }

    return true;
}

static bool identifier_is_name_target(char *cursor) {
    char *next = NULL;
    bool is_string = false;

    if (!next_token(&cursor, &next, &is_string)) {
        return false;
    }

    if (next == NULL) {
        return false;
    }

    if (!is_string &&
        (is_token(next, "let") || is_token(next, "set") || is_token(next, "const") ||
         is_token(next, "defun") || is_token(next, "apply") || is_token(next, "call") || is_token(next, "unlet"))) {
        free(next);
        return true;
    }

    free(next);
    return false;
}

static bool apply_error(RDNState *stack, Vars *vars) {
    Value *name_or_val = NULL;
    if (stack->count < 1) {
        return diagnostic_error_current("error keyword requires 1 operands");
    }
    name_or_val = ray_pop(stack);

    if (name_or_val->type == VALUE_AS_VAR) {
        Vars_t* var = find_current_scope_var_entry(vars, name_or_val->as.string);
        if (var == NULL) {
            free_value(name_or_val);
            return diagnostic_error_current("unknown variable in error keyword");
        }
        const char *msg = var->var_value->as.string;
        free_value(name_or_val);
        return diagnostic_error_current(msg);
    }

    if (name_or_val->type == VALUE_STRING) {
        char *msg = copy_string(name_or_val->as.string);
        free_value(name_or_val);
        bool ok = diagnostic_error_current(msg == NULL ? "(unknown)" : msg);
        free(msg);
        return ok;
    }
    free_value(name_or_val);
    return diagnostic_error_current("error keyword  requires variable name of type string or string literal");
}

static bool apply_file_name(RDNState *stack) {
    const char *path = g_diagnostic_context.path != NULL ? g_diagnostic_context.path : "<repl>";
    Value* rdn_file_path = create_string_value_copy(path);
    ray_append(stack, rdn_file_path);
    return true;
}

static bool apply_line_col(RDNState *stack) {
    size_t line;
    size_t column;
    diagnostic_compute_location(NULL, &line, &column, NULL, NULL);
    ray_append(stack, create_integer_value((long)column));
    ray_append(stack, create_integer_value((long)line));
    return true;
}

static bool apply_do_string(RDNState *stack, Vars *vars, Funcs *funcs){
    if (stack->count < 1) {
        return diagnostic_error_current("do_string requires 1 operands and must be string");
    }
    Value* str = ray_pop(stack);
    if (str->type == VALUE_STRING) {
        evaluate_source(stack, vars, funcs, str->as.string);
        free_value(str);
        return true;
    }else if(str->type == VALUE_AS_VAR) {
        Vars_t* var = find_current_scope_var_entry(vars, str->as.string);
        if (var == NULL || var->var_value->type != VALUE_STRING) {
            free_value(str);
            return diagnostic_error_current("variable must be a string type");
        }
        evaluate_source(stack, vars, funcs, var->var_value->as.string);
        free_value(str);
        return true;
    }
    free_value(str);
    return diagnostic_error_current("the value is not a string type or variable of string");
}

static bool apply_do_file(RDNState *stack, Vars *vars, Funcs *funcs){
    if (stack->count < 1) {
        return diagnostic_error_current("do_file requires 1 operands and must be string");
    }
    Value* path = ray_pop(stack);
    if (path->type == VALUE_STRING) {
        evaluate_file(stack, vars, funcs, path->as.string);
        free_value(path);
        return true;
    }else if (path->type == VALUE_AS_VAR) {
        Vars_t* var = find_current_scope_var_entry(vars, path->as.string);
        if (var == NULL || var->var_value->type != VALUE_STRING) {
            free_value(path);
            return diagnostic_error_current("variable must be a string type");
        }
        evaluate_file(stack, vars, funcs, var->var_value->as.string);
        free_value(path);
        return true;
    }
    free_value(path);
    return diagnostic_error_current("do_file requires 1 operands and must be string");
}

static bool apply_unlet(RDNState *stack, Vars *vars) {
    Value *name = NULL;
    if (stack->count < 1) {
        return diagnostic_error_current("unlet requires 1 operands");
    }

    name = ray_pop(stack);

    if (name == NULL) {
        return diagnostic_error_current("unlet NULL value detected");
    }

    if (name->type != VALUE_AS_VAR) {
        diagnostic_error_current("unlet requires variable name");
        ray_append(stack, name);
        return false;
    }

    int idx_to_remove = -1;
    for(size_t i = 0 ; i < vars->count ; ++i) {
        if (strcmp(vars->items[i]->var_name, name->as.string) == 0) {
            idx_to_remove = (int)i;
            break;
        }
    }

    if (idx_to_remove == -1) {
        ray_append(stack, name);
        return diagnostic_error_current("unlet variable is not identified");
    }

    free_var_entry(vars->items[idx_to_remove]);
    ray_remove_idx(vars, idx_to_remove);
    free_value(name);
    return true;
}

static bool apply_assert(RDNState *stack, Vars *vars)
{
    (void)vars;
    Value *condition = NULL;
    Value *msg = NULL;

    // false "test assert" assert
    if (stack->count < 2) {
        return diagnostic_error_current("assert requires 2 operands boolean and message");
    }

    msg = ray_pop(stack);
    condition = ray_pop(stack);

    if (condition->type == VALUE_BOOLEAN) {
        if (msg->type == VALUE_STRING) {
            if (!condition->as.boolean) {
                size_t line;
                size_t column;
                diagnostic_compute_location(NULL, &line, &column, NULL, NULL);
                fprintf(stderr, "Assertion error at %zu:%zu : %s",line , column , msg->as.string);
                free_value(condition);
                free_value(msg);
                return false;
            }
        }else {
            free_value(condition);
            free_value(msg);
            return diagnostic_error_current("assert message string type at top of the stack");
        }
    }else {
        free_value(condition);
        free_value(msg);
        return diagnostic_error_current("assert condition boolean type before top of the stack");
    }
    free_value(condition);
    free_value(msg);
    return true;
}

static bool apply_let(RDNState *stack, Vars *vars) {
    Value *name = NULL;
    Value *value = NULL;

    if (stack->count < 2) {
        return diagnostic_error_current("let requires 2 operands");
    }

    name = ray_pop(stack);
    value = ray_pop(stack);

    if (name->type != VALUE_AS_VAR) {
        diagnostic_error_current("let requires variable name");
        ray_append(stack, value);
        ray_append(stack, name);
        return false;
    }

    if (!vars_let(vars, name->as.string, value)) {
        ray_append(stack, value);
        ray_append(stack, name);
        return false;
    }

    free_value(name);
    free_value(value);
    return true;
}

static bool apply_set(RDNState *stack, Vars *vars) {
    Value *name = NULL;
    Value *value = NULL;

    if (stack->count < 2) {
        return diagnostic_error_current("set requires 2 operands");
    }

    name = ray_pop(stack);
    value = ray_pop(stack);

    if (name->type != VALUE_AS_VAR) {
        diagnostic_error_current("set requires variable name");
        ray_append(stack, value);
        ray_append(stack, name);
        return false;
    }

    if (!vars_set(vars, name->as.string, value)) {
        ray_append(stack, value);
        ray_append(stack, name);
        return false;
    }

    free_value(name);
    free_value(value);
    return true;
}

static bool apply_enum(RDNState *stack, Vars *vars , bool reset) {
    (void)vars;
    static long counter = 0;
    if (reset) {
        counter = 0;
        return true;
    }
    if (stack->count > 0) {
        Value* top = ray_pop(stack);
        if (top->type == VALUE_INTEGER) {
            counter = top->as.integer;
            free_value(top);
            Value* enum_val = create_integer_value(counter++);
            ray_append(stack, enum_val);
            return true;
        }
        ray_append(stack, top);
    }
    Value* enum_val = create_integer_value(counter++);
    ray_append(stack, enum_val);
    return true;
}

static bool apply_const(RDNState *stack, Vars *vars) {
    Value *name = NULL;
    Value *value = NULL;

    if (stack->count < 2) {
        return diagnostic_error_current("const requires 2 operands");
    }

    name = ray_pop(stack);
    value = ray_pop(stack);

    if (name->type != VALUE_AS_VAR) {
        diagnostic_error_current("const requires variable name");
        ray_append(stack, value);
        ray_append(stack, name);
        return false;
    }

    if (!vars_const(vars, name->as.string, value)) {
        ray_append(stack, value);
        ray_append(stack, name);
        return false;
    }

    free_value(name);
    free_value(value);
    return true;
}

static bool skip_block(char **cursor, BlockStop *stop_reason, bool allow_else);
static bool execute_block(RDNState *stack, Vars* vars, Funcs *funcs, char **cursor, BlockStop *stop_reason, bool allow_else);

static bool apply_if(RDNState *stack, Vars* vars, Funcs *funcs, char **cursor, BlockStop *stop_reason) {
    Value *condition = NULL;
    bool condition_value = false;
    BlockStop branch_stop = BLOCK_STOP_EOF;

    if (stack->count < 1) {
        return diagnostic_error_current("if requires 1 operand");
    }

    condition = ray_pop(stack);
    if (!value_to_boolean(condition, &condition_value)) {
        diagnostic_error_current("if requires a boolean operand");
        ray_append(stack, condition);
        return false;
    }

    free_value(condition);

    if (condition_value) {
        if (!vars_push_scope(vars)) {
            return false;
        }
        if (!execute_block(stack, vars, funcs, cursor, &branch_stop, true)) {
            vars_pop_scope(vars);
            return false;
        }

        if (branch_stop == BLOCK_STOP_EOF) {
            vars_pop_scope(vars);
            return diagnostic_error_current("if missing end");
        }

        if (!materialize_scope_references(stack, vars)) {
            vars_pop_scope(vars);
            return false;
        }

        vars_pop_scope(vars);

        if (branch_stop == BLOCK_STOP_BREAK || branch_stop == BLOCK_STOP_CONTINUE || branch_stop == BLOCK_STOP_RETURN) {
            *stop_reason = branch_stop;
            return true;
        }

        if (branch_stop == BLOCK_STOP_ELSE) {
            if (!skip_block(cursor, &branch_stop, true)) {
                return false;
            }

            if (branch_stop != BLOCK_STOP_END) {
                return diagnostic_error_current("else missing end");
            }
        }

        *stop_reason = BLOCK_STOP_END;
        return true;
    }

    if (!skip_block(cursor, &branch_stop, true)) {
        return false;
    }

    if (branch_stop == BLOCK_STOP_EOF) {
        return diagnostic_error_current("if missing end");
    }

    if (branch_stop == BLOCK_STOP_ELSE) {
        if (!vars_push_scope(vars)) {
            return false;
        }
        if (!execute_block(stack, vars, funcs, cursor, &branch_stop, true)) {
            vars_pop_scope(vars);
            return false;
        }

        if (branch_stop == BLOCK_STOP_BREAK || branch_stop == BLOCK_STOP_CONTINUE || branch_stop == BLOCK_STOP_RETURN) {
            if (!materialize_scope_references(stack, vars)) {
                vars_pop_scope(vars);
                return false;
            }
            vars_pop_scope(vars);
            *stop_reason = branch_stop;
            return true;
        }

        if (branch_stop != BLOCK_STOP_END) {
            vars_pop_scope(vars);
            return diagnostic_error_current("else missing end");
        }

        if (!materialize_scope_references(stack, vars)) {
            vars_pop_scope(vars);
            return false;
        }

        vars_pop_scope(vars);
        *stop_reason = BLOCK_STOP_END;
        return true;
    }

    *stop_reason = BLOCK_STOP_END;
    return true;
}

static bool apply_loop(RDNState *stack, Vars* vars, Funcs *funcs, char **cursor, BlockStop *stop_reason) {
    Value *condition = NULL;
    bool condition_value = false;
    BlockStop body_stop = BLOCK_STOP_EOF;
    char *body_start = *cursor;
    char *body_end = NULL;

    if (stack->count < 1) {
        return diagnostic_error_current("loop requires 1 operand");
    }

    condition = ray_pop(stack);
    if (!value_to_boolean(condition, &condition_value)) {
        diagnostic_error_current("loop requires a boolean operand");
        ray_append(stack, condition);
        return false;
    }
    free_value(condition);

    body_end = body_start;
    if (!skip_block(&body_end, &body_stop, false)) {
        return false;
    }

    if (body_stop != BLOCK_STOP_END) {
        return diagnostic_error_current("loop missing end");
    }

    while (condition_value) {
        char *iteration_cursor = body_start;

        if (!execute_block(stack, vars, funcs, &iteration_cursor, &body_stop, false)) {
            return false;
        }

        if (body_stop == BLOCK_STOP_BREAK) {
            condition_value = false;
            break;
        }

        if (body_stop == BLOCK_STOP_RETURN) {
            *cursor = body_end;
            *stop_reason = BLOCK_STOP_RETURN;
            return true;
        }

        if (body_stop != BLOCK_STOP_END && body_stop != BLOCK_STOP_CONTINUE) {
            return diagnostic_error_current("loop missing end");
        }

        if (stack->count < 1) {
            return diagnostic_error_current("loop body must leave boolean condition on stack");
        }

        condition = ray_pop(stack);
        if (!value_to_boolean(condition, &condition_value)) {
            diagnostic_error_current("loop body must leave boolean condition on stack");
            ray_append(stack, condition);
            return false;
        }
        free_value(condition);
    }

    *cursor = body_end;
    *stop_reason = BLOCK_STOP_END;
    return true;
}

static bool execute_block(RDNState *stack, Vars* vars, Funcs *funcs, char **cursor, BlockStop *stop_reason, bool allow_else) {
    char *token = NULL;
    bool is_string = false;

    while (true) {
        if (!next_token(cursor, &token, &is_string)) {
            return false;
        }

        if (token == NULL) {
            *stop_reason = BLOCK_STOP_EOF;
            return true;
        }

        if (!is_string && is_token(token, "(")) {
            Value *list_value = parse_list_literal(cursor, vars, funcs);
            free(token);
            if (list_value == NULL) {
                return false;
            }
            ray_append(stack, list_value);
            continue;
        } else if (is_token(token, "else")) {
            free(token);
            if (!allow_else) {
                return diagnostic_error_current("unexpected else");
            }
            *stop_reason = BLOCK_STOP_ELSE;
            return true;
        } else if (is_token(token, "end")) {
            free(token);
            *stop_reason = BLOCK_STOP_END;
            return true;
        } else if (is_token(token, "if")) {
            free(token);
            if (!apply_if(stack, vars, funcs, cursor, stop_reason)) {
                return false;
            }
            if (*stop_reason == BLOCK_STOP_BREAK || *stop_reason == BLOCK_STOP_CONTINUE || *stop_reason == BLOCK_STOP_RETURN) {
                return true;
            }
            continue;
        } else if (is_token(token, "loop")) {
            free(token);
            if (!apply_loop(stack, vars, funcs, cursor, stop_reason)) {
                return false;
            }
            if (*stop_reason == BLOCK_STOP_RETURN) {
                return true;
            }
            continue;
        } else if (is_token(token, "defun")) {
            free(token);
            if (!apply_defun(stack, vars, funcs, cursor)) {
                return false;
            }
            continue;
        } else if (is_token(token, "apply")) {
            free(token);
            if (!apply_apply(stack, funcs, cursor)) {
                return false;
            }
            continue;
        } else if (is_token(token, "demac")) {
            free(token);
            if (!apply_demac(stack, funcs, cursor)) {
                return false;
            }
            continue;
        } else if (is_token(token, "call")) {
            free(token);
            if (!apply_call(stack, vars, funcs)) {
                return false;
            }
            continue;
        } else if (is_token(token, "pcall")) {
            free(token);
            if (!apply_pcall(stack, vars, funcs)) {
                return false;
            }
            continue;
        } else if (is_token(token, "break")) {
            free(token);
            *stop_reason = BLOCK_STOP_BREAK;
            return true;
        } else if (is_token(token, "continue")) {
            free(token);
            *stop_reason = BLOCK_STOP_CONTINUE;
            return true;
        } else if (is_token(token, "ret")) {
            free(token);
            *stop_reason = BLOCK_STOP_RETURN;
            return true;
        } else if (is_value_token(token, is_string)) {
            if (!push_token_value(stack, token, is_string)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_operator_token(token)) {
            if (!apply_binary_operator(stack, vars, token)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "print")) {
            if (!apply_print(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "type")) {
            if (!apply_type(stack, vars, funcs)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "__func_name")) {
            if (!apply_func_name(stack, funcs)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "__stack_size")) {
            if (!apply_stack_size(stack)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "exit")){
            int ret = 0;
            if (!apply_exit(stack, vars, &ret)) {
                free(token);
                return false;
            }
            free(token);
            free_stack_values(stack);
            exit(ret);
        } else if (is_token(token, "pop")) {
            if (!apply_pop(stack)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "swap")) {
            if (!apply_swap(stack)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "dup")) {
            if (!apply_dup(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "let")) {
            if (!apply_let(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        }else if (is_token(token, "assert")) {
            if (!apply_assert(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "unlet")) {
            if (!apply_unlet(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "do_string")) {
            if (!apply_do_string(stack, vars , funcs)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "do_file")) {
            if (!apply_do_file(stack, vars , funcs)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "__line_col")) {
            if (!apply_line_col(stack)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "__file")) {
            if (!apply_file_name(stack)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "error")) {
            if (!apply_error(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "set")) {
            if (!apply_set(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "enum")) {
            if (!apply_enum(stack, vars , false)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "reset")) {
            if (!apply_enum(stack, vars , true)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "const")) {
            if (!apply_const(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "to_string")) {
            if (!apply_to_string(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "append")) {
            if (!apply_append(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "remove")) {
            if (!apply_remove(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "index")) {
            if (!apply_index(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "len")) {
            if (!apply_len(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "add_load_path")) {
            if (!apply_add_load_path(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "add_native_path")) {
            if (!apply_add_native_path(stack, vars)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        }else if(is_token(token, "load")) {
            if (!apply_load(stack, vars, funcs)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_token(token, "loadnative")) {
            if (!apply_loadnative(stack, vars, funcs)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        } else if (is_identifier_token(token)) {
            Value *resolved = NULL;
            Vars_t *var_entry = NULL;
            Funcs_t *func_entry = NULL;

            if (identifier_is_name_target(*cursor)) {
                resolved = create_var_name_value(token);
            } else if ((var_entry = find_var_entry(vars, token)) != NULL &&
                       (var_entry->var_value->type == VALUE_LIST || var_entry->var_value->type == VALUE_STRING)) {
                resolved = create_var_name_value(token);
            } else if ((var_entry = find_var_entry(vars, token)) != NULL) {
                resolved = clone_value(var_entry->var_value);
            } else if ((func_entry = find_func_entry(funcs, token)) != NULL && func_entry->type == FUNC_DEMAC) {
                if (!expand_demac(func_entry, cursor)) {
                    free(token);
                    return false;
                }
                free(token);
                continue;
            } else if (func_entry != NULL && func_entry->type == FUNC_APPLY) {
                bool ok = execute_named_entry(stack, vars, funcs, func_entry, "applying body", func_entry->func_name);
                free(token);
                if (!ok) {
                    return false;
                }
                continue;
            } else {
                resolved = create_var_name_value(token);
            }

            if (!push_value(stack, resolved)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        }else {
            diagnostic_error_current("unknown token: %s", token);
            free(token);
            return false;
        }
    }
    return true;
}

static bool skip_if(char **cursor) {
    BlockStop stop_reason = BLOCK_STOP_EOF;

    if (!skip_block(cursor, &stop_reason, true)) {
        return false;
    }

    if (stop_reason == BLOCK_STOP_EOF) {
        return diagnostic_error_current("if missing end");
    }

    if (stop_reason == BLOCK_STOP_ELSE) {
        if (!skip_block(cursor, &stop_reason, true)) {
            return false;
        }

        if (stop_reason != BLOCK_STOP_END) {
            return diagnostic_error_current("else missing end");
        }
    }

    return true;
}

static bool skip_loop(char **cursor) {
    BlockStop stop_reason = BLOCK_STOP_EOF;

    if (!skip_block(cursor, &stop_reason, false)) {
        return false;
    }

    if (stop_reason != BLOCK_STOP_END) {
        return diagnostic_error_current("loop missing end");
    }

    return true;
}

static bool skip_demac(char **cursor) {
    char *token = NULL;
    bool is_string = false;

    while (true) {
        if (!next_token(cursor, &token, &is_string)) {
            return false;
        }

        if (token == NULL) {
            return diagnostic_error_current("demac missing end");
        }

        if (!is_string && is_token(token, "end")) {
            free(token);
            return true;
        }

        free(token);
    }
}

static bool skip_block(char **cursor, BlockStop *stop_reason, bool allow_else) {
    char *token = NULL;
    bool is_string = false;

    while (true) {
        if (!next_token(cursor, &token, &is_string)) {
            return false;
        }

        if (token == NULL) {
            *stop_reason = BLOCK_STOP_EOF;
            return true;
        }

        if (is_token(token, "else")) {
            free(token);
            if (!allow_else) {
                return diagnostic_error_current("unexpected else");
            }
            *stop_reason = BLOCK_STOP_ELSE;
            return true;
        }

        if (is_token(token, "end")) {
            free(token);
            *stop_reason = BLOCK_STOP_END;
            return true;
        }

        if (is_token(token, "if")) {
            free(token);
            if (!skip_if(cursor)) {
                return false;
            }
            continue;
        }

        if (is_token(token, "demac")) {
            free(token);
            if (!skip_demac(cursor)) {
                return false;
            }
            continue;
        }

        if (is_token(token, "loop") || is_token(token, "defun") || is_token(token, "apply")) {
            free(token);
            if (!skip_loop(cursor)) {
                return false;
            }
            continue;
        }

        if (is_token(token, "break") || is_token(token, "continue")) {
            free(token);
            continue;
        }

        free(token);
    }
}

static bool evaluate_source(RDNState *stack, Vars* vars, Funcs *funcs, char *source) {
    BlockStop stop_reason = BLOCK_STOP_EOF;
    char *cursor = source;
    DiagnosticContext previous_context = g_diagnostic_context;

    RDNState _stack = {0};
    Vars _vars = {0};
    Funcs _funcs = {0};

    diagnostic_set_source(g_current_source_path, source, 1, 1);

    if (!execute_block(
                stack == NULL ? &_stack : stack , 
                vars == NULL ? &_vars : vars, 
                funcs == NULL ? &_funcs : funcs, &cursor, &stop_reason, false)) {
        g_diagnostic_context = previous_context;
        return false;
    }

    if (stop_reason == BLOCK_STOP_BREAK) {
        g_diagnostic_context = previous_context;
        return diagnostic_error_current("unexpected break");
    }

    if (stop_reason == BLOCK_STOP_CONTINUE) {
        g_diagnostic_context = previous_context;
        return diagnostic_error_current("unexpected continue");
    }

    if (stop_reason == BLOCK_STOP_RETURN) {
        g_diagnostic_context = previous_context;
        return diagnostic_error_current("unexpected ret");
    }

    if (stop_reason != BLOCK_STOP_EOF) {
        g_diagnostic_context = previous_context;
        return diagnostic_error_current("unexpected block terminator");
    }

    g_diagnostic_context = previous_context;
    return true;
}

static bool evaluate_file(RDNState *stack, Vars *vars, Funcs *funcs, const char *path) {
    char *source = NULL;
    const char *previous_path = g_current_source_path;
    bool ok = false;

    source = read_file(path);
    if (source == NULL) {
        return false;
    }

    g_current_source_path = path;
    ok = evaluate_source(stack, vars, funcs, source);
    g_current_source_path = previous_path;

    free(source);
    return ok;
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    char *buffer = NULL;
    long length = 0;
    size_t bytes_read = 0;

    if (file == NULL) {
        diagnostic_error_current("failed to open '%s'", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        diagnostic_error_current("failed to seek '%s'", path);
        fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0) {
        diagnostic_error_current("failed to read size of '%s'", path);
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        diagnostic_error_current("failed to rewind '%s'", path);
        fclose(file);
        return NULL;
    }

    buffer = malloc((size_t)length + 1);
    if (buffer == NULL) {
        diagnostic_error_current("failed to allocate file buffer");
        fclose(file);
        return NULL;
    }

    bytes_read = fread(buffer, 1, (size_t)length, file);
    if (bytes_read != (size_t)length) {
        diagnostic_error_current("failed to read '%s'", path);
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

static bool path_is_readable_file(const char *path) {
    FILE *file = NULL;

    if (path == NULL) {
        return false;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }

    fclose(file);
    return true;
}

static char *resolve_path_from_current_source(const char *path) {
    const char *slash = NULL;
    size_t dir_length = 0;
    size_t path_length = 0;
    char *resolved = NULL;

    if (path == NULL) {
        return NULL;
    }

    if (path[0] == '/' || g_current_source_path == NULL) {
        return copy_string(path);
    }

    slash = strrchr(g_current_source_path, '/');
    if (slash == NULL) {
        return copy_string(path);
    }

    dir_length = (size_t)(slash - g_current_source_path + 1);
    path_length = strlen(path);
    resolved = malloc(dir_length + path_length + 1);
    if (resolved == NULL) {
        return NULL;
    }

    memcpy(resolved, g_current_source_path, dir_length);
    memcpy(resolved + dir_length, path, path_length + 1);
    return resolved;
}

static char *join_paths(const char *base, const char *path) {
    size_t base_length = 0;
    size_t path_length = 0;
    bool need_sep = false;
    char *joined = NULL;

    if (base == NULL || path == NULL) {
        return NULL;
    }

    base_length = strlen(base);
    path_length = strlen(path);
    need_sep = base_length > 0 && base[base_length - 1] != '/';

    joined = malloc(base_length + path_length + (need_sep ? 2 : 1));
    if (joined == NULL) {
        return NULL;
    }

    memcpy(joined, base, base_length);
    if (need_sep) {
        joined[base_length] = '/';
        memcpy(joined + base_length + 1, path, path_length + 1);
    } else {
        memcpy(joined + base_length, path, path_length + 1);
    }

    return joined;
}

static bool path_has_separator(const char *path) {
    return path != NULL && (strchr(path, '/') != NULL || strchr(path, '\\') != NULL);
}

static bool path_is_absolute(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }

    if (path[0] == '/' || path[0] == '\\') {
        return true;
    }

    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' && (path[2] == '/' || path[2] == '\\')) {
        return true;
    }

    return false;
}

static char *resolve_load_path_candidate(const char *path, const SearchPathStack *search_paths) {
    char *candidate = NULL;
    char *canonical = NULL;
    size_t index = 0;

    if (path == NULL) {
        return NULL;
    }

    candidate = resolve_path_from_current_source(path);
    if (candidate != NULL && path_is_readable_file(candidate)) {
        canonical = canonicalize_existing_path(candidate);
        if (canonical != NULL) {
            free(candidate);
            candidate = canonical;
        }
        return candidate;
    }
    free(candidate);

    if (path_is_absolute(path) || path_has_separator(path)) {
        return NULL;
    }

    for (index = 0; index < search_paths->count; index++) {
        candidate = join_paths(search_paths->items[index], path);
        if (candidate == NULL) {
            return NULL;
        }

        if (path_is_readable_file(candidate)) {
            canonical = canonicalize_existing_path(candidate);
            if (canonical != NULL) {
                free(candidate);
                candidate = canonical;
            }
            return candidate;
        }

        free(candidate);
        candidate = NULL;
    }

    return NULL;
}

static char *canonicalize_existing_path(const char *path) {
    size_t length = 0;
    size_t out_length = 0;
    bool absolute = false;
    char *normalized = NULL;
    const char *cursor = NULL;

    if (path == NULL) {
        return NULL;
    }

    length = strlen(path);
    normalized = malloc(length + 2);
    if (normalized == NULL) {
        return NULL;
    }

    absolute = path[0] == '/';
    cursor = path;

    if (absolute) {
        normalized[out_length++] = '/';
        cursor++;
    }

    while (*cursor != '\0') {
        const char *segment_start = cursor;
        size_t segment_length = 0;

        while (*cursor == '/') {
            cursor++;
        }
        segment_start = cursor;

        while (*cursor != '\0' && *cursor != '/') {
            cursor++;
        }

        segment_length = (size_t)(cursor - segment_start);
        if (segment_length == 0) {
            continue;
        }

        if (segment_length == 1 && segment_start[0] == '.') {
            continue;
        }

        if (segment_length == 2 && segment_start[0] == '.' && segment_start[1] == '.') {
            if (out_length > 0 && !(out_length == 1 && normalized[0] == '/')) {
                if (normalized[out_length - 1] == '/') {
                    out_length--;
                }
                while (out_length > 0 && normalized[out_length - 1] != '/') {
                    out_length--;
                }
                if (out_length == 0 && absolute) {
                    normalized[out_length++] = '/';
                }
            } else if (!absolute) {
                if (out_length > 0 && normalized[out_length - 1] != '/') {
                    normalized[out_length++] = '/';
                }
                normalized[out_length++] = '.';
                normalized[out_length++] = '.';
            }
            continue;
        }

        if (out_length > 0 && normalized[out_length - 1] != '/') {
            normalized[out_length++] = '/';
        }

        memcpy(normalized + out_length, segment_start, segment_length);
        out_length += segment_length;
    }

    if (out_length == 0) {
        normalized[out_length++] = absolute ? '/' : '.';
    }

    normalized[out_length] = '\0';
    return normalized;
}

static bool load_path_stack_contains(const char *path) {
    size_t index = 0;

    for (index = 0; index < g_load_path_stack.count; index++) {
        if (strcmp(g_load_path_stack.items[index], path) == 0) {
            return true;
        }
    }

    return false;
}

static bool push_load_path(const char *path) {
    char *copy = copy_string(path);

    if (copy == NULL) {
        return false;
    }

    ray_append(&g_load_path_stack, copy);
    return true;
}

static void pop_load_path(void) {
    char *path = NULL;

    if (g_load_path_stack.count == 0) {
        return;
    }

    path = ray_pop(&g_load_path_stack);
    free(path);
}

static void free_search_path_stack(SearchPathStack *paths) {
    while (paths->count > 0) {
        free(ray_pop(paths));
    }

    ray_clear(paths);
}

static bool search_path_stack_contains(const SearchPathStack *paths, const char *path) {
    size_t index = 0;

    for (index = 0; index < paths->count; index++) {
        if (strcmp(paths->items[index], path) == 0) {
            return true;
        }
    }

    return false;
}

static bool push_search_path(SearchPathStack *paths, const char *path) {
    char *copy = NULL;

    if (search_path_stack_contains(paths, path)) {
        return true;
    }

    copy = copy_string(path);
    if (copy == NULL) {
        return false;
    }

    ray_append(paths, copy);
    return true;
}

// TODO: rayden was here
static bool reset_search_paths(void) {
    free_search_path_stack(&g_script_search_paths);
    free_search_path_stack(&g_native_search_paths);

    if (!push_search_path(&g_script_search_paths, "libs")) {
        return false;
    }
    if (!push_search_path(&g_script_search_paths, RDN_INSTALL_PREFIX "/libs")) {
        return false;
    }
    if (!push_search_path(&g_native_search_paths, "nativelibs")) {
        return false;
    }
    if (!push_search_path(&g_native_search_paths, RDN_INSTALL_PREFIX "/nativelibs")) {
        return false;
    }

    return true;
}

static bool pop_string_path_operand(RDNState *stack, Vars *vars, const char *context, Value **out_target, char **out_path) {
    Value *target = NULL;

    if (stack->count < 1) {
        return diagnostic_error_current("%s requires 1 operand", context);
    }

    target = ray_pop(stack);
    if (target->type == VALUE_AS_VAR) {
        Vars_t *entry = find_var_entry(vars, target->as.string);
        if (entry == NULL) {
            free_value(target);
            return diagnostic_error_current("%s requires existing variable path", context);
        }

        if (entry->var_value->type != VALUE_STRING) {
            free_value(target);
            return diagnostic_error_current("%s requires string type", context);
        }

        *out_target = target;
        *out_path = entry->var_value->as.string;
        return true;
    }

    if (target->type != VALUE_STRING) {
        free_value(target);
        return diagnostic_error_current("%s accepts a string path", context);
    }

    *out_target = target;
    *out_path = target->as.string;
    return true;
}

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

static bool native_api_push_list(RDNApi *api) {
    NativeCallState *state = api->userdata;
    return push_value(state->stack, create_list_value());
}

static bool native_api_list_len(RDNApi *api, long index, size_t *out_length) {
    NativeCallState *state = api->userdata;
    Value *value = native_get_stack_value(state->stack, index);

    if (value == NULL || value->type != VALUE_LIST || out_length == NULL) {
        return false;
    }

    *out_length = value->as.list.count;
    return true;
}

static bool native_api_list_append(RDNApi *api, long list_index, long value_index) {
    NativeCallState *state = api->userdata;
    Value *list_value = native_get_stack_value(state->stack, list_index);
    Value *item_value = native_get_stack_value(state->stack, value_index);
    Value *item_copy = NULL;

    if (list_value == NULL || list_value->type != VALUE_LIST) {
        return native_api_raise_error(api, "list_append requires list target");
    }

    if (item_value == NULL) {
        return native_api_raise_error(api, "list_append requires existing value");
    }

    item_copy = clone_value(item_value);
    if (item_copy == NULL) {
        return native_api_raise_error(api, "failed to clone appended list value");
    }

    ray_append(&list_value->as.list, item_copy);
    return true;
}

static bool native_api_list_index(RDNApi *api, long list_index, long item_index) {
    NativeCallState *state = api->userdata;
    Value *list_value = native_get_stack_value(state->stack, list_index);
    Value *item_copy = NULL;

    if (list_value == NULL || list_value->type != VALUE_LIST) {
        return native_api_raise_error(api, "list_index requires list target");
    }

    if (item_index < 0 || (size_t)item_index >= list_value->as.list.count) {
        return native_api_raise_error(api, "list_index out of range");
    }

    item_copy = clone_value(list_value->as.list.items[item_index]);
    if (item_copy == NULL) {
        return native_api_raise_error(api, "failed to clone list item");
    }

    return push_value(state->stack, item_copy);
}

static bool native_api_list_remove(RDNApi *api, long list_index, long item_index) {
    NativeCallState *state = api->userdata;
    Value *list_value = native_get_stack_value(state->stack, list_index);
    size_t index = 0;

    if (list_value == NULL || list_value->type != VALUE_LIST) {
        return native_api_raise_error(api, "list_remove requires list target");
    }

    if (item_index < 0 || (size_t)item_index >= list_value->as.list.count) {
        return native_api_raise_error(api, "list_remove out of range");
    }

    free_value(list_value->as.list.items[item_index]);
    for (index = (size_t)item_index + 1; index < list_value->as.list.count; index++) {
        list_value->as.list.items[index - 1] = list_value->as.list.items[index];
    }
    list_value->as.list.count--;
    return true;
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

static bool append_text(char **buffer, size_t *length, const char *text) {
    size_t text_length = strlen(text);
    char *grown = realloc(*buffer, *length + text_length + 1);

    if (grown == NULL) {
        return false;
    }

    memcpy(grown + *length, text, text_length + 1);
    *buffer = grown;
    *length += text_length;
    return true;
}

static bool source_has_complete_blocks(const char *source, const Funcs *funcs, bool *out_complete) {
    char *cursor = (char *)source;
    char *token = NULL;
    bool is_string = false;
    int depth = 0;
    MacroExpansionStack expansions = {0};
    DiagnosticContext previous_context = g_diagnostic_context;

    diagnostic_set_source(NULL, source, 1, 1);

    while (true) {
        if (!next_token(&cursor, &token, &is_string)) {
            free_macro_expansion_stack(&expansions);
            return false;
        }

        if (token == NULL) {
            *out_complete = (depth == 0);
            g_diagnostic_context = previous_context;
            free_macro_expansion_stack(&expansions);
            return true;
        }

        if (!is_string && is_token(token, "demac")) {
            free(token);
            while (true) {
                if (!next_token(&cursor, &token, &is_string)) {
                    free_macro_expansion_stack(&expansions);
                    return false;
                }

                if (token == NULL) {
                    *out_complete = false;
                    g_diagnostic_context = previous_context;
                    free_macro_expansion_stack(&expansions);
                    return true;
                }

                if (!is_string && is_token(token, "end")) {
                    break;
                }

                free(token);
            }
        } else if (!is_string && (is_token(token, "if") || is_token(token, "loop") || is_token(token, "defun") ||
                                  is_token(token, "apply"))) {
            depth++;
        } else if (!is_string && is_token(token, "end")) {
            depth--;
            if (depth < 0) {
                diagnostic_error_current("unexpected end");
                free(token);
                g_diagnostic_context = previous_context;
                free_macro_expansion_stack(&expansions);
                return false;
            }
        } else if (!is_string && is_token(token, "else") && depth == 0) {
            diagnostic_error_current("unexpected else");
            free(token);
            g_diagnostic_context = previous_context;
            free_macro_expansion_stack(&expansions);
            return false;
        } else if (!is_string && is_identifier_token(token)) {
            Funcs_t *entry = find_func_entry(funcs, token);

            if (entry != NULL && entry->type == FUNC_DEMAC) {
                size_t body_length = strlen(entry->as.func_body);
                size_t rest_length = strlen(cursor);
                char *expanded = malloc(body_length + rest_length + 1);

                if (expanded == NULL) {
                    free(token);
                    g_diagnostic_context = previous_context;
                    free_macro_expansion_stack(&expansions);
                    return false;
                }

                memcpy(expanded, entry->as.func_body, body_length);
                memcpy(expanded + body_length, cursor, rest_length + 1);
                ray_append(&expansions, expanded);
                cursor = expanded;
            }
        }

        free(token);
    }
}

static int run_repl(void) {
    RDNState stack = {0};
    Vars vars = {0};
    Funcs funcs = {0};
    char line[4096];
    char *source = NULL;
    size_t source_length = 0;
    bool complete = true;

    if (!reset_search_paths()) {
        fprintf(stderr, "failed to initialize search paths\n");
        return EXIT_FAILURE;
    }

    apply_host_environment(&vars);

    printf("raden repl\n");
    printf("raden interpreter version %s , check or report any bug in 'https://github.com/abdorayden/rdn'\n" , RADEN_VERSION);
    printf("press Ctrl-D to exit\n");

    while (true) {
        fputs(source_length == 0 ? "RDN >> " : ".. ", stdout);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            if (source_length != 0) {
                fprintf(stderr, "incomplete input\n");
            } else {
                putchar('\n');
            }
            break;
        }

        if (!append_text(&source, &source_length, line)) {
            fprintf(stderr, "failed to allocate repl buffer\n");
            free(source);
            free_macro_expansions();
            free_stack_values(&stack);
            free_vars(&vars);
            free_funcs(&funcs);
            return EXIT_FAILURE;
        }

        if (!source_has_complete_blocks(source, &funcs, &complete)) {
            free(source);
            source = NULL;
            source_length = 0;
            free_macro_expansions();
            free_stack_values(&stack);
            stack = (RDNState){0};
            continue;
        }

        if (!complete) {
            continue;
        }

        if (!evaluate_source(&stack, &vars, &funcs, source)) {
            free_stack_values(&stack);
            stack = (RDNState){0};
        }
        free_macro_expansions();

        free(source);
        source = NULL;
        source_length = 0;
    }

    free(source);
    free_macro_expansions();
    free_stack_values(&stack);
    free_vars(&vars);
    free_funcs(&funcs);
    return EXIT_SUCCESS;
}

static const char *host_os_name(void) {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#elif defined(__FreeBSD__)
    return "freebsd";
#elif defined(__unix__)
    return "unix";
#else
    return "unknown";
#endif
}

static const char *host_shared_library_extension(void) {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

static void apply_host_environment(Vars *vars) {
    Vars_t *host_os_var = NULL;
    Vars_t *shared_lib_ext_var = NULL;

    host_os_var = create_var_entry("__host_os", create_string_value_copy(host_os_name()), true);
    if (host_os_var != NULL) {
        ray_append(vars, host_os_var);
    }

    shared_lib_ext_var = create_var_entry(
        "__sharedlib_ext",
        create_string_value_copy(host_shared_library_extension()),
        true
    );
    if (shared_lib_ext_var != NULL) {
        ray_append(vars, shared_lib_ext_var);
    }
}

static void apply_argv(Vars* vars , const char* path, int argc , char** argv) {

    Value* argv_list = create_list_value();
    Value* tha_path_value = create_string_value_copy(path);
    ray_append(&argv_list->as.list, tha_path_value);

    for(int i = 2 ; i < argc ; ++i) {
        Value* val = create_string_value_copy(argv[i]);
        ray_append(&argv_list->as.list, val);
    }

    Vars_t* argv_var = create_var_entry("__argv", argv_list, false);
    ray_append(vars, argv_var);

}

int rdn_main(int argc , char** argv) {
    const char *path = NULL;
    RDNState stack = {0};
    Vars vars = {0};
    Funcs funcs = {0};
    int exit_code = EXIT_FAILURE;

    if (argc < 2) {
        return run_repl();
    }

    path = argv[1];
    if (!reset_search_paths()) {
        fprintf(stderr, "failed to initialize search paths\n");
        return exit_code;
    }
    apply_host_environment(&vars);
    apply_argv(&vars, path, argc ,  argv);

    if (!evaluate_file(&stack, &vars, &funcs, path)) {
        free_macro_expansions();
        free_stack_values(&stack);
        free_vars(&vars);
        free_funcs(&funcs);
        return exit_code;
    }

    if (stack.count != 0) {
        fprintf(stderr, "unexpected values left on stack: %zu\n", stack.count);
        free_macro_expansions();
        free_stack_values(&stack);
        free_vars(&vars);
        free_funcs(&funcs);
        return exit_code;
    }

    free_macro_expansions();
    free_stack_values(&stack);
    free_vars(&vars);
    free_funcs(&funcs);
    return EXIT_SUCCESS;
}
