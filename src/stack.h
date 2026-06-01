#ifndef STACK
#define STACK

#include <assert.h>
#include <stdlib.h>

#define RLList(T)        \
    struct {             \
        T* items;        \
        size_t count;    \
        size_t capacity; \
    }

#define RLStack(T) RLList(T)

#define RAY_INIT_CAP 256

#ifdef __cplusplus
#define RAY_DECL_TYPE_CAST(T) (decltype(T))
#else
#define RAY_DECL_TYPE_CAST(T)
#endif // __cplusplus

#define ray_pop(da) (da)->items[(assert((da)->count > 0), --(da)->count)]

#define ray_reserve(da, expected_capacity)                                                                              \
    do {                                                                                                                \
        if ((expected_capacity) > (da)->capacity) {                                                                     \
            if ((da)->capacity == 0) {                                                                                  \
                (da)->capacity = RAY_INIT_CAP;                                                                          \
            }                                                                                                           \
            while ((expected_capacity) > (da)->capacity) {                                                              \
                (da)->capacity *= 2;                                                                                    \
            }                                                                                                           \
            (da)->items = RAY_DECL_TYPE_CAST((da)->items)realloc((da)->items, (da)->capacity * sizeof(*(da)->items));   \
            assert((da)->items != NULL && "Buy more RAM lol");                                                          \
        }                                                                                                               \
    } while (0)

#define ray_append(da, item)                    \
    do {                                        \
        ray_reserve((da), (da)->count + 1);     \
        (da)->items[(da)->count++] = (item);    \
    } while (0)

#define ray_is_empty(da) (da)->count == 0

#define ray_peek(da) (da)->items[(assert((da)->count > 0), (da)->count - 1)]

#define ray_clear(da) free((da)->items)

#define ray_remove_idx(da, i)               \
    do {                                             \
        size_t j = (i);                              \
        assert(j < (da)->count);                 \
        (da)->items[j] = (da)->items[--(da)->count]; \
    } while(0)
// TODO: add foreach 

#endif // !STACK
