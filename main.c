#include "./src/state.h"
#include <stdio.h>

// my stack based language
// typedef struct {
//     
// }Program;

int main(void)
{
    RDNState state = {0};
    rdn_init(&state);

    rdn_push_int(&state, 35);
    rdn_push_int(&state, 34);

    rdn_swap(&state);

    int* x = rdn_pop(&state);
    int* y = rdn_pop(&state);

    printf("%d\n", *x);
    printf("%d\n", *y);

    // rdn_print(&state);
    // puts("");
    // rdn_print(&state);
    // puts("");
    // rdn_print(&state);

    rdn_clean(&state);
    return 0;
}
