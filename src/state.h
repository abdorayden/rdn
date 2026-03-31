#ifndef STATE_H_
#define STATE_H_

#include "../third_party/raylist.h"

typedef struct {
    RLCollections rdn_stack;
}RDNState;

void rdn_init(RDNState* state);
void rdn_push_int32(RDNState* state,int value);
void rdn_add(RDNState* state);
void rdn_print(RDNState* state);

#endif // !STATE_H_
