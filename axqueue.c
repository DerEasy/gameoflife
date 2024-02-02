//
// Created by easy on 19.10.23.
//

#include "axqueue.h"
#include <stdlib.h>
#include <string.h>

#define BOUND(q) ((q)->items + (q)->cap)
#define MAX(x, y) ((x) > (y) ? (x) : (y))

typedef unsigned long ulong;

struct axqueue {
    void **items;   // base of array
    void **write;   // points to next cell to be written to
    void **read;    // points to current cell to be read from
    ulong len;
    ulong cap;
    void (*destroy)(void *);
};

union Long {
    ulong u;
    long s;
};


static ulong toItemSize(ulong n) {
    return n * sizeof(void *);
}
    

static axqueue *sizedNew(ulong size) {
    size = MAX(1, size);
    axqueue *q = malloc(sizeof *q);
    if (q) q->items = malloc(toItemSize(size));

    if (!q || !q->items) {
        free(q);
        return NULL;
    }

    q->write = q->read = q->items;
    q->len = 0;
    q->cap = size;
    q->destroy = NULL;
    return q;
}


static axqueue *new(void) {
    return sizedNew(7);
}


static void destroy(axqueue *q) {
    q->read -= q->cap * (q->read >= BOUND(q));
    if (q->destroy && q->len) do {
        q->destroy(*q->read++);
        q->read -= q->cap * (q->read >= BOUND(q));
    } while (q->read != q->write);

    free(q->items);
    free(q);
}


static bool grow(axqueue *q) {
    ulong cap = q->cap * 2;
    void **items = realloc(q->items, toItemSize(cap));
    if (!items) return true;

    // set r/w heads to their possibly new addresses
    q->write = items + (q->write - q->items);
    q->read = items + (q->read - q->items);
    q->items = items;

    if (q->write <= q->read) {   // else, write already'd have space to the right
        ulong num = BOUND(q) - q->read;  // items from read head to end of array
        memcpy(q->read + q->cap, q->read, toItemSize(num));
        q->read += q->cap;
    }

    q->cap = cap;   // finally, save the doubled capacity
    return false;
}


static bool enqueue(axqueue *q, void *val) {
    if (q->len >= q->cap) {
        if (grow(q)) return true;
    }

    *q->write++ = val;
    q->write -= q->cap * (q->write >= BOUND(q));   // wrap-around
    ++q->len;
    return false;
}


static void *dequeue(axqueue *q) {
    void *val = q->len ? *q->read++ : NULL;
    q->read -= q->cap * (q->read >= BOUND(q));   // wrap-around
    q->len -= !!q->len;
    return val;
}


static void *front(axqueue *q) {
    return q->len ? *q->read : NULL;
}


static union Long normaliseIndex(axqueue *q, long index) {
    union Long i = {.s = index};

    i.u += (i.s < 0) * q->len;    // convert negative to positive index

    if (i.u < q->len) {    // if index valid
        void **pval = q->read + i.u;
        pval -= q->cap * (pval >= BOUND(q));
        return (union Long) {.s = pval - q->items};
    } else {    // index out of range
        return (union Long) {.u = q->cap};
    }
}


static void *at(axqueue *q, long index) {
    ulong i = normaliseIndex(q, index).u;
    return i < q->cap ? q->items[i] : NULL;
}


static bool swap(axqueue *q, long index1, long index2) {
    ulong i1 = normaliseIndex(q, index1).u;
    ulong i2 = normaliseIndex(q, index2).u;
    if (i1 >= q->cap || i2 >= q->cap)
        return true;

    void *tmp = q->items[i1];
    q->items[i1] = q->items[i2];
    q->items[i2] = tmp;
    return false;
}


static axqueue *reverse(axqueue *q) {
    void **l = q->read;
    void **r = q->write - 1;
    r += q->cap * (r < q->items);

    for (ulong i = 0, swaps = q->len / 2; i < swaps; ++i) {
        void *tmp = *l;
        *l = *r;
        *r = tmp;
        ++l; --r;

        l -= q->cap * (l >= BOUND(q));
        r += q->cap * (r < q->items);
    }

    return q;
}


static axqueue *clear(axqueue *q) {
    q->read -= q->cap * (q->read >= BOUND(q));
    if (q->destroy && q->len) do {
        q->destroy(*q->read++);
        q->read -= q->cap * (q->read >= BOUND(q));
    } while (q->read != q->write);

    q->write = q->read = q->items;
    q->len = 0;
    return q;
}


static axqueue *copy(axqueue *q) {
    axqueue *q2 = sizedNew(q->cap);
    if (!q2) return NULL;

    if (q->read < q->write || q->len == 0) {
        memcpy(q2->items, q->read, toItemSize(q->len));
    } else {
        ulong lsize = q->write - q->items;
        ulong rsize = q->len - lsize;
        memcpy(q2->items, q->read, toItemSize(rsize));
        memcpy(q2->items + rsize, q->items, toItemSize(lsize));
    }

    // q2->read implicity set to q2->items by sizedNew()
    q2->write = q2->items + q->len;
    q2->len = q->len;
    return q2;
}


static void reverseSection(void **s, void **e) {    // do not expose
    while (s < e) {
        void *tmp = *s;
        *s = *e;
        *e = tmp;
        ++s; --e;
    }
}


static void rotate(axqueue *q, ulong n) {       // do not expose
    if (n == 0) return;
    reverseSection(q->items, BOUND(q) - 1);
    reverseSection(q->items, q->items + n - 1);
    reverseSection(q->items + n, BOUND(q) - 1);
}


static bool resize(axqueue *q, ulong size) {
    size = MAX(1, size);

    if (size == q->cap) {
        return false;
    } else if (size > q->cap) {
        void **items = realloc(q->items, toItemSize(size));
        if (!items) return true;

        // set r/w heads to their possibly new addresses
        q->write = items + (q->write - q->items);
        q->read = items + (q->read - q->items);
        q->items = items;

        if (q->write <= q->read && q->len) {
            ulong num = BOUND(q) - q->read;
            ulong extra = size - q->cap;
            memmove(q->read + extra, q->read, toItemSize(num));
            q->read += extra;
        }

        q->cap = size;
        return false;
    } else /*(size < q->cap)*/ {
        ulong lost = size < q->len ? q->len - size : 0;

        if (q->destroy) for (ulong i = 0; i < lost; ++i) {
            q->read -= q->cap * (q->read >= BOUND(q));
            q->destroy(*q->read++);
        } else {
            q->read += lost;
            q->read -= q->cap * (q->read >= BOUND(q));
        }

        rotate(q, q->cap - (q->read - q->items));

        void **items = realloc(q->items, toItemSize(size));
        bool failed = !items;
        if (failed) items = q->items;
        else q->cap = size;

        q->len -= lost;
        q->items = items;
        q->read = items;
        q->write = items + q->len;
        q->write -= q->cap * (q->write >= BOUND(q));    // shouldn't happen (?!)
        return failed;
    }
}


static axqueue *destroyItem(axqueue *q, void *val) {
    if (q->destroy) q->destroy(val);
    return q;
}


static axqueue *setDestructor(axqueue *q, void (*destroy)(void *)) {
    q->destroy = destroy;
    return q;
}


static void (*getDestructor(axqueue *q))(void *) {
    return q->destroy;
}


static void **data(axqueue *q) {
    if (q->write <= q->read && q->len)
        rotate(q, q->cap - (q->read - q->items));
    return q->read;
}


static long len(axqueue *q) {
    union Long len = {q->len};
    len.u = len.u << 1 >> 1;    // get rid of sign bit
    return len.s;
}


static long cap(axqueue *q) {
    union Long cap = {q->cap};
    cap.u = cap.u << 1 >> 1;    // get rid of sign bit
    return cap.s;
}


#ifdef AXQUEUE_NAMESPACE
#define axq AXQUEUE_NAMESPACE
#endif

const struct axqueueFn axq = {
        new,
        sizedNew,
        destroy,
        enqueue,
        dequeue,
        front,
        len,
        at,
        swap,
        reverse,
        clear,
        copy,
        resize,
        destroyItem,
        setDestructor,
        getDestructor,
        data,
        cap
};

#undef axq
