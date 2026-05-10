#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "./src/stack.h"

typedef enum {
    VALUE_INTEGER,
    VALUE_DOUBLE,
    VALUE_STRING,
    VALUE_BOOLEAN,
} ValueType;

typedef struct {
    ValueType type;
    union {
        long integer;
        double number;
        char *string;
        bool boolean;
    } as;
} Value;

typedef RLStack(Value *) MainStack;

static bool is_token(const char *value, const char *expected) {
    return strcmp(value, expected) == 0;
}

// TODO: implement equal "="
// TODO: implement not "!"
static bool is_operator_token(const char *value) {
    return is_token(value, "+") || is_token(value, "-") || is_token(value, "*") ||
           is_token(value, "/") || is_token(value, "<<") || is_token(value, ">>") ||
           is_token(value, "|") || is_token(value, "&") || is_token(value, "^");
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

static void free_value(Value *value) {
    if (value == NULL) {
        return;
    }

    if (value->type == VALUE_STRING) {
        free(value->as.string);
    }

    free(value);
}

static void free_stack_values(MainStack *stack) {
    while (!ray_is_empty(stack)) {
        free_value(ray_pop(stack));
    }

    ray_clear(stack);
}

static bool push_value(MainStack *stack, Value *value) {
    if (value == NULL) {
        fprintf(stderr, "failed to allocate value\n");
        return false;
    }

    ray_append(stack, value);
    return true;
}

static bool parse_integer_token(const char *text, long *out_value) {
    char *end = NULL;
    long value = 0;

    if (text == NULL || *text == '\0') {
        return false;
    }

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || *end != '\0') {
        return false;
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

static void print_value(const Value *value) {
    if (value->type == VALUE_INTEGER) {
        printf("%ld\n", value->as.integer);
        return;
    }

    if (value->type == VALUE_DOUBLE) {
        printf("%.15g\n", value->as.number);
        return;
    }

    if (value->type == VALUE_BOOLEAN) {
        printf("%s\n", value->as.boolean ? "true" : "false");
        return;
    }

    printf("%s\n", value->as.string);
}

static bool apply_binary_operator(MainStack *stack, const char *operator_token) {
    Value *left = NULL;
    Value *right = NULL;
    Value *result = NULL;
    double left_double = 0;
    double right_double = 0;
    long left_long = 0;
    long right_long = 0;
    bool left_bool = false;
    bool right_bool = false;

    if (stack->count < 2) {
        fprintf(stderr, "operator '%s' requires 2 operands, got %zu\n", operator_token, stack->count);
        return false;
    }

    right = ray_pop(stack);
    left = ray_pop(stack);

    if (is_token(operator_token, "|") || is_token(operator_token, "&")) {
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

static bool apply_print(MainStack *stack) {
    Value *value = NULL;

    if (stack->count < 1) {
        fprintf(stderr, "print requires 1 operand\n");
        return false;
    }

    value = ray_pop(stack);
    print_value(value);
    free_value(value);
    return true;
}

static bool apply_type(MainStack *stack) {
    Value *value = NULL;
    Value *result = NULL;

    if (stack->count < 1) {
        fprintf(stderr, "type requires 1 operand\n");
        return false;
    }

    value = ray_pop(stack);

    if (value->type == VALUE_INTEGER) {
        result = create_string_value_copy("integer");
    } else if (value->type == VALUE_DOUBLE) {
        result = create_string_value_copy("double");
    } else if (value->type == VALUE_BOOLEAN) {
        result = create_string_value_copy("boolean");
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

    while ((*cursor)[length] != '\0' && !isspace((unsigned char)(*cursor)[length])) {
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

static bool push_token_value(MainStack *stack, const char *token, bool is_string) {
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

static bool evaluate_source(MainStack *stack, char *source) {
    char *cursor = source;
    char *token = NULL;
    bool is_string = false;

    while (true) {
        if (!next_token(&cursor, &token, &is_string)) {
            return false;
        }

        if (token == NULL) {
            return true;
        }

        if (is_operator_token(token)) {
            if (!apply_binary_operator(stack, token)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        }

        if (is_token(token, "print")) {
            if (!apply_print(stack)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        }

        if (is_token(token, "type")) {
            if (!apply_type(stack)) {
                free(token);
                return false;
            }
            free(token);
            continue;
        }

        if (!push_token_value(stack, token, is_string)) {
            free(token);
            return false;
        }

        free(token);
    }
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

int main(int argc, char **argv) {
    const char *path = "./test/booleans.rdn";
    char *source = NULL;
    MainStack stack = {0};
    int exit_code = EXIT_FAILURE;

    if (argc > 1) {
        path = argv[1];
    }

    source = read_file(path);
    if (source == NULL) {
        return exit_code;
    }

    if (!evaluate_source(&stack, source)) {
        free(source);
        free_stack_values(&stack);
        return exit_code;
    }

    if (stack.count != 0) {
        fprintf(stderr, "unexpected values left on stack: %zu\n", stack.count);
        free(source);
        free_stack_values(&stack);
        return exit_code;
    }

    free(source);
    free_stack_values(&stack);
    return EXIT_SUCCESS;
}
