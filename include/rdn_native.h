#ifndef RDN_NATIVE_H
#define RDN_NATIVE_H

/*
 * Public native module API for Raden.
 *
 * A native module is a shared library loaded by `loadnative`.
 * The module exports:
 *
 *     bool rdn_module_init(RDNModule *module);
 *
 * Inside `rdn_module_init`, register native functions by name with
 * `module->register_function(...)`.
 *
 * Native functions operate on the interpreter stack through `RDNApi`.
 * The API is intentionally stack-oriented so native code matches the
 * semantics of builtin Raden operations.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RDNApi RDNApi;
typedef struct RDNModule RDNModule;

/*
 * Public value tags visible to native code.
 *
 * `RDN_VALUE_IDENTIFIER` is an unresolved name token inside the interpreter.
 * Most native code will mainly work with integers, doubles, strings,
 * booleans, and lists.
 */
typedef enum RDNValueType {
    RDN_VALUE_NONE = 0,
    RDN_VALUE_INTEGER,
    RDN_VALUE_DOUBLE,
    RDN_VALUE_STRING,
    RDN_VALUE_BOOLEAN,
    RDN_VALUE_LIST,
    RDN_VALUE_IDENTIFIER
} RDNValueType;

typedef bool (*RDNNativeFunction)(RDNApi *api);
typedef bool (*RDNModuleInit)(RDNModule *module);

struct RDNApi {
    /*
     * Interpreter-owned internal pointer.
     *
     * Treat this as opaque. Native modules should not read or write it
     * directly. It is only stored here so the runtime can route API calls
     * back into the current interpreter state.
     */
    void *userdata;

    /*
     * Return the number of values currently on the stack.
     */
    size_t (*stack_size)(RDNApi *api);

    /*
     * Return the type of the value at `index`.
     *
     * Index rules:
     * - `-1` is the top of the stack
     * - `-2` is one below the top
     * - positive indices start at `1`
     *
     * If the index is invalid, the runtime returns `RDN_VALUE_NONE`.
     */
    RDNValueType (*type)(RDNApi *api, long index);

    /*
     * Return true if the value at `index` is an integer or double.
     */
    bool (*is_number)(RDNApi *api, long index);

    /*
     * Convert the value at `index` to an integer.
     *
     * Succeeds only for integer values.
     */
    bool (*to_integer)(RDNApi *api, long index, long *out_value);

    /*
     * Convert the value at `index` to a double.
     *
     * Succeeds for both integer and double values.
     */
    bool (*to_number)(RDNApi *api, long index, double *out_value);

    /*
     * Convert the value at `index` to a boolean.
     *
     * Succeeds only for boolean values.
     */
    bool (*to_boolean)(RDNApi *api, long index, bool *out_value);

    /*
     * Return a pointer to the string stored at `index`.
     *
     * Succeeds only for string values.
     * The returned pointer is owned by the interpreter and must not be freed.
     */
    const char *(*to_string)(RDNApi *api, long index);

    /*
     * Return a pointer to the identifier text at `index`.
     *
     * Succeeds only for identifier values.
     * The returned pointer is owned by the interpreter and must not be freed.
     */
    const char *(*to_identifier)(RDNApi *api, long index);

    /*
     * Pop `count` values from the top of the stack.
     */
    bool (*pop)(RDNApi *api, size_t count);

    /*
     * Push scalar values onto the stack.
     */
    bool (*push_integer)(RDNApi *api, long value);
    bool (*push_number)(RDNApi *api, double value);
    bool (*push_boolean)(RDNApi *api, bool value);
    bool (*push_string)(RDNApi *api, const char *value);

    /*
     * Push a new empty list onto the stack.
     */
    bool (*push_list)(RDNApi *api);

    /*
     * Read the length of a list value.
     *
     * Succeeds only when `index` points to a list.
     */
    bool (*list_len)(RDNApi *api, long index, size_t *out_length);

    /*
     * Append the value at `value_index` into the list at `list_index`.
     *
     * Important: the list receives a clone of the value.
     * The original value remains on the stack unchanged.
     *
     * Typical pattern:
     *
     *     api->push_list(api);           // stack: [list]
     *     api->push_string(api, "x");    // stack: [list, "x"]
     *     api->list_append(api, -2, -1); // list now contains "x"
     *     api->pop(api, 1);              // remove temporary "x"
     *
     * If you want the function to return only the list, remember to pop the
     * temporary appended values after each append.
     */
    bool (*list_append)(RDNApi *api, long list_index, long value_index);

    /*
     * Clone the item at `item_index` from the list at `list_index` and push
     * that cloned value onto the stack.
     *
     * `item_index` is zero-based.
     */
    bool (*list_index)(RDNApi *api, long list_index, long item_index);

    /*
     * Remove the item at `item_index` from the list at `list_index`.
     *
     * `item_index` is zero-based.
     */
    bool (*list_remove)(RDNApi *api, long list_index, long item_index);

    /*
     * Report an error and make the native function fail.
     *
     * Native functions normally return the result of `raise_error(...)`
     * directly:
     *
     *     return api->raise_error(api, "bad argument");
     */
    bool (*raise_error)(RDNApi *api, const char *message);
};

struct RDNModule {
    /*
     * Interpreter-owned internal pointer.
     *
     * Treat this as opaque.
     */
    void *userdata;

    /*
     * Register a native function under a Raden-visible name.
     *
     * Example:
     *
     *     module->register_function(module, "readLines", readLines);
     */
    bool (*register_function)(RDNModule *module, const char *name, RDNNativeFunction function);

    /*
     * Report module initialization failure from `rdn_module_init`.
     */
    bool (*set_error)(RDNModule *module, const char *message);
};

#ifdef __cplusplus
}
#endif

#endif
