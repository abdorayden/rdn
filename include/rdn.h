#ifndef SRC_H_
#define SRC_H_

#include "./rdn_native.h"
#include "../src/stack.h"

#define RADEN_VERSION "1.0.0"
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
typedef RLList(char *) LoadPathStack;
typedef RLList(char *) SearchPathStack;
typedef RLList(char *) MacroExpansionStack;

typedef RLStack(Value *) RDNState;
typedef RLList(Vars_t*) Vars;
typedef RLList(Funcs_t*) Funcs;
typedef RLList(NativeModuleReg*) NativeModuleRegs;

static void free_value(Value *value);

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
        RLList(Value*) list;
    } as;
};

struct Vars_t {
    char* var_name;
    Value* var_value;
    bool is_scope_marker; // define the variable in scope
    bool is_const; // check if the variable is constant
};

enum FuncType {
    FUNC_SCRIPT,
    FUNC_APPLY,
    FUNC_DEMAC,
    FUNC_NATIVE,
};

struct Funcs_t {
    char* func_name;
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

struct NativeModuleReg {
    char *name;
    RDNNativeFunction function;
};

struct NativeCallState {
    RDNState *stack;
    Vars *vars;
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

struct RDNSharedState {
    RDNState rdn_state;
    Vars    rdn_vars;
    Funcs   rdn_funcs;
};

struct DiagnosticContext {
    const char *path;
    const char *source;
    size_t base_line;
    size_t base_column;
    const char *last_token_start;
    const char *last_token_end;
};

static const char *g_current_source_path = NULL;
static DiagnosticContext g_diagnostic_context = {0};
static LoadPathStack g_load_path_stack = {0};
static SearchPathStack g_script_search_paths = {0};
static SearchPathStack g_native_search_paths = {0};
static MacroExpansionStack g_macro_expansions = {0};
static RLList(char*) g_stack_trace_protected = {0};
static bool g_diagnostics_suppressed = false;

static bool is_token(const char *value, const char *expected);
static bool is_operator_token(const char *value);
static char *copy_string(const char *text) ;
static Value *create_null_value(void);
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
static Funcs_t *create_func_entry(const char *name, char *body, const char *source_path, size_t source_line, size_t source_column);
static Funcs_t *create_native_func_entry(const char *name, RDNNativeFunction native_function, void *native_library_handle);
static void free_var_entry(Vars_t *entry);
static void free_vars(Vars *vars);
static void free_func_entry(Funcs_t *entry);
static void free_funcs(Funcs *funcs);
static bool vars_push_scope(Vars *vars);
static void vars_pop_scope(Vars *vars);
static Vars_t *find_var_entry(const Vars *vars, const char *name);
static Vars_t *find_current_scope_var_entry(const Vars *vars, const char *name);
static Funcs_t *find_func_entry(const Funcs *funcs, const char *name);
static bool func_entry_has_body(const Funcs_t *entry);
static bool funcs_define(Funcs *funcs, const char *name, char *body, const char *source_path, size_t source_line, size_t source_column);
static bool funcs_define_apply(Funcs *funcs, const char *name, char *body, const char *source_path, size_t source_line, size_t source_column);
static bool funcs_define_demac(Funcs *funcs, const char *name, char *body, const char *source_path, size_t source_line, size_t source_column);
static bool funcs_define_native(Funcs *funcs, const char *name, RDNNativeFunction native_function, void *native_library_handle);
static bool vars_let(Vars *vars, const char *name, const Value *value);
static bool vars_set(Vars *vars, const char *name, const Value *value);
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
static bool apply_type(RDNState *stack, Vars *vars, Funcs *funcs);
static bool apply_swap(RDNState *stack);
static bool apply_pop(RDNState *stack);
static bool apply_dup(RDNState *stack, Vars *vars);
static bool apply_append(RDNState *stack, Vars *vars);
static bool apply_remove(RDNState *stack, Vars *vars);
static bool apply_index(RDNState *stack, Vars *vars);
static bool apply_len(RDNState *stack, Vars *vars);
static bool apply_add_load_path(RDNState *stack, Vars *vars);
static bool apply_add_native_path(RDNState *stack, Vars *vars);
static bool apply_load(RDNState *stack, Vars *vars, Funcs *funcs);
static bool apply_loadnative(RDNState *stack, Vars *vars, Funcs *funcs);
static bool typecheck_signature_types_valid(const Value *types);
static bool append_typecheck_signature(Vars *vars, const char *name, const Value *params, const Value *returns);
static bool apply_defun(RDNState *stack, Vars *vars, Funcs *funcs, char **cursor);
static bool apply_apply(RDNState *stack, Funcs *funcs, char **cursor);
static bool apply_demac(RDNState *stack, Funcs *funcs, char **cursor);
static bool apply_call(RDNState *stack, Vars *vars, Funcs *funcs);
static bool apply_pcall(RDNState *stack, Vars *vars, Funcs *funcs);
static bool execute_named_entry(RDNState *stack, Vars *vars, Funcs *funcs, Funcs_t *entry, const char *context_kind, const char *context_name);
static bool expand_demac(Funcs_t *entry, char **cursor);
static void free_macro_expansion_stack(MacroExpansionStack *expansions);
static void free_macro_expansions(void);
static bool materialize_scope_references(RDNState *stack, Vars *vars);

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
static bool execute_list_literal(RDNState *stack, Vars *vars, Funcs *funcs, char **cursor);
static Value *parse_list_literal(char **cursor, Vars *vars, Funcs *funcs);
static bool identifier_is_name_target(char *cursor);
static bool apply_line_col(RDNState *stack);
static bool apply_file_name(RDNState *stack);
static bool apply_func_name(RDNState *stack, Funcs *funcs);
static bool apply_stack_size(RDNState *stack);
static bool apply_do_string(RDNState *stack, Vars *vars, Funcs *funcs);
static bool apply_do_file(RDNState *stack, Vars *vars, Funcs *funcs);
static bool apply_let(RDNState *stack, Vars *vars);
static bool apply_assert(RDNState *stack, Vars *vars);
static bool apply_unlet(RDNState *stack, Vars *vars);
static bool apply_error(RDNState *stack, Vars *vars);
static bool apply_set(RDNState *stack, Vars *vars);
static bool apply_enum(RDNState *stack, Vars *vars , bool reset);
static bool apply_const(RDNState *stack, Vars *vars);
static bool skip_block(char **cursor, BlockStop *stop_reason, bool allow_else);
static bool execute_block(RDNState *stack, Vars* vars, Funcs *funcs, char **cursor, BlockStop *stop_reason, bool allow_else);
static bool apply_if(RDNState *stack, Vars* vars, Funcs *funcs, char **cursor, BlockStop *stop_reason);
static bool apply_loop(RDNState *stack, Vars* vars, Funcs *funcs, char **cursor, BlockStop *stop_reason);
static bool execute_block(RDNState *stack, Vars* vars, Funcs *funcs, char **cursor, BlockStop *stop_reason, bool allow_else);
static bool skip_if(char **cursor);
static bool skip_loop(char **cursor);
static bool skip_demac(char **cursor);
static bool skip_block(char **cursor, BlockStop *stop_reason, bool allow_else);
static bool evaluate_source(RDNState *stack, Vars* vars, Funcs *funcs, char *source);
static bool evaluate_file(RDNState *stack, Vars *vars, Funcs *funcs, const char *path);
#define rdn_do_string(src) do{evaluate_source(NULL , NULL , NULL , (src));}while(0)
static char *read_file(const char *path);
static bool path_is_readable_file(const char *path);
static char *resolve_path_from_current_source(const char *path);
static char *canonicalize_existing_path(const char *path);
static char *join_paths(const char *base, const char *path);
static bool path_has_separator(const char *path);
static bool path_is_absolute(const char *path);
static char *resolve_load_path_candidate(const char *path, const SearchPathStack *search_paths);
static bool load_path_stack_contains(const char *path);
static bool push_load_path(const char *path);
static void pop_load_path(void);
static void free_search_path_stack(SearchPathStack *paths);
static bool search_path_stack_contains(const SearchPathStack *paths, const char *path);
static bool push_search_path(SearchPathStack *paths, const char *path);
static bool reset_search_paths(void);
static bool pop_string_path_operand(RDNState *stack, Vars *vars, const char *context, Value **out_target, char **out_path);
static bool set_owned_error_message(char **slot, const char *message);
static bool append_string_repr(char **target_string, const Value *value);
static Value *create_string_char_value(char ch);
static Value *native_get_stack_value(RDNState *stack, long index);
static RDNValueType native_value_type_from_value(const Value *value);
static size_t native_api_stack_size(RDNApi *api);
static RDNValueType native_api_type(RDNApi *api, long index);
static bool native_api_is_number(RDNApi *api, long index);
static bool native_api_to_integer(RDNApi *api, long index, long *out_value);
static bool native_api_to_number(RDNApi *api, long index, double *out_value);
static bool native_api_to_boolean(RDNApi *api, long index, bool *out_value);
static const char *native_api_to_string(RDNApi *api, long index);
static const char *native_api_to_identifier(RDNApi *api, long index);
static bool native_api_pop(RDNApi *api, size_t count);
static bool native_api_push_null(RDNApi *api);
static bool native_api_push_integer(RDNApi *api, long value);
static bool native_api_push_number(RDNApi *api, double value);
static bool native_api_push_boolean(RDNApi *api, bool value);
static bool native_api_push_string(RDNApi *api, const char *value);
static bool native_api_push_list(RDNApi *api);
static bool native_api_list_len(RDNApi *api, long index, size_t *out_length);
static bool native_api_list_append(RDNApi *api, long list_index, long value_index);
static bool native_api_list_index(RDNApi *api, long list_index, long item_index);
static bool native_api_list_remove(RDNApi *api, long list_index, long item_index);
static bool native_api_raise_error(RDNApi *api, const char *message);
static NativeModuleReg *create_native_module_reg(const char *name, RDNNativeFunction function);
static void free_native_module_reg(NativeModuleReg *reg);
static void free_native_module_regs(NativeModuleRegs *regs);
static bool native_module_register_function(RDNModule *module, const char *name, RDNNativeFunction function);
static bool native_module_set_error(RDNModule *module, const char *message);
static bool append_text(char **buffer, size_t *length, const char *text);
static void diagnostic_set_source(const char *path, const char *source, size_t base_line, size_t base_column);
static void diagnostic_set_last_token(const char *start, const char *end);
static bool diagnostic_error_at(const char *pointer, const char *fmt, ...);
static bool diagnostic_error_current(const char *fmt, ...);
static bool diagnostic_note_current(const char *fmt, ...);
static bool source_has_complete_blocks(const char *source, const Funcs *funcs, bool *out_complete);
static int run_repl(void);
static const char *host_os_name(void);
static const char *host_shared_library_extension(void);
static void apply_host_environment(Vars *vars);
static void apply_argv(Vars* vars , const char* path, int argc , char** argv);
int rdn_main(int argc , char** argv);

#endif // !SRC_H_
