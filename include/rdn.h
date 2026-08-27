#ifndef RDN_H_
#define RDN_H_

#include "./rdn_native.h"

#define RADEN_VERSION "1.2.0"
#define RDN_TYPECHECK_FUNC_CODE 6
#define RDN_TYPECHECK_ANY_CODE 7

#ifndef RDN_INSTALL_PREFIX
#define RDN_INSTALL_PREFIX "/usr/local/share/rdn"
#endif

typedef enum ValueType ValueType;
typedef enum BlockStop BlockStop;
typedef enum FuncType FuncType;
typedef struct Value Value ;
typedef struct Vars_t Vars_t;
typedef struct Funcs_t Funcs_t;
typedef struct RDNSharedState RDNSharedState;
typedef struct NativeCallState NativeCallState;
typedef struct NativeModuleReg NativeModuleReg;
typedef struct NativeModuleLoadState NativeModuleLoadState;
typedef struct DiagnosticContext DiagnosticContext;
typedef struct RDNStringList {
    char **items;
    size_t count;
    size_t capacity;
} RDNStringList;
typedef RDNStringList LoadPathStack;
typedef RDNStringList SearchPathStack;
typedef RDNStringList MacroExpansionStack;

typedef struct RDNValueList {
    Value **items;
    size_t count;
    size_t capacity;
} RDNValueList;

enum ValueType{
    VALUE_NULL,
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
        RDNValueList list;
    } as;
};

struct Vars_t {
    // char* var_name; now it's key of the hash table
    Value* var_value;
    bool is_scope_marker; // define the variable in scope
    bool is_const; // check if the variable is constant

    char* module_name; // this for the module that if the variable defined inside it

    // Hash-table scoping support:
    // `prev` is the previous binding of the same name (shadowed by this one).
    // Restoring it when the owning scope is popped unshadows the outer binding.
    Vars_t *prev;
    // `scope_id` is the id of the scope that owns this binding. Bindings with
    // scope_id == 0 live at the top level and are never popped.
    size_t scope_id;
};

enum FuncType {
    FUNC_SCRIPT,
    FUNC_APPLY,
    FUNC_DEMAC,
    FUNC_NATIVE,
};

struct Funcs_t {
    char* module_name; // this for the module that if the function defined inside it
 
    // char* func_name; now it's key of hash table
    FuncType type;
    union {
        char *func_body;
        RDNNativeFunction native_function;
    } as;
    char *source_path;
    size_t source_line;
    size_t source_column;
    void *native_library_handle;
};

typedef struct RDNState {
    Value **items;
    size_t count;
    size_t capacity;
} RDNState;
typedef struct RDNNativeModuleList {
    NativeModuleReg **items;
    size_t count;
    size_t capacity;
} NativeModuleRegs;

struct NativeModuleReg {
    char *name;
    RDNNativeFunction function;
};

struct NativeCallState {
    RDNState *stack;
    void *vars;
    char *error_message;
};

struct NativeModuleLoadState {
    NativeModuleRegs regs;
    char *error_message;
};

enum BlockStop{
    BLOCK_STOP_EOF,
    BLOCK_STOP_ELSE,
    BLOCK_STOP_END,
    BLOCK_STOP_BREAK,
    BLOCK_STOP_CONTINUE,
    BLOCK_STOP_RETURN,
};

struct DiagnosticContext {
    const char *path;
    const char *source;
    size_t base_line;
    size_t base_column;
    const char *last_token_start;
    const char *last_token_end;
};

// Curated public runtime API
// These functions are exported by the interpreter (linked with -rdynamic) so
// that natively-loaded modules can resolve them via dlsym(). The interpreter
// keeps module-level `vars` / `funcs` state internally.

char *copy_string(const char *text);
Value *create_null_value(void);
Value *create_integer_value(long integer);
Value *create_double_value(double number);
Value *create_boolean_value(bool boolean);
Value *create_string_value_owned(char *string);
Value *create_string_value_copy(const char *string);
Value *create_list_value(void);
Value *create_var_name_value(const char *name);
Value *clone_value(const Value *value);
void free_value(Value *value);

Vars_t *create_var_entry(const char *name, Value *value, bool is_const);
Vars_t *find_var_entry(const char *name);
bool vars_let(const char *name, const Value *value);
bool vars_set(const char *name, const Value *value);
bool vars_const(const char *name, const Value *value);
bool vars_push_scope(void);
void vars_pop_scope(void);
Value *resolve_value_if_var(Value *value, const char *context);

Funcs_t *create_func_entry(const char *name, char *body, const char *source_path, size_t source_line, size_t source_column);
Funcs_t *create_native_func_entry(const char *name, RDNNativeFunction native_function, void *native_library_handle);
Funcs_t *find_func_entry(const char *name);
bool funcs_define(const char *name, char *body, const char *source_path, size_t source_line, size_t source_column);
bool funcs_define_native(const char *name, RDNNativeFunction native_function, void *native_library_handle);
void free_vars(void);
void free_funcs(void);

bool push_value(RDNState *stack, Value *value);
Value *pop_value(RDNState *stack);
bool evaluate_source(RDNState *stack, char *source);
bool evaluate_file(RDNState *stack, const char *path);
char *read_file(const char *path);
int rdn_main(int argc, char **argv);

// Native-module helpers (also exported):
Value *native_get_stack_value(RDNState *stack, long index);
RDNValueType native_value_type_from_value(const Value *value);
bool value_to_double(const Value *value, double *out_value);
bool value_to_long(const Value *value, long *out_value);
bool value_to_boolean(const Value *value, bool *out_value);

#define rdn_do_string(src) do{evaluate_source(NULL, (src));}while(0)

#endif // !RDN_H_
