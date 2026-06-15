#ifndef RC_H_
#define RC_H_

#include <stddef.h>
#include <stdlib.h>

typedef struct {
    ptrdiff_t count;
    void (*destroy)(void *data);
} Rc;

void *rc_alloc(size_t size, void (*destroy)(void *data));
void *rc_acquire(void *data);
void rc_release(void *data);
ptrdiff_t rc_count(void *data);

#endif // RC_H_

#ifdef RC_IMPLEMENTATION

void *rc_alloc(size_t size, void (*destroy)(void *data))
{
    Rc *rc = malloc(sizeof(Rc) + size);
    if (rc == NULL) {
        return NULL;
    }
    rc->count = 0;
    rc->destroy = destroy;
#ifdef RC_DEBUG
    printf("[RC] %p allocated\n", rc);
#endif
    return rc + 1;
}

void *rc_acquire(void *data)
{
    if (data == NULL) {
        return NULL;
    }

    Rc *rc = (Rc*)data - 1;
    rc->count += 1;
#ifdef RC_DEBUG
    printf("[RC] %p acquired\n", rc);
#endif
    return data;
}

void rc_release(void *data)
{
    if (data == NULL) {
        return;
    }

    Rc *rc = (Rc*)data - 1;
    rc->count -= 1;
    if (rc->count <= 0) {
        if (rc->destroy != NULL) {
            rc->destroy(rc + 1);
        }
#ifdef RC_DEBUG
        printf("[RC] %p released\n", rc);
#endif
        free(rc);
    }
}

ptrdiff_t rc_count(void *data)
{
    if (data == NULL) {
        return 0;
    }

    Rc *rc = (Rc*)data - 1;
    return rc->count;
}

#endif // RC_IMPLEMENTATION
