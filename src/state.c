#include "state.h"

void rdn_init(RDNState* state){
    state->rdn_stack = Stack(Buf_Disable);
}

void rdn_push_int(RDNState* state,int value){
    state->rdn_stack.Push(RL_INT , RLTempalloc(value));
}

void rdn_add(RDNState* state){
    RLResult x = state->rdn_stack.Pop();
    if (x.IsError()) {
        fprintf(stderr, "err 1");
    }
    int xx = *(int*)x.GetData();

    RLResult y = state->rdn_stack.Pop();
    if (y.IsError()) {
        fprintf(stderr, "err 2");
    }
    int yy = *(int*)y.GetData();


    int result = xx + yy;
    state->rdn_stack.Push(RL_INT , RLTempalloc(result));
}

void rdn_sub(RDNState* state){
    RLResult x = state->rdn_stack.Pop();
    if (x.IsError()) {
        fprintf(stderr, "err 1");
    }
    int xx = *(int*)x.GetData();

    RLResult y = state->rdn_stack.Pop();
    if (y.IsError()) {
        fprintf(stderr, "err 2");
    }
    int yy = *(int*)y.GetData();


    int result = xx - yy;
    state->rdn_stack.Push(RL_INT , RLTempalloc(result));
}
void rdn_mul(RDNState* state){
    RLResult x = state->rdn_stack.Pop();
    if (x.IsError()) {
        fprintf(stderr, "err 1");
    }
    int xx = *(int*)x.GetData();

    RLResult y = state->rdn_stack.Pop();
    if (y.IsError()) {
        fprintf(stderr, "err 2");
    }
    int yy = *(int*)y.GetData();


    int result = xx * yy;
    state->rdn_stack.Push(RL_INT , RLTempalloc(result));
}
void rdn_div(RDNState* state){
    RLResult x = state->rdn_stack.Pop();
    if (x.IsError()) {
        fprintf(stderr, "err 1");
    }
    int xx = *(int*)x.GetData();

    RLResult y = state->rdn_stack.Pop();
    if (y.IsError()) {
        fprintf(stderr, "err 2");
    }
    int yy = *(int*)y.GetData();


    int result = xx / yy;
    state->rdn_stack.Push(RL_INT , RLTempalloc(result));
}

void rdn_print(RDNState* state){
    printf("%d" , *(int*)state->rdn_stack.Pop().GetData());
}

void rdn_swap(RDNState* state){
    RLResult x = state->rdn_stack.Pop();
    if (x.IsError()) {
        fprintf(stderr, "err 1");
    }
    int xx = *(int*)x.GetData();

    RLResult y = state->rdn_stack.Pop();
    if (y.IsError()) {
        fprintf(stderr, "err 2");
    }
    int yy = *(int*)y.GetData();

    rdn_push_int(state, xx);
    rdn_push_int(state, yy);
}

int* rdn_pop(RDNState* state){
    RLResult x = state->rdn_stack.Pop();
    if (x.IsError()) {
        fprintf(stderr, "err 1");
    }
    return (int*)x.GetData();
}

void rdn_clean(RDNState* state){
    state->rdn_stack.Clear();
}
