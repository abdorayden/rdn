/*
 * json.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Purpose:
 * Native JSON parsing and serialization helpers for Raden.
 *
 * Conventions:
 * - JSON arrays become normal Raden lists.
 * - JSON objects become lists of 2-item lists: ((key value) (key value)).
 * - JSON null/booleans/numbers/strings map to existing scalar Raden values.
 *
 * Exports:
 * - parseJson(text) -> value
 * - stringifyJson(value) -> string
 */

#define _POSIX_C_SOURCE 200809L

#include "../include/rdn_native.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct JsonParser {
    RDNApi *api;
    const char *cursor;
} JsonParser;

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

static bool append_char(char **buffer, size_t *length, char ch) {
    char *grown = realloc(*buffer, *length + 2);
    if (grown == NULL) {
        return false;
    }
    grown[*length] = ch;
    grown[*length + 1] = '\0';
    *buffer = grown;
    *length += 1;
    return true;
}

static void skip_ws(JsonParser *parser) {
    while (*parser->cursor != '\0' && isspace((unsigned char)*parser->cursor)) {
        parser->cursor++;
    }
}

static bool parse_value(JsonParser *parser);

static bool parse_literal(JsonParser *parser, const char *literal) {
    size_t length = strlen(literal);
    if (strncmp(parser->cursor, literal, length) != 0) {
        return false;
    }
    parser->cursor += length;
    return true;
}

static bool parse_string_raw(JsonParser *parser, char **out_text) {
    char *buffer = NULL;
    size_t length = 0;

    if (*parser->cursor != '"') {
        return parser->api->raise_error(parser->api, "expected JSON string");
    }

    parser->cursor++;

    while (*parser->cursor != '\0') {
        char ch = *parser->cursor++;

        if (ch == '"') {
            if (buffer == NULL) {
                buffer = malloc(1);
                if (buffer == NULL) {
                    return parser->api->raise_error(parser->api, "failed to allocate JSON string");
                }
                buffer[0] = '\0';
            }
            *out_text = buffer;
            return true;
        }

        if (ch == '\\') {
            char escaped = *parser->cursor++;
            if (escaped == '\0') {
                free(buffer);
                return parser->api->raise_error(parser->api, "unterminated JSON escape");
            }
            switch (escaped) {
                case '"': ch = '"'; break;
                case '\\': ch = '\\'; break;
                case '/': ch = '/'; break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case 'u': {
                    int code = 0;
                    int i = 0;
                    for (i = 0; i < 4; i++) {
                        char hex = parser->cursor[i];
                        if (hex >= '0' && hex <= '9') {
                            code = (code << 4) | (hex - '0');
                        } else if (hex >= 'A' && hex <= 'F') {
                            code = (code << 4) | (hex - 'A' + 10);
                        } else if (hex >= 'a' && hex <= 'f') {
                            code = (code << 4) | (hex - 'a' + 10);
                        } else {
                            free(buffer);
                            return parser->api->raise_error(parser->api, "invalid JSON unicode escape");
                        }
                    }
                    parser->cursor += 4;
                    ch = code >= 0 && code <= 0x7F ? (char)code : '?';
                    break;
                }
                default:
                    free(buffer);
                    return parser->api->raise_error(parser->api, "invalid JSON escape");
            }
        }

        if (!append_char(&buffer, &length, ch)) {
            free(buffer);
            return parser->api->raise_error(parser->api, "failed to grow JSON string");
        }
    }

    free(buffer);
    return parser->api->raise_error(parser->api, "unterminated JSON string");
}

static bool parse_string(JsonParser *parser) {
    char *text = NULL;
    bool ok = parse_string_raw(parser, &text);
    if (!ok) {
        return false;
    }
    ok = parser->api->push_string(parser->api, text);
    free(text);
    return ok;
}

static bool parse_number(JsonParser *parser) {
    const char *start = parser->cursor;
    char *end = NULL;
    double number = 0;
    bool is_integer = true;

    if (*parser->cursor == '-') {
        parser->cursor++;
    }

    if (!isdigit((unsigned char)*parser->cursor)) {
        return parser->api->raise_error(parser->api, "invalid JSON number");
    }

    if (*parser->cursor == '0') {
        parser->cursor++;
    } else {
        while (isdigit((unsigned char)*parser->cursor)) {
            parser->cursor++;
        }
    }

    if (*parser->cursor == '.') {
        is_integer = false;
        parser->cursor++;
        if (!isdigit((unsigned char)*parser->cursor)) {
            return parser->api->raise_error(parser->api, "invalid JSON number");
        }
        while (isdigit((unsigned char)*parser->cursor)) {
            parser->cursor++;
        }
    }

    if (*parser->cursor == 'e' || *parser->cursor == 'E') {
        is_integer = false;
        parser->cursor++;
        if (*parser->cursor == '+' || *parser->cursor == '-') {
            parser->cursor++;
        }
        if (!isdigit((unsigned char)*parser->cursor)) {
            return parser->api->raise_error(parser->api, "invalid JSON number");
        }
        while (isdigit((unsigned char)*parser->cursor)) {
            parser->cursor++;
        }
    }

    errno = 0;
    number = strtod(start, &end);
    if (errno == ERANGE || end != parser->cursor) {
        return parser->api->raise_error(parser->api, "invalid JSON number");
    }

    if (is_integer) {
        long integer = strtol(start, NULL, 10);
        if (!(integer == LONG_MIN || integer == LONG_MAX) || errno != ERANGE) {
            return parser->api->push_integer(parser->api, integer);
        }
        errno = 0;
    }

    return parser->api->push_number(parser->api, number);
}

static bool parse_array(JsonParser *parser) {
    if (*parser->cursor != '[') {
        return parser->api->raise_error(parser->api, "expected JSON array");
    }

    parser->cursor++;
    skip_ws(parser);

    if (!parser->api->push_list(parser->api)) {
        return false;
    }

    if (*parser->cursor == ']') {
        parser->cursor++;
        return true;
    }

    while (true) {
        if (!parse_value(parser)) {
            return false;
        }
        if (!parser->api->list_append(parser->api, -2, -1)) {
            return false;
        }
        if (!parser->api->pop(parser->api, 1)) {
            return false;
        }

        skip_ws(parser);
        if (*parser->cursor == ']') {
            parser->cursor++;
            return true;
        }
        if (*parser->cursor != ',') {
            return parser->api->raise_error(parser->api, "expected ',' or ']' in JSON array");
        }
        parser->cursor++;
        skip_ws(parser);
    }
}

static bool parse_object(JsonParser *parser) {
    if (*parser->cursor != '{') {
        return parser->api->raise_error(parser->api, "expected JSON object");
    }

    parser->cursor++;
    skip_ws(parser);

    if (!parser->api->push_list(parser->api)) {
        return false;
    }

    if (*parser->cursor == '}') {
        parser->cursor++;
        return true;
    }

    while (true) {
        char *key = NULL;

        if (!parse_string_raw(parser, &key)) {
            return false;
        }

        skip_ws(parser);
        if (*parser->cursor != ':') {
            free(key);
            return parser->api->raise_error(parser->api, "expected ':' in JSON object");
        }
        parser->cursor++;
        skip_ws(parser);

        if (!parser->api->push_list(parser->api)) {
            free(key);
            return false;
        }

        if (!parser->api->push_string(parser->api, key)) {
            free(key);
            return false;
        }
        free(key);

        if (!parser->api->list_append(parser->api, -2, -1)) {
            return false;
        }
        if (!parser->api->pop(parser->api, 1)) {
            return false;
        }

        if (!parse_value(parser)) {
            return false;
        }
        if (!parser->api->list_append(parser->api, -2, -1)) {
            return false;
        }
        if (!parser->api->pop(parser->api, 1)) {
            return false;
        }

        if (!parser->api->list_append(parser->api, -2, -1)) {
            return false;
        }
        if (!parser->api->pop(parser->api, 1)) {
            return false;
        }

        skip_ws(parser);
        if (*parser->cursor == '}') {
            parser->cursor++;
            return true;
        }
        if (*parser->cursor != ',') {
            return parser->api->raise_error(parser->api, "expected ',' or '}' in JSON object");
        }
        parser->cursor++;
        skip_ws(parser);
    }
}

static bool parse_value(JsonParser *parser) {
    skip_ws(parser);

    if (*parser->cursor == '"') {
        return parse_string(parser);
    }
    if (*parser->cursor == '{') {
        return parse_object(parser);
    }
    if (*parser->cursor == '[') {
        return parse_array(parser);
    }
    if (*parser->cursor == 'n') {
        if (!parse_literal(parser, "null")) {
            return parser->api->raise_error(parser->api, "invalid JSON literal");
        }
        return parser->api->push_null(parser->api);
    }
    if (*parser->cursor == 't') {
        if (!parse_literal(parser, "true")) {
            return parser->api->raise_error(parser->api, "invalid JSON literal");
        }
        return parser->api->push_boolean(parser->api, true);
    }
    if (*parser->cursor == 'f') {
        if (!parse_literal(parser, "false")) {
            return parser->api->raise_error(parser->api, "invalid JSON literal");
        }
        return parser->api->push_boolean(parser->api, false);
    }
    if (*parser->cursor == '-' || isdigit((unsigned char)*parser->cursor)) {
        return parse_number(parser);
    }

    return parser->api->raise_error(parser->api, "invalid JSON value");
}

static bool parseJson(RDNApi *api) {
    JsonParser parser;
    const char *text = NULL;
    char *owned_text = NULL;
    size_t text_length = 0;
    bool ok = false;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "parseJson requires 1 param");
    }
    if (api->type(api, -1) != RDN_VALUE_STRING) {
        return api->raise_error(api, "parseJson requires string text");
    }

    text = api->to_string(api, -1);
    if (text == NULL) {
        return api->raise_error(api, "parseJson requires string text");
    }

    text_length = strlen(text);
    owned_text = malloc(text_length + 1);
    if (owned_text == NULL) {
        return api->raise_error(api, "parseJson failed to allocate input buffer");
    }
    memcpy(owned_text, text, text_length + 1);

    if (!api->pop(api, 1)) {
        free(owned_text);
        return false;
    }

    parser.api = api;
    parser.cursor = owned_text;

    if (!parse_value(&parser)) {
        free(owned_text);
        return false;
    }

    skip_ws(&parser);
    if (*parser.cursor != '\0') {
        free(owned_text);
        return api->raise_error(api, "unexpected trailing JSON content");
    }

    ok = true;
    free(owned_text);
    return ok;
}

static bool append_json_string(char **buffer, size_t *length, const char *text) {
    size_t i = 0;
    if (!append_char(buffer, length, '"')) {
        return false;
    }
    for (i = 0; text[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)text[i];
        switch (ch) {
            case '"': if (!append_text(buffer, length, "\\\"")) return false; break;
            case '\\': if (!append_text(buffer, length, "\\\\")) return false; break;
            case '\b': if (!append_text(buffer, length, "\\b")) return false; break;
            case '\f': if (!append_text(buffer, length, "\\f")) return false; break;
            case '\n': if (!append_text(buffer, length, "\\n")) return false; break;
            case '\r': if (!append_text(buffer, length, "\\r")) return false; break;
            case '\t': if (!append_text(buffer, length, "\\t")) return false; break;
            default:
                if (ch < 0x20) {
                    char escaped[7];
                    snprintf(escaped, sizeof(escaped), "\\u%04X", ch);
                    if (!append_text(buffer, length, escaped)) {
                        return false;
                    }
                } else if (!append_char(buffer, length, (char)ch)) {
                    return false;
                }
        }
    }
    return append_char(buffer, length, '"');
}

static bool value_is_object_like(RDNApi *api, long index) {
    size_t count = 0;
    size_t i = 0;

    if (!api->list_len(api, index, &count)) {
        return false;
    }
    if (count == 0) {
        return false;
    }

    for (i = 0; i < count; i++) {
        size_t pair_len = 0;
        bool ok = false;
        if (!api->list_index(api, index, (long)i)) {
            return false;
        }
        if (api->type(api, -1) != RDN_VALUE_LIST) {
            api->pop(api, 1);
            return false;
        }
        if (!api->list_len(api, -1, &pair_len) || pair_len != 2) {
            api->pop(api, 1);
            return false;
        }
        if (!api->list_index(api, -1, 0)) {
            api->pop(api, 1);
            return false;
        }
        ok = api->type(api, -1) == RDN_VALUE_STRING;
        if (!api->pop(api, 2)) {
            return false;
        }
        if (!ok) {
            return false;
        }
    }

    return true;
}

static bool append_json_value(RDNApi *api, long index, char **buffer, size_t *length);

static bool append_json_list(RDNApi *api, long index, char **buffer, size_t *length) {
    size_t count = 0;
    size_t i = 0;
    bool object_like = value_is_object_like(api, index);

    if (!api->list_len(api, index, &count)) {
        return false;
    }

    if (!append_char(buffer, length, object_like ? '{' : '[')) {
        return false;
    }

    for (i = 0; i < count; i++) {
        if (i > 0 && !append_char(buffer, length, ',')) {
            return false;
        }

        if (!api->list_index(api, index, (long)i)) {
            return false;
        }

        if (object_like) {
            if (!api->list_index(api, -1, 0)) {
                api->pop(api, 1);
                return false;
            }
            if (!append_json_string(buffer, length, api->to_string(api, -1))) {
                api->pop(api, 2);
                return false;
            }
            if (!append_char(buffer, length, ':')) {
                api->pop(api, 2);
                return false;
            }
            if (!api->pop(api, 1)) {
                api->pop(api, 1);
                return false;
            }
            if (!api->list_index(api, -1, 1)) {
                api->pop(api, 1);
                return false;
            }
            if (!append_json_value(api, -1, buffer, length)) {
                api->pop(api, 2);
                return false;
            }
            if (!api->pop(api, 2)) {
                return false;
            }
        } else {
            if (!append_json_value(api, -1, buffer, length)) {
                api->pop(api, 1);
                return false;
            }
            if (!api->pop(api, 1)) {
                return false;
            }
        }
    }

    return append_char(buffer, length, object_like ? '}' : ']');
}

static bool append_json_value(RDNApi *api, long index, char **buffer, size_t *length) {
    char number[64];
    long integer = 0;
    double real = 0;
    bool boolean = false;
    const char *string = NULL;

    switch (api->type(api, index)) {
        case RDN_VALUE_NULL:
            return append_text(buffer, length, "null");
        case RDN_VALUE_INTEGER:
            if (!api->to_integer(api, index, &integer)) {
                return false;
            }
            snprintf(number, sizeof(number), "%ld", integer);
            return append_text(buffer, length, number);
        case RDN_VALUE_DOUBLE:
            if (!api->to_number(api, index, &real)) {
                return false;
            }
            snprintf(number, sizeof(number), "%.15g", real);
            return append_text(buffer, length, number);
        case RDN_VALUE_BOOLEAN:
            if (!api->to_boolean(api, index, &boolean)) {
                return false;
            }
            return append_text(buffer, length, boolean ? "true" : "false");
        case RDN_VALUE_STRING:
            string = api->to_string(api, index);
            if (string == NULL) {
                return false;
            }
            return append_json_string(buffer, length, string);
        case RDN_VALUE_LIST:
            return append_json_list(api, index, buffer, length);
        default:
            return api->raise_error(api, "stringifyJson cannot serialize this value");
    }
}

static bool stringifyJson(RDNApi *api) {
    char *buffer = NULL;
    size_t length = 0;
    bool ok = false;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "stringifyJson requires 1 param");
    }

    buffer = malloc(1);
    if (buffer == NULL) {
        return api->raise_error(api, "stringifyJson failed to allocate buffer");
    }
    buffer[0] = '\0';

    if (!append_json_value(api, -1, &buffer, &length)) {
        free(buffer);
        return false;
    }

    if (!api->pop(api, 1)) {
        free(buffer);
        return false;
    }

    ok = api->push_string(api, buffer);
    free(buffer);
    return ok;
}

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "parseJson", parseJson)) {
        return false;
    }
    if (!module->register_function(module, "stringifyJson", stringifyJson)) {
        return false;
    }
    return true;
}
