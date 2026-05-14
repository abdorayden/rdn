#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./src/stack.h"

// TODO: make a good api in C so i can implement native functions or create bindings

// TODO: introduce loops 
// example :
// true loop 
//  ...
// end

// TODO: introduce lists 
// example :
// (1 2 3 4 5 "hello") [* push this list on the stack *]
// 6 append [* accept a value *]
// 0 index [* accept index from list and push it in the stack *]
// 0 remove [* accept index and remove the value from it *]
// [* other example *]
// ("rayden" , "abdo") names let
// names "other_name" append

// TODO: introduce string manipulations (to make it easy just convert it to a list)
// example :
// "hello" slice 0 index print

// TODO: introduce functions 
// example :
// defun foo
//  "hello foo" print
// end
// [* call function *]
// foo call

// TODO: introduce big ints 

typedef enum ValueType ValueType;
typedef enum BlockStop BlockStop;
typedef struct Value Value ;
typedef struct Vars_t Vars_t;
typedef struct RDNSharedState RDNSharedState;

typedef RLStack(Value *) RDNState;
typedef RLList(Vars_t*) Vars;

static void free_value(Value *value);

enum ValueType{
    VALUE_INTEGER,
    VALUE_DOUBLE,
    VALUE_STRING,
    VALUE_BOOLEAN,
    VALUE_LIST,

    VALUE_AS_VAR,
};

struct Value{
    ValueType type;
    union {
        long integer;
        double number;
        char *string;
        bool boolean;
        // list is (1 2 3 4 5 6)
        RLList(Value*) list;
    } as;
};

struct Vars_t {
    char* var_name;
    Value* var_value;
    bool is_scope_marker; // define the variable in scope
    bool is_const; // check if the variable is constant
};

struct Funcs_t {
    char* func_name;
    RLStack(Value*) body_stack;
};

enum BlockStop{
    BLOCK_STOP_EOF,
    BLOCK_STOP_ELSE,
    BLOCK_STOP_END,
};

struct RDNSharedState {
    RDNState rdn_state;
    Vars    rdn_vars;
};

static bool is_token(const char *value, const char *expected);
static bool is_operator_token(const char *value);
static char *copy_string(const char *text) ;
static Value *create_integer_value(long integer);
static Value *create_double_value(double number);
static Value *create_boolean_value(bool boolean);
static Value *create_string_value_owned(char *string);
static Value *create_string_value_copy(const char *string);
static Value *create_list_value(void);
static Value *create_var_name_value(const char *name);
static Value *clone_value(const Value *value);
static Vars_t *create_scope_marker(void);
static Vars_t *create_var_entry(const char *name, Value *value, bool is_const);
static void free_var_entry(Vars_t *entry);
static void free_vars(Vars *vars);
static bool vars_push_scope(Vars *vars);
static void vars_pop_scope(Vars *vars);
static Vars_t *find_var_entry(const Vars *vars, const char *name);
static Vars_t *find_current_scope_var_entry(const Vars *vars, const char *name);
static bool vars_let(Vars *vars, const char *name, const Value *value);
static bool vars_const(Vars *vars, const char *name, const Value *value);
static void free_value(Value *value);
static void free_stack_values(RDNState *stack);
static bool push_value(RDNState *stack, Value *value);
static bool parse_integer_token(const char *text, long *out_value);
static bool parse_double_token(const char *text, double *out_value);
static bool value_to_double(const Value *value, double *out_value);
static bool value_to_long(const Value *value, long *out_value);
static bool value_to_boolean(const Value *value, bool *out_value);
static Value *resolve_value_if_var(const Vars *vars, Value *value, const char *context);
#define resolve_value_of_var_if_it_is(vars , value) do{resolve_value_if_var(vars, value, NULL);}while(0)
static bool append_value_repr(char **buffer, size_t *length, const Value *value);
static bool values_equal(const Value *left, const Value *right);
static bool values_not_equal(const Value *left, const Value *right);
static bool values_compare(const Value *left, const Value *right, const char *operator_token, bool *out_value);
static void print_value(const Value *value);
static char* exit_value(const Value* value , int* out_exit);
static bool apply_binary_operator(RDNState *stack, Vars *vars, const char *operator_token);
static bool apply_print(RDNState *stack, Vars *vars);
static bool apply_exit(RDNState *stack , Vars *vars, int* exit_status);
static bool apply_type(RDNState *stack, Vars *vars);
static bool apply_swap(RDNState *stack);
static bool apply_pop(RDNState *stack);
static bool apply_dup(RDNState *stack, Vars *vars);
static bool apply_append(RDNState *stack, Vars *vars);
static bool apply_remove(RDNState *stack, Vars *vars);
static bool apply_index(RDNState *stack, Vars *vars);
static bool apply_len(RDNState *stack, Vars *vars);

// to_string builtin function convert value from the top stack to string without remove it
static bool apply_to_string(RDNState *stack, Vars *vars);
static bool skip_comment(char **cursor);
static bool append_char(char **buffer, size_t *length, size_t *capacity, char ch);
static bool read_string_token(char **cursor, char **out_token);
static bool read_plain_token(char **cursor, char **out_token);
static bool next_token(char **cursor, char **out_token, bool *out_is_string);
static bool push_token_value(RDNState *stack, const char *token, bool is_string);
static bool is_value_token(const char *token, bool is_string);
static bool is_identifier_token(const char *token);
static Value *parse_list_literal(char **cursor, Vars *vars);
static bool identifier_is_save_target(char *cursor);
static bool apply_let(RDNState *stack, Vars *vars);
static bool apply_const(RDNState *stack, Vars *vars);
static bool skip_block(char **cursor, BlockStop *stop_reason, bool allow_else);
static bool execute_block(RDNState *stack, Vars* vars, char **cursor, BlockStop *stop_reason, bool allow_else);
static bool apply_if(RDNState *stack, Vars* vars, char **cursor);
static bool execute_block(RDNState *stack, Vars* vars, char **cursor, BlockStop *stop_reason, bool allow_else);
static bool skip_if(char **cursor);
static bool skip_block(char **cursor, BlockStop *stop_reason, bool allow_else);
static bool evaluate_source(RDNState *stack, Vars* vars, char *source);
#define rdn_do_string(src) do{evaluate_source(NULL , NULL , (src))}while(0)
static char *read_file(const char *path);
static bool append_text(char **buffer, size_t *length, const char *text);
static bool source_has_complete_blocks(const char *source, bool *out_complete);
static int run_repl(void);

int main(int argc, char **argv) {
    const char *path = NULL;
    char *source = NULL;
    RDNState stack = {0};
    Vars vars = {0};
    int exit_code = EXIT_FAILURE;

    if (argc < 2) {
        return run_repl();
    }

    path = argv[1];
    source = read_file(path);
    if (source == NULL) {
        return exit_code;
    }

    if (!evaluate_source(&stack,&vars, source)) {
        free(source);
        free_stack_values(&stack);
        free_vars(&vars);
        return exit_code;
    }

    if (stack.count != 0) {
        fprintf(stderr, "unexpected values left on stack: %zu\n", stack.count);
        free(source);
        free_stack_values(&stack);
        free_vars(&vars);
        return exit_code;
    }

    free(source);
    free_stack_values(&stack);
    free_vars(&vars);
    return EXIT_SUCCESS;
}

static bool is_token(const char *value, const char *expected) {
    return strcmp(value, expected) == 0;
}

static bool is_operator_token(const char *value) {
    return is_token(value, "+") || is_token(value, "-") || is_token(value, "*") ||
           is_token(value, "/") || is_token(value, "<<") || is_token(value, ">>") ||
           is_token(value, "|") || is_token(value, "&") || is_token(value, "^") ||
           is_token(value, "<") || is_token(value, ">") || is_token(value, "<=") ||
           is_token(value, ">=") || is_token(value, "!=") || is_token(value, "=") ||
           is_token(value, "!");
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

static bool vars_push_scope(Vars *vars) {
    Vars_t *marker = create_scope_marker();

    if (marker == NULL) {
        fprintf(stderr, "failed to allocate scope marker\n");
        return false;
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

static bool vars_let(Vars *vars, const char *name, const Value *value) {
    Vars_t *entry = find_current_scope_var_entry(vars, name);
    Value *copy = clone_value(value);

    if (copy == NULL) {
        fprintf(stderr, "failed to clone variable value\n");
        return false;
    }

    if (entry != NULL) {
        if (entry->is_const) {
            fprintf(stderr, "cannot change constant '%s'\n", name);
            free_value(copy);
            return false;
        }
        free_value(entry->var_value);
        entry->var_value = copy;
        return true;
    }

    entry = create_var_entry(name, copy, false);
    if (entry == NULL) {
        fprintf(stderr, "failed to allocate variable entry\n");
        free_value(copy);
        return false;
    }

    ray_append(vars, entry);
    return true;
}

static bool vars_const(Vars *vars, const char *name, const Value *value) {
    Vars_t *entry = find_current_scope_var_entry(vars, name);
    Value *copy = clone_value(value);

    if (entry != NULL) {
        fprintf(stderr, "'%s' already exists in current scope\n", name);
        return false;
    }

    if (copy == NULL) {
        fprintf(stderr, "failed to clone constant value\n");
        return false;
    }

    entry = create_var_entry(name, copy, true);
    if (entry == NULL) {
        fprintf(stderr, "failed to allocate constant entry\n");
        free_value(copy);
        return false;
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
        fprintf(stderr, "%s requires known variable '%s'\n", context == NULL ? "(UNKNOWN)" : context, value->as.string);
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
            fprintf(stderr, "operator '%s' requires 1 operand, got %zu\n", operator_token, stack->count);
            return false;
        }

        right = resolve_value_if_var(vars, ray_pop(stack), operator_token);
        if (right == NULL) {
            return false;
        }
        if (!value_to_boolean(right, &right_bool)) {
            fprintf(stderr, "operator '%s' requires a boolean operand\n", operator_token);
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
        fprintf(stderr, "operator '%s' requires 2 operands, got %zu\n", operator_token, stack->count);
        return false;
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
            fprintf(stderr, "operator '%s' requires numeric operands\n", operator_token);
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
            fprintf(stderr, "operator '%s' requires both operands to be boolean or integer\n", operator_token);
            ray_append(stack, left);
            ray_append(stack, right);
            return false;
        } else {
            if (!value_to_long(left, &left_long) || !value_to_long(right, &right_long)) {
                fprintf(stderr, "operator '%s' requires integer operands\n", operator_token);
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
            fprintf(stderr, "operator '%s' requires integer operands\n", operator_token);
            ray_append(stack, left);
            ray_append(stack, right);
            return false;
        }

        if ((is_token(operator_token, "<<") || is_token(operator_token, ">>")) && right_long < 0) {
            fprintf(stderr, "shift operators require a non-negative count\n");
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
            fprintf(stderr, "operator '%s' requires numeric operands\n", operator_token);
            ray_append(stack, left);
            ray_append(stack, right);
            return false;
        }

        if (is_token(operator_token, "/") && right_double == 0.0) {
            fprintf(stderr, "division by zero\n");
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
        fprintf(stderr, "print requires 1 operand\n");
        return false;
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
        fprintf(stderr, "type requires 1 operand\n");
        return false;
    }

    Value *value = NULL;
    value = resolve_value_if_var(vars, ray_pop(stack), "exit");
    if (value == NULL) {
        return false;
    }

    char* ret = exit_value(value, exit_status);

    free_value(value);

    if (ret){
        fprintf(stderr, "%s\n" , ret);
        return false;
    }
    return true;
}

static bool apply_type(RDNState *stack, Vars *vars) {
    Value *value = NULL;
    Value *result = NULL;

    if (stack->count < 1) {
        fprintf(stderr, "type requires 1 operand\n");
        return false;
    }

    value = resolve_value_if_var(vars, ray_pop(stack), "type");
    if (value == NULL) {
        return false;
    }

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

    free_value(value);

    if (result == NULL) {
        fprintf(stderr, "failed to allocate type result\n");
        return false;
    }

    ray_append(stack, result);
    return true;
}

static bool apply_swap(RDNState *stack) {
    if (stack->count < 2) {
        fprintf(stderr, "swap type requires 2 operand in stack\n");
        return false;
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
        fprintf(stderr, "pop type requires 1 operand in stack\n");
        return false;
    }

    Value *value = NULL;
    value = ray_pop(stack);
    free_value(value);
    return true;
}

static bool apply_dup(RDNState *stack, Vars *vars) {
    if (stack->count < 1) {
        fprintf(stderr, "dup type requires 1 operand in stack\n");
        return false;
    }

    Value *value = NULL;
    value = resolve_value_if_var(vars, ray_pop(stack), "dup");
    if (value == NULL) {
        return false;
    }

    Value* dup1;
    Value* dup2;

    switch (value->type) {
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
            fprintf(stderr, "dup type requires 1 operand in stack\n");
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
        fprintf(stderr, "to_string type requires 1 operand in stack\n");
        return false;
    }

    Value *value = NULL;
    value = resolve_value_if_var(vars, ray_pop(stack), "to_string");
    if (value == NULL) {
        return false;
    }

    Value* converted;

    switch (value->type) {
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
            fprintf(stderr, "to_string type requires 1 operand in stack\n");
            free_value(value);
            return false;
        }
    }

    ray_append(stack, value);
    ray_append(stack, converted);
    return true;
}

static bool apply_append(RDNState *stack, Vars *vars) {
    Value *item = NULL;
    Value *target = NULL;
    Vars_t *entry = NULL;
    Value *item_copy = NULL;

    if (stack->count < 2) {
        fprintf(stderr, "append requires 2 operands\n");
        return false;
    }

    item = ray_pop(stack);
    target = ray_pop(stack);

    if (target->type == VALUE_AS_VAR && (entry = find_var_entry(vars, target->as.string)) != NULL) {
        if (entry->var_value->type != VALUE_LIST) {
            fprintf(stderr, "append requires list target\n");
            ray_append(stack, target);
            ray_append(stack, item);
            return false;
        }

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

    target = resolve_value_if_var(vars, target, "append");
    if (target == NULL) {
        free_value(item);
        return false;
    }

    if (target->type != VALUE_LIST) {
        fprintf(stderr, "append requires list target\n");
        ray_append(stack, target);
        ray_append(stack, item);
        return false;
    }

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

static bool apply_index(RDNState *stack, Vars *vars) {
    Value *index_value = NULL;
    Value *target = NULL;
    Vars_t *entry = NULL;
    Value *resolved_target = NULL;
    long index = 0;

    if (stack->count < 2) {
        fprintf(stderr, "index requires 2 operands\n");
        return false;
    }

    index_value = resolve_value_if_var(vars, ray_pop(stack), "index");
    if (index_value == NULL) {
        return false;
    }
    target = ray_pop(stack);

    if (!value_to_long(index_value, &index)) {
        fprintf(stderr, "index requires integer index\n");
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

    if (resolved_target->type != VALUE_LIST) {
        fprintf(stderr, "index requires list target\n");
        if (resolved_target == target) {
            ray_append(stack, target);
        } else {
            ray_append(stack, target);
        }
        ray_append(stack, index_value);
        return false;
    }

    if (index < 0 || (size_t)index >= resolved_target->as.list.count) {
        fprintf(stderr, "index out of range\n");
        if (resolved_target == target) {
            ray_append(stack, target);
        } else {
            ray_append(stack, target);
        }
        ray_append(stack, index_value);
        return false;
    }

    Value *result = clone_value(resolved_target->as.list.items[index]);
    free_value(index_value);
    if (resolved_target == target) {
        free_value(target);
    } else {
        free_value(target);
    }

    if (result == NULL) {
        fprintf(stderr, "failed to clone indexed value\n");
        return false;
    }

    ray_append(stack, result);
    return true;
}

static bool apply_remove(RDNState *stack, Vars *vars) {
    Value *index_value = NULL;
    Value *target = NULL;
    Vars_t *entry = NULL;
    Value *resolved_target = NULL;
    long index = 0;
    size_t i = 0;

    if (stack->count < 2) {
        fprintf(stderr, "remove requires 2 operands\n");
        return false;
    }

    index_value = resolve_value_if_var(vars, ray_pop(stack), "remove");
    if (index_value == NULL) {
        return false;
    }
    target = ray_pop(stack);

    if (!value_to_long(index_value, &index)) {
        fprintf(stderr, "remove requires integer index\n");
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

    if (resolved_target->type != VALUE_LIST) {
        fprintf(stderr, "remove requires list target\n");
        ray_append(stack, target);
        ray_append(stack, index_value);
        return false;
    }

    if (index < 0 || (size_t)index >= resolved_target->as.list.count) {
        fprintf(stderr, "remove index out of range\n");
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

static bool apply_len(RDNState *stack, Vars *vars) {
    Value *target = NULL;
    Vars_t *entry = NULL;
    Value *resolved_target = NULL;
    Value *result = NULL;

    if (stack->count < 1) {
        fprintf(stderr, "len requires 1 operand\n");
        return false;
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

    if (resolved_target->type != VALUE_LIST) {
        fprintf(stderr, "len requires list target\n");
        ray_append(stack, target);
        return false;
    }

    result = create_integer_value((long)resolved_target->as.list.count);
    if (resolved_target == target) {
        free_value(target);
    } else {
        free_value(target);
    }

    if (result == NULL) {
        fprintf(stderr, "failed to create len result\n");
        return false;
    }

    ray_append(stack, result);
    return true;
}

static bool skip_comment(char **cursor) {
    *cursor += 2;

    while (**cursor != '\0') {
        if ((*cursor)[0] == '*' && (*cursor)[1] == ']') {
            *cursor += 2;
            return true;
        }
        (*cursor)++;
    }

    fprintf(stderr, "unterminated comment\n");
    return false;
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

    fprintf(stderr, "unterminated string literal\n");
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

    if (is_token(token, "true")) {
        return push_value(stack, create_boolean_value(true));
    }

    if (is_token(token, "false")) {
        return push_value(stack, create_boolean_value(false));
    }

    fprintf(stderr, "unknown token: %s\n", token);
    return false;
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

    return is_token(token, "true") || is_token(token, "false");
}

static Value *parse_list_literal(char **cursor, Vars *vars) {
    Value *list = create_list_value();
    char *token = NULL;
    bool is_string = false;

    if (list == NULL) {
        return NULL;
    }

    while (true) {
        Value *item = NULL;
        Vars_t *entry = NULL;

        if (!next_token(cursor, &token, &is_string)) {
            free_value(list);
            return NULL;
        }

        if (token == NULL) {
            fprintf(stderr, "unterminated list literal\n");
            free_value(list);
            return NULL;
        }

        if (!is_string && is_token(token, ")")) {
            free(token);
            return list;
        }

        if (!is_string && is_token(token, "(")) {
            free(token);
            item = parse_list_literal(cursor, vars);
        } else if (is_value_token(token, is_string)) {
            long integer_value = 0;
            double double_value = 0;

            if (is_string) {
                item = create_string_value_copy(token);
            } else if (parse_integer_token(token, &integer_value)) {
                item = create_integer_value(integer_value);
            } else if (parse_double_token(token, &double_value)) {
                item = create_double_value(double_value);
            } else if (is_token(token, "true")) {
                item = create_boolean_value(true);
            } else if (is_token(token, "false")) {
                item = create_boolean_value(false);
            }
            free(token);
        } else if (is_identifier_token(token) && (entry = find_var_entry(vars, token)) != NULL) {
            item = clone_value(entry->var_value);
            free(token);
        } else if (is_identifier_token(token)) {
            item = create_var_name_value(token);
            free(token);
        } else {
            fprintf(stderr, "invalid list item: %s\n", token);
            free(token);
            free_value(list);
            return NULL;
        }

        if (item == NULL) {
            fprintf(stderr, "failed to allocate list item\n");
            free_value(list);
            return NULL;
        }

        ray_append(&list->as.list, item);
    }
}

static bool is_identifier_token(const char *token) {
    size_t index = 0;

    if (token == NULL || token[0] == '\0') {
        return false;
    }

    if (!(isalpha((unsigned char)token[0]) || token[0] == '_')) {
        return false;
    }

    for (index = 1; token[index] != '\0'; index++) {
        if (!(isalnum((unsigned char)token[index]) || token[index] == '_')) {
            return false;
        }
    }

    return true;
}

static bool identifier_is_save_target(char *cursor) {
    char *next = NULL;
    bool is_string = false;

    if (!next_token(&cursor, &next, &is_string)) {
        return false;
    }

    if (next == NULL) {
        return false;
    }

    if (!is_string && (is_token(next, "let") || is_token(next, "const"))) {
        free(next);
        return true;
    }

    free(next);
    return false;
}

static bool apply_let(RDNState *stack, Vars *vars) {
    Value *name = NULL;
    Value *value = NULL;

    if (stack->count < 2) {
        fprintf(stderr, "let requires 2 operands\n");
        return false;
    }

    name = ray_pop(stack);
    value = ray_pop(stack);

    if (name->type != VALUE_AS_VAR) {
        fprintf(stderr, "let requires variable name\n");
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

static bool apply_const(RDNState *stack, Vars *vars) {
    Value *name = NULL;
    Value *value = NULL;

    if (stack->count < 2) {
        fprintf(stderr, "const requires 2 operands\n");
        return false;
    }

    name = ray_pop(stack);
    value = ray_pop(stack);

    if (name->type != VALUE_AS_VAR) {
        fprintf(stderr, "const requires variable name\n");
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
static bool execute_block(RDNState *stack, Vars* vars, char **cursor, BlockStop *stop_reason, bool allow_else);

static bool apply_if(RDNState *stack, Vars* vars, char **cursor) {
    Value *condition = NULL;
    bool condition_value = false;
    BlockStop stop_reason = BLOCK_STOP_EOF;

    if (stack->count < 1) {
        fprintf(stderr, "if requires 1 operand\n");
        return false;
    }

    condition = ray_pop(stack);
    if (!value_to_boolean(condition, &condition_value)) {
        fprintf(stderr, "if requires a boolean operand\n");
        ray_append(stack, condition);
        return false;
    }

    free_value(condition);

    if (condition_value) {
        if (!vars_push_scope(vars)) {
            return false;
        }
        if (!execute_block(stack, vars, cursor, &stop_reason, true)) {
            vars_pop_scope(vars);
            return false;
        }

        if (stop_reason == BLOCK_STOP_EOF) {
            vars_pop_scope(vars);
            fprintf(stderr, "if missing end\n");
            return false;
        }

        vars_pop_scope(vars);

        if (stop_reason == BLOCK_STOP_ELSE) {
            if (!skip_block(cursor, &stop_reason, true)) {
                return false;
            }

            if (stop_reason != BLOCK_STOP_END) {
                fprintf(stderr, "else missing end\n");
                return false;
            }
        }

        return true;
    }

    if (!skip_block(cursor, &stop_reason, true)) {
        return false;
    }

    if (stop_reason == BLOCK_STOP_EOF) {
        fprintf(stderr, "if missing end\n");
        return false;
    }

    if (stop_reason == BLOCK_STOP_ELSE) {
        if (!vars_push_scope(vars)) {
            return false;
        }
        if (!execute_block(stack, vars, cursor, &stop_reason, true)) {
            vars_pop_scope(vars);
            return false;
        }

        if (stop_reason != BLOCK_STOP_END) {
            vars_pop_scope(vars);
            fprintf(stderr, "else missing end\n");
            return false;
        }

        vars_pop_scope(vars);
        return true;
    }

    return true;
}

static bool execute_block(RDNState *stack, Vars* vars, char **cursor, BlockStop *stop_reason, bool allow_else) {

    if (vars == NULL){
        *vars = (Vars){0};
    }

    if (stack == NULL){
        *stack = (RDNState){0};
    }

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
            Value *list_value = parse_list_literal(cursor, vars);
            free(token);
            if (list_value == NULL) {
                return false;
            }
            ray_append(stack, list_value);
            continue;
        } else if (is_token(token, "else")) {
            free(token);
            if (!allow_else) {
                fprintf(stderr, "unexpected else\n");
                return false;
            }
            *stop_reason = BLOCK_STOP_ELSE;
            return true;
        } else if (is_token(token, "end")) {
            free(token);
            *stop_reason = BLOCK_STOP_END;
            return true;
        } else if (is_token(token, "if")) {
            free(token);
            if (!apply_if(stack, vars, cursor)) {
                return false;
            }
            continue;
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
            if (!apply_type(stack, vars)) {
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
        } else if (is_identifier_token(token)) {
            Value *resolved = NULL;
            Vars_t *entry = NULL;

            if (identifier_is_save_target(*cursor)) {
                resolved = create_var_name_value(token);
            } else if ((entry = find_var_entry(vars, token)) != NULL && entry->var_value->type == VALUE_LIST) {
                resolved = create_var_name_value(token);
            } else if ((entry = find_var_entry(vars, token)) != NULL) {
                resolved = clone_value(entry->var_value);
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
            fprintf(stderr, "unknown token: %s\n", token);
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
        fprintf(stderr, "if missing end\n");
        return false;
    }

    if (stop_reason == BLOCK_STOP_ELSE) {
        if (!skip_block(cursor, &stop_reason, true)) {
            return false;
        }

        if (stop_reason != BLOCK_STOP_END) {
            fprintf(stderr, "else missing end\n");
            return false;
        }
    }

    return true;
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
                fprintf(stderr, "unexpected else\n");
                return false;
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

        free(token);
    }
}

static bool evaluate_source(RDNState *stack, Vars* vars, char *source) {
    BlockStop stop_reason = BLOCK_STOP_EOF;
    char *cursor = source;

    if (!execute_block(stack, vars, &cursor, &stop_reason, false)) {
        return false;
    }

    if (stop_reason != BLOCK_STOP_EOF) {
        fprintf(stderr, "unexpected block terminator\n");
        return false;
    }

    return true;
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    char *buffer = NULL;
    long length = 0;
    size_t bytes_read = 0;

    if (file == NULL) {
        fprintf(stderr, "failed to open '%s'\n", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "failed to seek '%s'\n", path);
        fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0) {
        fprintf(stderr, "failed to read size of '%s'\n", path);
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "failed to rewind '%s'\n", path);
        fclose(file);
        return NULL;
    }

    buffer = malloc((size_t)length + 1);
    if (buffer == NULL) {
        fprintf(stderr, "failed to allocate file buffer\n");
        fclose(file);
        return NULL;
    }

    bytes_read = fread(buffer, 1, (size_t)length, file);
    if (bytes_read != (size_t)length) {
        fprintf(stderr, "failed to read '%s'\n", path);
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[length] = '\0';
    fclose(file);
    return buffer;
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

static bool source_has_complete_blocks(const char *source, bool *out_complete) {
    char *cursor = (char *)source;
    char *token = NULL;
    bool is_string = false;
    int depth = 0;

    while (true) {
        if (!next_token(&cursor, &token, &is_string)) {
            return false;
        }

        if (token == NULL) {
            *out_complete = (depth == 0);
            return true;
        }

        if (!is_string && is_token(token, "if")) {
            depth++;
        } else if (!is_string && is_token(token, "end")) {
            depth--;
            if (depth < 0) {
                fprintf(stderr, "unexpected end\n");
                free(token);
                return false;
            }
        } else if (!is_string && is_token(token, "else") && depth == 0) {
            fprintf(stderr, "unexpected else\n");
            free(token);
            return false;
        }

        free(token);
    }
}

static int run_repl(void) {
    RDNState stack = {0};
    Vars vars = {0};
    char line[4096];
    char *source = NULL;
    size_t source_length = 0;
    bool complete = true;

    printf("raden repl\n");
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
            free_stack_values(&stack);
            free_vars(&vars);
            return EXIT_FAILURE;
        }

        if (!source_has_complete_blocks(source, &complete)) {
            free(source);
            source = NULL;
            source_length = 0;
            free_stack_values(&stack);
            stack = (RDNState){0};
            continue;
        }

        if (!complete) {
            continue;
        }

        if (!evaluate_source(&stack, &vars, source)) {
            free_stack_values(&stack);
            stack = (RDNState){0};
        }

        free(source);
        source = NULL;
        source_length = 0;
    }

    free(source);
    free_stack_values(&stack);
    free_vars(&vars);
    return EXIT_SUCCESS;
}
