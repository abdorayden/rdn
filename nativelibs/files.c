#include "../include/rdn_native.h"
#include <stdio.h>

static bool OpenFile(RDNApi* api) {
    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "OpenFile requires 2 params");
    }


    if (api->type(api, -1) == RDN_VALUE_STRING && api->type(api, -2) == RDN_VALUE_STRING) {
        const char* filepath = api->to_string(api, -2);
        const char* mode = api->to_string(api, -1);
        if (!mode || !filepath) {
            return api->raise_error(api, "OpenFile requires string inputs");
        }
        if (!api->pop(api, 2)) {
            return false;
        }


        return api->push_string(api , (char*)fopen(filepath, mode));
    }

    return true;
}

// TODO: introduce readLines

bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "OpenFile", OpenFile)) {
        return false;
    }
    return true;
}
