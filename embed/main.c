#include <stdio.h>
#include "../include/src.h"

// GCC is ALL u neeeeed
// built: gcc -o main main.c && ./main

void hello (RDNState* state) {
    Value* top = ray_pop(state);
    if (top->type == VALUE_INTEGER) {
        printf("%ld\n" , top->as.integer);
    }
}

bool f(RDNApi *api){
    /*
     * Native list pattern:
     * 1. push the result list
     * 2. push a temporary value
     * 3. append that value into the list
     * 4. pop the temporary value, because list_append clones it
     *
     * After this function returns, only the list should remain on the stack.
     */
    if (!api->push_list(api)) {
        return false;
    }

    if (!api->push_string(api, "10")) {
        return false;
    }
    if (!api->list_append(api, -2, -1)) {
        return false;
    }
    if (!api->pop(api, 1)) {
        return false;
    }

    if (!api->push_string(api, "20")) {
        return false;
    }
    if (!api->list_append(api, -2, -1)) {
        return false;
    }
    if (!api->pop(api, 1)) {
        return false;
    }

    return true;
}

int main(void)
{
    // no requires any initialization
    // rdn_do_string("32 print");

    RDNState stack = {0};
    Vars vars = {0};
    Funcs funcs = {0};
    ray_append(&funcs, create_func_entry("hey", "331 print"));
    ray_append(&funcs, create_native_func_entry("foo", f, NULL));
    evaluate_source(&stack, &vars, &funcs, "1");
    hello(&stack);
    evaluate_source(&stack, &vars, &funcs, "hey call");
    evaluate_source(&stack, &vars, &funcs, "foo call print");

    return 0;
}

#include "../src/src.c"
