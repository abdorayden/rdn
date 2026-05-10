#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./src/stack.h"

typedef RLStack(char *) MainStack;

static bool is_token(const char *value, const char *expected) {
    return strcmp(value, expected) == 0;
}

static bool is_operator_token(const char *value) {
    return is_token(value, "+") || is_token(value, "-") || is_token(value, "*") ||
           is_token(value, "/") || is_token(value, "<<") || is_token(value, ">>") ||
           is_token(value, "|") || is_token(value, "&") || is_token(value, "^");
}

static bool parse_long(const char *text, long *out_value) {
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

static char *copy_string(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length);
    return copy;
}

static char *long_to_string(long value) {
    int length = snprintf(NULL, 0, "%ld", value);
    char *buffer = NULL;

    if (length < 0) {
        return NULL;
    }

    buffer = malloc((size_t)length + 1);
    if (buffer == NULL) {
        return NULL;
    }

    snprintf(buffer, (size_t)length + 1, "%ld", value);
    return buffer;
}

static void free_stack_strings(MainStack *stack) {
    while (!ray_is_empty(stack)) {
        free(ray_pop(stack));
    }

    ray_clear(stack);
}

static bool push_owned_token(MainStack *stack, const char *token) {
    char *owned_token = copy_string(token);

    if (owned_token == NULL) {
        fprintf(stderr, "failed to allocate token\n");
        return false;
    }

    ray_append(stack, owned_token);
    return true;
}

static bool apply_binary_operator(MainStack *stack, const char *operator_token) {
    long left = 0;
    long right = 0;
    char *left_token = NULL;
    char *right_token = NULL;
    char *result_token = NULL;

    if (stack->count < 2) {
        fprintf(stderr, "operator '%s' requires 2 operands, got %zu\n", operator_token, stack->count);
        return false;
    }

    right_token = ray_pop(stack);
    left_token = ray_pop(stack);

    if (!parse_long(left_token, &left) || !parse_long(right_token, &right)) {
        fprintf(stderr, "expected numeric operands for '%s'\n", operator_token);
        ray_append(stack, left_token);
        ray_append(stack, right_token);
        return false;
    }

    if (is_token(operator_token, "+")) {
        result_token = long_to_string(left + right);
    } else if (is_token(operator_token, "-")) {
        result_token = long_to_string(left - right);
    } else if (is_token(operator_token, "*")) {
        result_token = long_to_string(left * right);
    } else if (is_token(operator_token, "/")) {
        if (right == 0) {
            fprintf(stderr, "division by zero\n");
            ray_append(stack, left_token);
            ray_append(stack, right_token);
            return false;
        }
        result_token = long_to_string(left / right);
    } else if (is_token(operator_token, "<<")) {
        if (right < 0) {
            fprintf(stderr, "left shift requires a non-negative count\n");
            ray_append(stack, left_token);
            ray_append(stack, right_token);
            return false;
        }
        result_token = long_to_string(left << right);
    } else if (is_token(operator_token, ">>")) {
        if (right < 0) {
            fprintf(stderr, "right shift requires a non-negative count\n");
            ray_append(stack, left_token);
            ray_append(stack, right_token);
            return false;
        }
        result_token = long_to_string(left >> right);
    } else if (is_token(operator_token, "|")) {
        result_token = long_to_string(left | right);
    } else if (is_token(operator_token, "&")) {
        result_token = long_to_string(left & right);
    } else if (is_token(operator_token, "^")) {
        result_token = long_to_string(left ^ right);
    } else {
        fprintf(stderr, "unsupported operator: %s\n", operator_token);
        ray_append(stack, left_token);
        ray_append(stack, right_token);
        return false;
    }

    if (result_token == NULL) {
        fprintf(stderr, "failed to allocate result\n");
        ray_append(stack, left_token);
        ray_append(stack, right_token);
        return false;
    }

    free(left_token);
    free(right_token);
    ray_append(stack, result_token);
    return true;
}

static bool apply_print(MainStack *stack) {
    char *value = NULL;

    if (stack->count < 1) {
        fprintf(stderr, "print requires 1 operand\n");
        return false;
    }

    value = ray_pop(stack);
    printf("%s\n", value);
    free(value);
    return true;
}

static bool strip_comments(char *source) {
    char *cursor = source;

    while (*cursor != '\0') {
        if (cursor[0] == '[' && cursor[1] == '*') {
            cursor[0] = ' ';
            cursor[1] = ' ';
            cursor += 2;

            while (!(cursor[0] == '*' && cursor[1] == ']')) {
                if (cursor[0] == '\0' || cursor[1] == '\0') {
                    fprintf(stderr, "unterminated comment\n");
                    return false;
                }
                *cursor = ' ';
                cursor++;
            }

            cursor[0] = ' ';
            cursor[1] = ' ';
            cursor += 2;
            continue;
        }

        cursor++;
    }

    return true;
}

static bool evaluate_source(MainStack *stack, char *source) {
    char *token = NULL;
    long numeric_value = 0;

    if (!strip_comments(source)) {
        return false;
    }

    for (token = strtok(source, " \t\r\n"); token != NULL; token = strtok(NULL, " \t\r\n")) {
        if (parse_long(token, &numeric_value)) {
            if (!push_owned_token(stack, token)) {
                return false;
            }
            continue;
        }

        if (is_operator_token(token)) {
            if (!apply_binary_operator(stack, token)) {
                return false;
            }
            continue;
        }

        if (is_token(token, "print")) {
            if (!apply_print(stack)) {
                return false;
            }
            continue;
        }

        fprintf(stderr, "unknown token: %s\n", token);
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

int main(void) {
    const char *path = "./test/operations.rdn";
    char *source = NULL;
    MainStack stack = {0};
    int exit_code = EXIT_FAILURE;

    source = read_file(path);
    if (source == NULL) {
        return exit_code;
    }

    if (!evaluate_source(&stack, source)) {
        free(source);
        free_stack_strings(&stack);
        return exit_code;
    }

    if (stack.count != 0) {
        fprintf(stderr, "unexpected values left on stack: %zu\n", stack.count);
        free(source);
        free_stack_strings(&stack);
        return exit_code;
    }

    free(source);
    free_stack_strings(&stack);
    return EXIT_SUCCESS;
}
