/*
 * sqlite3.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Native SQLite module for Raden backed by the public-domain amalgamation
 * in src/sqlite-amalgamation-3530400/.
 *
 * A connection value is a Raden list holding a single integer id:
 *
 *     (conn_id)
 *
 * A cursor value likewise holds its own id:
 *
 *     (cursor_id)
 *
 * Exported Raden functions:
 *   __m__sqlite3__open          ( path -- (conn_id) )
 *   __m__sqlite3__exec          ( conn sql -- )
 *   __m__sqlite3__cursor        ( conn sql -- (cursor_id) )
 *   __m__sqlite3__bind          ( cur (params) -- )
 *   __m__sqlite3__step          ( cur -- (row) | null )
 *   __m__sqlite3__close_cursor  ( cur -- )
 *   __m__sqlite3__close         ( conn -- )
 */

#include "../include/rdn.h"
#include "../include/rdn_native.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/sqlite-amalgamation-3530400/sqlite3.h"

typedef struct {
    long id;
    sqlite3 *db;
} SqliteConn;

typedef struct {
    long id;
    long conn_id;
    sqlite3_stmt *stmt;
    bool exhausted;
} SqliteCursor;

static SqliteConn *g_conns = NULL;
static size_t g_conn_count = 0;
static size_t g_conn_capacity = 0;

static SqliteCursor *g_cursors = NULL;
static size_t g_cursor_count = 0;
static size_t g_cursor_capacity = 0;

static long g_next_id = 1;

static RDNState *sql_stack(RDNApi *api) {
    NativeCallState *state = (NativeCallState *)api->userdata;
    return state->stack;
}

static Value *sql_resolve(RDNApi *api, Value *value) {
    if (value != NULL && value->type == VALUE_AS_VAR) {
        return api->resolve_variable(api, value->as.string);
    }
    return value;
}

static bool sql_error(RDNApi *api, const char *message) {
    return api->raise_error(api, message);
}

static bool sql_error_db(RDNApi *api, sqlite3 *db, const char *context) {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s: %s", context,
             db != NULL ? sqlite3_errmsg(db) : "out of memory");
    return api->raise_error(api, buffer);
}

static bool sql_push_handle(RDNApi *api, long id) {
    if (!api->push_list(api)) {
        return false;
    }
    if (!api->push_integer(api, id)) {
        return false;
    }
    if (!api->list_append(api, -2, -1)) {
        return false;
    }
    return api->pop(api, 1);
}

/* Pull the numeric id out of a (id) handle list. */
static bool sql_handle_id(Value *value, long *out_id) {
    if (value == NULL || value->type != VALUE_LIST || value->as.list.count < 1) {
        return false;
    }
    if (value->as.list.items[0] == NULL ||
        value->as.list.items[0]->type != VALUE_INTEGER) {
        return false;
    }
    *out_id = value->as.list.items[0]->as.integer;
    return true;
}

static SqliteConn *sql_conn_register(void) {
    SqliteConn *slot = NULL;

    if (g_conn_count == g_conn_capacity) {
        size_t new_capacity = g_conn_capacity == 0 ? 8 : g_conn_capacity * 2;
        SqliteConn *grown = realloc(g_conns, new_capacity * sizeof(*grown));
        if (grown == NULL) {
            return NULL;
        }
        g_conns = grown;
        g_conn_capacity = new_capacity;
    }

    slot = &g_conns[g_conn_count];
    slot->id = g_next_id++;
    slot->db = NULL;
    g_conn_count++;
    return slot;
}

static SqliteConn *sql_conn_find(long id) {
    size_t index = 0;

    for (index = 0; index < g_conn_count; index++) {
        if (g_conns[index].id == id) {
            return &g_conns[index];
        }
    }
    return NULL;
}

static void sql_conn_remove(SqliteConn *slot) {
    if (slot != NULL && g_conn_count > 0) {
        *slot = g_conns[g_conn_count - 1];
        g_conn_count--;
    }
}

static SqliteCursor *sql_cursor_register(long conn_id, sqlite3_stmt *stmt) {
    SqliteCursor *slot = NULL;

    if (g_cursor_count == g_cursor_capacity) {
        size_t new_capacity = g_cursor_capacity == 0 ? 8 : g_cursor_capacity * 2;
        SqliteCursor *grown = realloc(g_cursors, new_capacity * sizeof(*grown));
        if (grown == NULL) {
            return NULL;
        }
        g_cursors = grown;
        g_cursor_capacity = new_capacity;
    }

    slot = &g_cursors[g_cursor_count];
    slot->id = g_next_id++;
    slot->conn_id = conn_id;
    slot->stmt = stmt;
    slot->exhausted = false;
    g_cursor_count++;
    return slot;
}

static SqliteCursor *sql_cursor_find(long id) {
    size_t index = 0;

    for (index = 0; index < g_cursor_count; index++) {
        if (g_cursors[index].id == id) {
            return &g_cursors[index];
        }
    }
    return NULL;
}

static void sql_cursor_remove(SqliteCursor *slot) {
    if (slot != NULL && g_cursor_count > 0) {
        *slot = g_cursors[g_cursor_count - 1];
        g_cursor_count--;
    }
}

/*
 * Resolve a popped handle value into a live connection. Handles may arrive
 * as identifier values pointing at a variable that holds the (conn_id) list,
 * so resolve through the interpreter first.
 */
static bool sql_resolve_conn(RDNApi *api, Value *value, SqliteConn **out) {
    long id = 0;

    *out = NULL;

    value = sql_resolve(api, value);
    if (!sql_handle_id(value, &id)) {
        return sql_error(api, "Sqlite requires a connection value");
    }

    *out = sql_conn_find(id);
    if (*out == NULL) {
        return sql_error(api, "unknown connection value");
    }
    return true;
}

static bool sql_resolve_cursor(RDNApi *api, Value *value, SqliteCursor **out) {
    long id = 0;

    *out = NULL;

    value = sql_resolve(api, value);
    if (!sql_handle_id(value, &id)) {
        return sql_error(api, "Sqlite requires a cursor value");
    }

    *out = sql_cursor_find(id);
    if (*out == NULL) {
        return sql_error(api, "unknown cursor value");
    }
    return true;
}

static bool __m__sqlite3__open(RDNApi *api) {
    RDNState *stack = sql_stack(api);
    Value *path_value = NULL;
    SqliteConn *slot = NULL;
    const char *path = NULL;

    if (stack->count < 1) {
        return sql_error(api, "Sqlite::open requires a database path");
    }

    path_value = rdn_pop_value(stack);
    path_value = sql_resolve(api, path_value);

    if (path_value == NULL || path_value->type != VALUE_STRING) {
        return sql_error(api, "Sqlite::open requires a database path string");
    }

    path = path_value->as.string;

    slot = sql_conn_register();
    if (slot == NULL) {
        return sql_error(api, "failed to register connection");
    }

    if (sqlite3_open_v2(path, &slot->db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        NULL) != SQLITE_OK) {
        bool raised = sql_error_db(api, slot->db, "Sqlite::open failed");
        sqlite3_close_v2(slot->db);
        slot->db = NULL;
        sql_conn_remove(slot);
        return raised;
    }

    return sql_push_handle(api, slot->id);
}

static bool __m__sqlite3__exec(RDNApi *api) {
    RDNState *stack = sql_stack(api);
    Value *sql_value = NULL;
    Value *conn_value = NULL;
    SqliteConn *conn = NULL;
    char *errmsg = NULL;

    if (stack->count < 2) {
        return sql_error(api, "Sqlite::exec requires a connection and SQL text");
    }

    sql_value = rdn_pop_value(stack);
    conn_value = rdn_pop_value(stack);

    if (!sql_resolve_conn(api, conn_value, &conn)) {
        return false;
    }

    sql_value = sql_resolve(api, sql_value);
    if (sql_value == NULL || sql_value->type != VALUE_STRING) {
        return sql_error(api, "Sqlite::exec requires SQL text");
    }

    if (sqlite3_exec(conn->db, sql_value->as.string, NULL, NULL, &errmsg) != SQLITE_OK) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "Sqlite::exec failed: %s",
                 errmsg != NULL ? errmsg : "unknown error");
        if (errmsg != NULL) {
            sqlite3_free(errmsg);
        }
        return api->raise_error(api, buffer);
    }

    return true;
}

static bool __m__sqlite3__cursor(RDNApi *api) {
    RDNState *stack = sql_stack(api);
    Value *sql_value = NULL;
    Value *conn_value = NULL;
    SqliteConn *conn = NULL;
    SqliteCursor *slot = NULL;
    sqlite3_stmt *stmt = NULL;

    if (stack->count < 2) {
        return sql_error(api, "Sqlite::cursor requires a connection and SQL text");
    }

    sql_value = rdn_pop_value(stack);
    conn_value = rdn_pop_value(stack);

    if (!sql_resolve_conn(api, conn_value, &conn)) {
        return false;
    }

    sql_value = sql_resolve(api, sql_value);
    if (sql_value == NULL || sql_value->type != VALUE_STRING) {
        return sql_error(api, "Sqlite::cursor requires SQL text");
    }

    if (sqlite3_prepare_v2(conn->db, sql_value->as.string, -1, &stmt, NULL) != SQLITE_OK) {
        return sql_error_db(api, conn->db, "Sqlite::cursor failed");
    }

    slot = sql_cursor_register(conn->id, stmt);
    if (slot == NULL) {
        sqlite3_finalize(stmt);
        return sql_error(api, "failed to register cursor");
    }

    return sql_push_handle(api, slot->id);
}

static bool __m__sqlite3__bind(RDNApi *api) {
    RDNState *stack = sql_stack(api);
    Value *params_value = NULL;
    Value *cursor_value = NULL;
    SqliteCursor *cursor = NULL;
    size_t index = 0;

    if (stack->count < 2) {
        return sql_error(api, "Sqlite::bind requires a cursor and a parameter list");
    }

    params_value = rdn_pop_value(stack);
    cursor_value = rdn_pop_value(stack);

    if (!sql_resolve_cursor(api, cursor_value, &cursor)) {
        return false;
    }

    params_value = sql_resolve(api, params_value);
    if (params_value == NULL || params_value->type != VALUE_LIST) {
        return sql_error(api, "Sqlite::bind requires a parameter list");
    }

    /* Rebinding restarts the statement from scratch. */
    sqlite3_reset(cursor->stmt);
    cursor->exhausted = false;

    for (index = 0; index < params_value->as.list.count; index++) {
        Value *item = sql_resolve(api, params_value->as.list.items[index]);
        int rc = SQLITE_OK;
        int position = (int)index + 1;

        if (item == NULL) {
            return sql_error(api, "Sqlite::bind parameter variable not found");
        }

        switch (item->type) {
            case VALUE_INTEGER:
                rc = sqlite3_bind_int64(cursor->stmt, position, item->as.integer);
                break;
            case VALUE_DOUBLE:
                rc = sqlite3_bind_double(cursor->stmt, position, item->as.number);
                break;
            case VALUE_BOOLEAN:
                rc = sqlite3_bind_int(cursor->stmt, position, item->as.boolean ? 1 : 0);
                break;
            case VALUE_STRING:
                rc = sqlite3_bind_text(cursor->stmt, position, item->as.string, -1,
                                       SQLITE_TRANSIENT);
                break;
            case VALUE_NULL:
                rc = sqlite3_bind_null(cursor->stmt, position);
                break;
            default:
                return sql_error(api, "Sqlite::bind cannot bind this value type");
        }

        if (rc != SQLITE_OK) {
            return sql_error_db(api, sqlite3_db_handle(cursor->stmt),
                                "Sqlite::bind failed");
        }
    }

    return true;
}

static bool __m__sqlite3__step(RDNApi *api) {
    RDNState *stack = sql_stack(api);
    Value *cursor_value = NULL;
    SqliteCursor *cursor = NULL;
    int columns = 0;
    int index = 0;
    int rc = SQLITE_OK;

    if (stack->count < 1) {
        return sql_error(api, "Sqlite::step requires a cursor");
    }

    cursor_value = rdn_pop_value(stack);

    if (!sql_resolve_cursor(api, cursor_value, &cursor)) {
        return false;
    }

    /* A spent cursor keeps answering null so loops end cleanly. */
    if (cursor->exhausted) {
        return api->push_null(api);
    }

    rc = sqlite3_step(cursor->stmt);

    if (rc == SQLITE_DONE) {
        cursor->exhausted = true;
        return api->push_null(api);
    }

    if (rc != SQLITE_ROW) {
        return sql_error_db(api, sqlite3_db_handle(cursor->stmt),
                            "Sqlite::step failed");
    }

    columns = sqlite3_column_count(cursor->stmt);

    if (!api->push_list(api)) {
        return false;
    }

    for (index = 0; index < columns; index++) {
        bool pushed = true;

        switch (sqlite3_column_type(cursor->stmt, index)) {
            case SQLITE_INTEGER:
                pushed = api->push_integer(api, sqlite3_column_int64(cursor->stmt, index));
                break;
            case SQLITE_FLOAT:
                pushed = api->push_number(api, sqlite3_column_double(cursor->stmt, index));
                break;
            case SQLITE_TEXT: {
                const unsigned char *text = sqlite3_column_text(cursor->stmt, index);
                pushed = api->push_string(api, text != NULL ? (const char *)text : "");
                break;
            }
            case SQLITE_NULL:
                pushed = api->push_null(api);
                break;
            default:
                api->pop(api, 1);
                return sql_error(api, "Sqlite::step cannot convert blob columns");
        }

        if (!pushed) {
            return false;
        }
        if (!api->list_append(api, -2, -1)) {
            return false;
        }
        if (!api->pop(api, 1)) {
            return false;
        }
    }

    return true;
}

static bool __m__sqlite3__close_cursor(RDNApi *api) {
    RDNState *stack = sql_stack(api);
    Value *cursor_value = NULL;
    SqliteCursor *cursor = NULL;

    if (stack->count < 1) {
        return sql_error(api, "Sqlite::close-cursor requires a cursor");
    }

    cursor_value = rdn_pop_value(stack);

    if (!sql_resolve_cursor(api, cursor_value, &cursor)) {
        return false;
    }

    sqlite3_finalize(cursor->stmt);
    cursor->stmt = NULL;
    sql_cursor_remove(cursor);
    return true;
}

static bool __m__sqlite3__close(RDNApi *api) {
    RDNState *stack = sql_stack(api);
    Value *conn_value = NULL;
    SqliteConn *conn = NULL;

    if (stack->count < 1) {
        return sql_error(api, "Sqlite::close requires a connection");
    }

    conn_value = rdn_pop_value(stack);

    if (!sql_resolve_conn(api, conn_value, &conn)) {
        return false;
    }

    /* Cursors opened on this connection stay valid until closed themselves;
     * sqlite3_close_v2 defers the real teardown until then. */
    sqlite3_close_v2(conn->db);
    conn->db = NULL;
    sql_conn_remove(conn);
    return true;
}

bool rdn_module_init(RDNModule *module) {

    struct { const char *name; RDNNativeFunction func; } entries[] = {
        { "__m__sqlite3__open", __m__sqlite3__open },
        { "__m__sqlite3__exec", __m__sqlite3__exec },
        { "__m__sqlite3__cursor", __m__sqlite3__cursor },
        { "__m__sqlite3__bind", __m__sqlite3__bind },
        { "__m__sqlite3__step", __m__sqlite3__step },
        { "__m__sqlite3__close_cursor", __m__sqlite3__close_cursor },
        { "__m__sqlite3__close", __m__sqlite3__close },
    };

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        if (!module->register_function(module, entries[i].name, entries[i].func)) {
            return false;
        }
    }
    return true;
}
