#include "../include/rdn_native.h"

#include <math.h>

static bool powerOf(RDNApi* api) {

    double x = 0;
    double y = 0;


    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "powerOf requires 2 operands");
    }

    if (api->type(api, -1) == RDN_VALUE_DOUBLE && api->type(api, -2) == RDN_VALUE_DOUBLE) {

        if (!api->to_number(api, -1, &y) || !api->to_number(api, -2, &x)) {
            return api->raise_error(api, "powerOf requires number operands");
        }
        if (!api->pop(api, 2)) {
            return false;
        }
        return api->push_number(api, pow(x, y));
    }
    return true;

}


bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "powerOf", powerOf)) {
        return false;
    }
    return true;
}
