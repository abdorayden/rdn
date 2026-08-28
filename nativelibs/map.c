/*
 * map.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Native hash-map module for Raden backed by src/ht.h.
 *
 * A map value is a Raden list holding a single integer table id:
 *
 *     (table_id)
 *
 * The real hash table lives in a registry owned by this module. Keys are
 * scalar Raden values (integer, double, string, boolean, null). The map is
 * typed: `Map::new` receives `(key_type value_type)` codes from
 * `Typecheck::OGTypes`, and put/get enforce them (m_Any accepts anything).
 *
 * Exported Raden functions:
 *   __m__map__new, __m__map__put, __m__map__get, __m__map__del,
 *   __m__map__clear, __m__map__size, __m__map__keys, __m__map__values,
 *   __m__map__types
 */

#include "../include/rdn.h"
#include "../include/rdn_native.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HT_IMPLEMENTATION
#include "../src/ht.h"

enum {
    m_Int,
    m_Str,
    m_Bool,
    m_Real,
    m_List,
    m_Null,
    m_Func,
    m_Any,
};

typedef struct {
    Value *value;
} MapKey;

typedef struct {
    long id;
    int key_type;
    int value_type;
    Ht(MapKey, Value *) table;
} MapTable;

static MapTable *g_tables = NULL;
static size_t g_table_count = 0;
static size_t g_table_capacity = 0;
static long g_next_table_id = 1;

static RDNState *map_stack(RDNApi *api) {
    NativeCallState *state = (NativeCallState *)api->userdata;
    return state->stack;
}

static uintptr_t map_value_hash(const Value *value) {
    uintptr_t hash = ht_fnv1a_hash(&value->type, sizeof(value->type));

    switch (value->type) {
        case VALUE_STRING:
            hash ^= ht_fnv1a_hash(value->as.string, strlen(value->as.string));
            break;
        case VALUE_INTEGER:
            hash ^= (uintptr_t)value->as.integer * 0x9e3779b97f4a7c15ULL;
            break;
        case VALUE_DOUBLE: {
            uint64_t bits = 0;
            memcpy(&bits, &value->as.number, sizeof(bits));
            hash ^= (uintptr_t)bits;
            break;
        }
        case VALUE_BOOLEAN:
            hash ^= value->as.boolean ? 1231U : 1237U;
            break;
        default:
            break;
    }

    return hash;
}

static bool map_value_eq(const Value *left, const Value *right) {
    if (left->type != right->type) {
        return false;
    }

    switch (left->type) {
        case VALUE_NULL:
            return true;
        case VALUE_INTEGER:
            return left->as.integer == right->as.integer;
        case VALUE_DOUBLE:
            return left->as.number == right->as.number;
        case VALUE_BOOLEAN:
            return left->as.boolean == right->as.boolean;
        case VALUE_STRING:
            return strcmp(left->as.string, right->as.string) == 0;
        default:
            return false;
    }
}

static uintptr_t map_key_hasheq(Ht_Op op, void const *a, void const *b, size_t n) {
    MapKey const *ka = (MapKey const *)a;
    MapKey const *kb = (MapKey const *)b;

    (void)n;

    if (op == HT_HASH) {
        return map_value_hash(ka->value);
    }

    return map_value_eq(ka->value, kb->value) ? 1 : 0;
}

static MapTable *map_table_register(int key_type, int value_type) {
    MapTable *table = NULL;

    if (g_table_count == g_table_capacity) {
        size_t new_capacity = g_table_capacity == 0 ? 8 : g_table_capacity * 2;
        MapTable *new_tables = realloc(g_tables, new_capacity * sizeof(*new_tables));

        if (new_tables == NULL) {
            return NULL;
        }

        g_tables = new_tables;
        g_table_capacity = new_capacity;
    }

    table = &g_tables[g_table_count];
    table->id = g_next_table_id++;
    table->key_type = key_type;
    table->value_type = value_type;
    /* Tables live for the whole process, so ht_free only marks the helper
     * as used; on a fresh zeroed table it is a harmless no-op. */
    memset(&table->table, 0, sizeof(table->table));
    ht_free(&table->table);
    table->table.hasheq = map_key_hasheq;
    g_table_count++;
    return table;
}

static bool map_resolve_table(RDNApi *api, Value *value, MapTable **out) {
    Value *id_value = NULL;
    long id = 0;
    size_t index = 0;

    if (out == NULL) {
        return api->raise_error(api, "internal map error");
    }

    *out = NULL;

    if (value == NULL) {
        return api->raise_error(api, "map requires a map value");
    }

    if (value->type == VALUE_AS_VAR) {
        value = api->resolve_variable(api, value->as.string);
        if (value == NULL) {
            return api->raise_error(api, "map requires an existing map variable");
        }
    }

    if (value->type != VALUE_LIST || value->as.list.count < 1) {
        return api->raise_error(api, "map requires a map value");
    }

    id_value = value->as.list.items[0];
    if (id_value == NULL || id_value->type != VALUE_INTEGER) {
        return api->raise_error(api, "map requires a map value");
    }

    id = id_value->as.integer;
    for (index = 0; index < g_table_count; index++) {
        if (g_tables[index].id == id) {
            *out = &g_tables[index];
            return true;
        }
    }

    return api->raise_error(api, "unknown map value");
}

static bool map_push_value_clone(RDNApi *api, const Value *value) {
    size_t index = 0;

    if (value == NULL) {
        return api->raise_error(api, "internal map error");
    }

    switch (value->type) {
        case VALUE_NULL:
            return api->push_null(api);
        case VALUE_INTEGER:
            return api->push_integer(api, value->as.integer);
        case VALUE_DOUBLE:
            return api->push_number(api, value->as.number);
        case VALUE_BOOLEAN:
            return api->push_boolean(api, value->as.boolean);
        case VALUE_STRING:
            return api->push_string(api, value->as.string);
        case VALUE_LIST:
            if (!api->push_list(api)) {
                return false;
            }
            for (index = 0; index < value->as.list.count; index++) {
                if (!map_push_value_clone(api, value->as.list.items[index])) {
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
        default:
            return api->raise_error(api, "cannot clone value for map");
    }
}

static Value *map_clone_value(RDNApi *api, RDNState *stack, Value *value) {
    Value *resolved = value;

    if (value == NULL) {
        return NULL;
    }

    if (value->type == VALUE_AS_VAR) {
        resolved = api->resolve_variable(api, value->as.string);
        if (resolved == NULL) {
            return NULL;
        }
    }

    if (!map_push_value_clone(api, resolved)) {
        return NULL;
    }

    return rdn_pop_value(stack);
}

static bool map_type_valid(int expected, ValueType actual) {
    if (expected == m_Any) {
        return true;
    }

    switch (actual) {
        case VALUE_INTEGER:
            return expected == m_Int;
        case VALUE_STRING:
            return expected == m_Str;
        case VALUE_BOOLEAN:
            return expected == m_Bool;
        case VALUE_DOUBLE:
            return expected == m_Real;
        case VALUE_LIST:
            return expected == m_List;
        case VALUE_NULL:
            return expected == m_Null;
        default:
            return false;
    }
}

static bool __m__map__new(RDNApi *api) {
    RDNState *stack = map_stack(api);
    Value *types_value = NULL;
    Value *resolved = NULL;
    MapTable *table = NULL;
    long key_type = 0;
    long value_type = 0;

    if (stack->count < 1) {
        return api->raise_error(api, "Map::new requires a types list");
    }

    types_value = rdn_pop_value(stack);
    resolved = types_value;
    if (resolved->type == VALUE_AS_VAR) {
        resolved = api->resolve_variable(api, resolved->as.string);
        if (resolved == NULL) {
            return api->raise_error(api, "Map::new requires an existing types list variable");
        }
    }

    if (resolved->type != VALUE_LIST || resolved->as.list.count != 2) {
        return api->raise_error(api, "Map::new requires a list of two types");
    }

    if (resolved->as.list.items[0] == NULL || resolved->as.list.items[0]->type != VALUE_INTEGER ||
        resolved->as.list.items[1] == NULL || resolved->as.list.items[1]->type != VALUE_INTEGER) {
        return api->raise_error(api, "Map::new types must be integers from OGTypes");
    }

    key_type = resolved->as.list.items[0]->as.integer;
    value_type = resolved->as.list.items[1]->as.integer;

    table = map_table_register((int)key_type, (int)value_type);
    if (table == NULL) {
        return api->raise_error(api, "failed to allocate map table");
    }

    if (!api->push_list(api)) {
        return false;
    }
    if (!api->push_integer(api, table->id)) {
        return false;
    }
    if (!api->list_append(api, -2, -1)) {
        return false;
    }
    return api->pop(api, 1);
}

static bool __m__map__put(RDNApi *api) {
    RDNState *stack = map_stack(api);
    Value *value_value = NULL;
    Value *key_value = NULL;
    Value *map_value = NULL;
    MapTable *table = NULL;
    Value *key = NULL;
    Value *value = NULL;
    MapKey map_key = {0};
    Value **slot = NULL;

    if (stack->count < 3) {
        return api->raise_error(api, "Map::put requires map, key and value");
    }

    value_value = rdn_pop_value(stack);
    key_value = rdn_pop_value(stack);
    map_value = rdn_pop_value(stack);

    if (!map_resolve_table(api, map_value, &table)) {
        return false;
    }

    key = map_clone_value(api, stack, key_value);
    if (key == NULL) {
        return api->raise_error(api, "Map::put failed to clone key");
    }

    value = map_clone_value(api, stack, value_value);
    if (value == NULL) {
        return api->raise_error(api, "Map::put failed to clone value");
    }

    if (!map_type_valid(table->key_type, key->type)) {
        return api->raise_error(api, "Map::put key type does not match map key type");
    }
    if (!map_type_valid(table->value_type, value->type)) {
        return api->raise_error(api, "Map::put value type does not match map value type");
    }

    map_key.value = key;
    slot = ht_find_or_put(&table->table, map_key);
    if (slot == NULL) {
        return api->raise_error(api, "Map::put failed to store entry");
    }
    *slot = value;
    return true;
}

static bool __m__map__get(RDNApi *api) {
    RDNState *stack = map_stack(api);
    Value *key_value = NULL;
    Value *map_value = NULL;
    MapTable *table = NULL;
    Value *key = NULL;
    MapKey map_key = {0};
    Value **slot = NULL;

    if (stack->count < 2) {
        return api->raise_error(api, "Map::get requires map and key");
    }

    key_value = rdn_pop_value(stack);
    map_value = rdn_pop_value(stack);

    if (!map_resolve_table(api, map_value, &table)) {
        return false;
    }

    key = map_clone_value(api, stack, key_value);
    if (key == NULL) {
        return api->raise_error(api, "Map::get failed to clone key");
    }

    if (!map_type_valid(table->key_type, key->type)) {
        return api->raise_error(api, "Map::get key type does not match map key type");
    }

    map_key.value = key;
    slot = ht_find(&table->table, map_key);
    if (slot == NULL) {
        return api->push_null(api);
    }

    return map_push_value_clone(api, *slot);
}

static bool __m__map__del(RDNApi *api) {
    RDNState *stack = map_stack(api);
    Value *key_value = NULL;
    Value *map_value = NULL;
    MapTable *table = NULL;
    Value *key = NULL;
    MapKey map_key = {0};

    if (stack->count < 2) {
        return api->raise_error(api, "Map::del requires map and key");
    }

    key_value = rdn_pop_value(stack);
    map_value = rdn_pop_value(stack);

    if (!map_resolve_table(api, map_value, &table)) {
        return false;
    }

    key = map_clone_value(api, stack, key_value);
    if (key == NULL) {
        return api->raise_error(api, "Map::del failed to clone key");
    }

    if (!map_type_valid(table->key_type, key->type)) {
        return api->raise_error(api, "Map::del key type does not match map key type");
    }

    map_key.value = key;
    ht_find_and_delete(&table->table, map_key);
    return true;
}

static bool __m__map__clear(RDNApi *api) {
    RDNState *stack = map_stack(api);
    Value *map_value = NULL;
    MapTable *table = NULL;

    if (stack->count < 1) {
        return api->raise_error(api, "Map::clear requires a map value");
    }

    map_value = rdn_pop_value(stack);

    if (!map_resolve_table(api, map_value, &table)) {
        return false;
    }

    ht_reset(&table->table);
    return true;
}

static bool __m__map__size(RDNApi *api) {
    RDNState *stack = map_stack(api);
    Value *map_value = NULL;
    MapTable *table = NULL;

    if (stack->count < 1) {
        return api->raise_error(api, "Map::size requires a map value");
    }

    map_value = rdn_pop_value(stack);

    if (!map_resolve_table(api, map_value, &table)) {
        return false;
    }

    return api->push_integer(api, (long)table->table.count);
}

static bool __m__map__keys(RDNApi *api) {
    RDNState *stack = map_stack(api);
    Value *map_value = NULL;
    MapTable *table = NULL;

    if (stack->count < 1) {
        return api->raise_error(api, "Map::keys requires a map value");
    }

    map_value = rdn_pop_value(stack);

    if (!map_resolve_table(api, map_value, &table)) {
        return false;
    }

    if (!api->push_list(api)) {
        return false;
    }

    ht_foreach(value_slot, &table->table) {
        MapKey key = ht_key(&table->table, value_slot);
        if (!map_push_value_clone(api, key.value)) {
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

static bool __m__map__values(RDNApi *api) {
    RDNState *stack = map_stack(api);
    Value *map_value = NULL;
    MapTable *table = NULL;

    if (stack->count < 1) {
        return api->raise_error(api, "Map::values requires a map value");
    }

    map_value = rdn_pop_value(stack);

    if (!map_resolve_table(api, map_value, &table)) {
        return false;
    }

    if (!api->push_list(api)) {
        return false;
    }

    ht_foreach(value_slot, &table->table) {
        if (!map_push_value_clone(api, *value_slot)) {
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

static bool __m__map__types(RDNApi *api) {
    RDNState *stack = map_stack(api);
    Value *map_value = NULL;
    MapTable *table = NULL;

    if (stack->count < 1) {
        return api->raise_error(api, "Map::types requires a map value");
    }

    map_value = rdn_pop_value(stack);

    if (!map_resolve_table(api, map_value, &table)) {
        return false;
    }

    if (!api->push_list(api)) {
        return false;
    }
    if (!api->push_integer(api, table->key_type)) {
        return false;
    }
    if (!api->list_append(api, -2, -1)) {
        return false;
    }
    if (!api->pop(api, 1)) {
        return false;
    }
    if (!api->push_integer(api, table->value_type)) {
        return false;
    }
    if (!api->list_append(api, -2, -1)) {
        return false;
    }
    return api->pop(api, 1);
}

bool rdn_module_init(RDNModule *module) {

    if (!module->register_function(module, "__m__map__types", __m__map__types)) {
        return false;
    }

    if (!module->register_function(module, "__m__map__values", __m__map__values)) {
        return false;
    }

    if (!module->register_function(module, "__m__map__keys", __m__map__keys)) {
        return false;
    }

    if (!module->register_function(module, "__m__map__size", __m__map__size)) {
        return false;
    }

    if (!module->register_function(module, "__m__map__clear", __m__map__clear)) {
        return false;
    }

    if (!module->register_function(module, "__m__map__del", __m__map__del)) {
        return false;
    }

    if (!module->register_function(module, "__m__map__new", __m__map__new)) {
        return false;
    }

    if (!module->register_function(module, "__m__map__get", __m__map__get)) {
        return false;
    }

    if (!module->register_function(module, "__m__map__put", __m__map__put)) {
        return false;
    }

    return true;
}
