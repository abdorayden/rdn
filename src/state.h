#ifndef STATE_H_
#define STATE_H_

#include <stdbool.h>

void rdn_init(void* state);
void rdn_push_int(void* state, int value);
void rdn_push_float(void* state, double value);
void rdn_push_string(void* state, const char* value);
void rdn_push_bool(void* state, bool value);
void rdn_add(void* state);
void rdn_sub(void* state);
void rdn_mul(void* state);
void rdn_div(void* state);
void rdn_lt(void* state);
void rdn_gt(void* state);
void rdn_lte(void* state);
void rdn_gte(void* state);
void rdn_eq(void* state);
void rdn_neq(void* state);
void rdn_print(void* state);
void rdn_swap(void* state);
void* rdn_pop(void* state);
void rdn_push(void* state, void* value);
void* rdn_peek(void* state);
int rdn_len(void* state);
void rdn_clean(void* state);

#endif
