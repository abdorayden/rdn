#include <stdio.h>
#include "../include/rdn.h"

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

    const char* foo = api->to_string(api , -1);

    printf("called from f : %s" , foo);

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
    funcs_define("hey", "331 print", NULL, 1, 1);
    funcs_define_native("foo", f, NULL);
    evaluate_source(&stack, "1");
    hello(&stack);
    // evaluate_source(&stack, "hey call");
    // evaluate_source(&stack, "foo call print");
    evaluate_source(&stack, "\"hey im rayden\" foo call print");

    return 0;
}

#include "../src/rdn.c"
