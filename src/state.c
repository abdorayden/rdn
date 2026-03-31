#include "state.h"

void rdn_init(RDNState* state){
    state->rdn_stack = Stack(Buf_Disable);
}

void rdn_push_int32(RDNState* state,int value){
    state->rdn_stack.Push(RL_INT , RLTempalloc(value));
}

void rdn_add(RDNState* state){
    RLResult x = state->rdn_stack.Pop();
    if (x.IsError()) {
        fprintf(stderr, "err 1");
    }
    RLResult y = state->rdn_stack.Pop();
    if (y.IsError()) {
        fprintf(stderr, "err 2");
    }

    state->rdn_stack.Push(RL_INT , RLTempalloc(*(int*)x.GetData() + *(int*)y.GetData()));
}

void rdn_print(RDNState* state){
    printf("%d" , *(int*)state->rdn_stack.Pop().GetData());
}

