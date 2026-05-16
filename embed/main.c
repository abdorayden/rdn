#include <stdio.h>
#include "../include/src.h"

void hello (RDNState* state) {
    Value* top = ray_pop(state);
    if (top->type == VALUE_INTEGER) {
        printf("%ld\n" , top->as.integer);
    }
}

int main(void)
{
    // no requires any initialization
    // rdn_do_string("32 print");

    RDNState stack = {0};
    Vars vars = {0};
    Funcs funcs = {0};
    ray_append(&funcs, create_func_entry("hey", "331 print"));
    evaluate_source(&stack, &vars, &funcs, "hey call");
    evaluate_source(&stack, &vars, &funcs, "1");
    hello(&stack);

    return 0;
}

#include "../src/src.c"
