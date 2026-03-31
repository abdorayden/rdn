#include "./src/state.h"

// my stack based language

// typedef struct {
//     
// }Program;

int main(void)
{
    RDNState state = {0};
    rdn_init(&state);

    rdn_push_int32(&state, 5);
    rdn_push_int32(&state, 5);

    rdn_add(&state);

    rdn_print(&state);
    return 0;
}
