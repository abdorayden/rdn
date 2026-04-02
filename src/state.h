#ifndef STATE_H_
#define STATE_H_

#include "../third_party/raylist.h"

typedef struct {
    RLCollections rdn_stack;
}RDNState;

void rdn_init(RDNState* state);
void rdn_push_int(RDNState* state,int value);
void rdn_add(RDNState* state);
void rdn_sub(RDNState* state);
void rdn_mul(RDNState* state);
void rdn_div(RDNState* state);
void rdn_print(RDNState* state);

void rdn_swap(RDNState* state);
int* rdn_pop(RDNState* state);


void rdn_clean(RDNState* state);

#endif // !STATE_H_
