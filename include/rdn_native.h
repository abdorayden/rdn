#ifndef RDN_NATIVE_H
#define RDN_NATIVE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RDNApi RDNApi;
typedef struct RDNModule RDNModule;

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
    void *userdata;
    size_t (*stack_size)(RDNApi *api);
    RDNValueType (*type)(RDNApi *api, long index);
    bool (*is_number)(RDNApi *api, long index);
    bool (*to_integer)(RDNApi *api, long index, long *out_value);
    bool (*to_number)(RDNApi *api, long index, double *out_value);
    bool (*to_boolean)(RDNApi *api, long index, bool *out_value);
    const char *(*to_string)(RDNApi *api, long index);
    const char *(*to_identifier)(RDNApi *api, long index);
    bool (*pop)(RDNApi *api, size_t count);
    bool (*push_integer)(RDNApi *api, long value);
    bool (*push_number)(RDNApi *api, double value);
    bool (*push_boolean)(RDNApi *api, bool value);
    bool (*push_string)(RDNApi *api, const char *value);
    bool (*raise_error)(RDNApi *api, const char *message);
};

struct RDNModule {
    void *userdata;
    bool (*register_function)(RDNModule *module, const char *name, RDNNativeFunction function);
    bool (*set_error)(RDNModule *module, const char *message);
};

#ifdef __cplusplus
}
#endif

#endif
