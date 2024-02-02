//
// Created by easy on 27.10.23.
//

#include "axvector.h"
#include <stdlib.h>
#include <string.h>

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

typedef unsigned long ulong;

struct axvector {
    void **items;
    ulong len;
    ulong cap;
    int (*comp)(const void *, const void *);
    void (*destroy)(void *);
    void *context;
    long refcount;
};


union Long {
    ulong u;
    long s;
};


static long len(axvector *v);
static bool resize(axvector *v, ulong size);


static union Long normaliseIndex(ulong len, long index) {
    union Long i = {.s = index};
    i.u += (i.s < 0) * len;
    return i;
}


static ulong toItemSize(ulong n) {
    return n * sizeof(void *);
}


static int defaultComparator(const void *a, const void *b) {
    const void *x = *(void **) a;
    const void *y = *(void **) b;
    return (x > y) - (x < y);
}


static axvector *sizedNew(ulong size) {
    size = MAX(1, size);
    axvector *v = malloc(sizeof *v);
    if (v) v->items = malloc(toItemSize(size));

    if (!v || !v->items) {
        free(v);
        return NULL;
    }

    v->len = 0;
    v->cap = size;
    v->comp = defaultComparator;
    v->context = NULL;
    v->destroy = NULL;
    v->refcount = 1;
    return v;
}


static axvector *new(void) {
    return sizedNew(7);
}


static void *destroy(axvector *v) {
    if (v->destroy) while (v->len) {
        v->destroy(v->items[--v->len]);
    }

    void *context = v->context;
    free(v->items);
    free(v);
    return context;
}


static axvector *iref(axvector *v) {
    ++v->refcount;
    return v;
}


static bool dref(axvector *v) {
    const bool destroyed = --v->refcount <= 0;
    if (destroyed) destroy(v);
    return destroyed;
}


static long refs(axvector *v) {
    return v->refcount;
}


static axvsnap snapshot(axvector *v) {
    return (axvsnap) {
        .i = 0,
        .len = len(v),
        .vec = v->items
    };
}


static bool push(axvector *v, void *val) {
    if (v->len >= v->cap) {
        ulong cap = (v->cap << 1) | 1;  // add another bit
        void **items = realloc(v->items, toItemSize(cap));
        if (!items) return true;
        v->items = items;
        v->cap = cap;
    }

    v->items[v->len++] = val;
    return false;
}


static void *pop(axvector *v) {
    return v->len ? v->items[--v->len] : NULL;
}


static void *top(axvector *v) {
    return v->len ? v->items[v->len - 1] : NULL;
}


static long len(axvector *v) {
    union Long len = {v->len};
    len.u = len.u << 1 >> 1;
    return len.s;
}


static void *at(axvector *v, long index) {
    ulong i = normaliseIndex(v->len, index).u;
    return i < v->len ? v->items[i] : NULL;
}


static bool set(axvector *v, long index, void *val) {
    ulong i = normaliseIndex(v->len, index).u;
    if (i >= v->len) return true;
    v->items[i] = val;
    return false;
}


static bool swap(axvector *v, long index1, long index2) {
    ulong i1 = normaliseIndex(v->len, index1).u;
    ulong i2 = normaliseIndex(v->len, index2).u;
    if (i1 >= v->len || i2 >= v->len)
        return true;

    void *tmp = v->items[i1];
    v->items[i1] = v->items[i2];
    v->items[i2] = tmp;
    return false;
}


static axvector *reverse(axvector *v) {
    void **l = v->items;
    void **r = v->items + v->len - 1;

    while (l < r) {
        void *tmp = *l;
        *l = *r;
        *r = tmp;
        ++l; --r;
    }

    return v;
}


static bool reverseSection(axvector *v, long index1, long index2) {
    ulong i1 = normaliseIndex(v->len, index1).u;
    ulong i2 = normaliseIndex(v->len, index2).u - 1;
    if (i1 >= v->len || i2 >= v->len)
        return true;

    void **l = v->items + i1;
    void **r = v->items + i2;

    while (l < r) {
        void *tmp = *l;
        *l = *r;
        *r = tmp;
        ++l; --r;
    }

    return false;
}


static axvector *rotate(axvector *v, long n) {
    if (n == 0) return v;
    reverse(v);
    reverseSection(v, 0, n - 1);
    reverseSection(v, n, -1);
    return v;
}


static bool shift(axvector *v, long index, unsigned long n) {
    if (n == 0)
        return false;
    if (v->len + n > v->cap && resize(v, v->len + n))
        return true;

    memmove(v->items + index + n, v->items + index, toItemSize(v->len - n - 1));
    return false;
}


static long discard(axvector *v, long n) {
    const long removed = n = MIN(len(v), n);

    if (v->destroy) while (n > 0) {
        v->destroy(v->items[v->len - n--]);
    }

    v->len -= removed;
    return removed;
}


static axvector *clear(axvector *v) {
    if (v->destroy) while (v->len) {
        v->destroy(v->items[--v->len]);
    }

    v->len = 0;
    return v;
}


static axvector *copy(axvector *v) {
    axvector *v2 = sizedNew(v->cap);
    if (!v2) return NULL;

    memcpy(v2->items, v->items, toItemSize(v->len));
    v2->len = v->len;
    v2->comp = v->comp;
    v2->context = v->context;
    v2->destroy = NULL;
    return v2;
}


static axvector *slice(axvector *v, long index1, long index2) {
    long i1 = index1 + (index1 < 0) * len(v);
    long i2 = index2 + (index2 < 0) * len(v);
    i1 = MAX(0, i1); i1 = MIN(i1, len(v));
    i2 = MAX(0, i2); i2 = MIN(i2, len(v));

    axvector *v2 = sizedNew(v->len);
    if (!v2) return NULL;

    memcpy(v2->items, v->items + i1, toItemSize(i2 - i1));
    v2->len = i2 - i1;
    v2->comp = v->comp;
    v2->context = v->context;
    v2->destroy = NULL;
    return v2;
}


static axvector *rslice(axvector *v, long index1, long index2) {
    long i1 = index1 + (index1 < 0) * len(v);
    long i2 = index2 + (index2 < 0) * len(v);
    i1 = MAX(0, i1); i1 = MIN(i1, len(v));
    i2 = MAX(0, i2); i2 = MIN(i2, len(v));

    axvector *v2 = sizedNew(v->len);
    if (!v2) return NULL;

    void **save = v2->items;
    for (long i = i2 - 1; i >= i1; --i) {
        *save++ = v->items[i];
    }

    v2->len = i2 - i1;
    v2->comp = v->comp;
    v2->context = v->context;
    v2->destroy = NULL;
    return v2;
}


static bool resize(axvector *v, ulong size) {
    size = MAX(1, size);

    if (size < v->len && v->destroy) while (v->len > size) {
        v->destroy(v->items[--v->len]);
    } else {
        v->len = MIN(v->len, size);
    }

    void **items = realloc(v->items, toItemSize(size));
    if (!items) return true;
    v->items = items;
    v->cap = size;
    return false;
}


static axvector *destroyItem(axvector *v, void *val) {
    if (v->destroy) v->destroy(val);
    return v;
}


static void *max(axvector *v) {
    if (v->len == 0) return NULL;

    void *max = *v->items;
    for (ulong i = 1; i < v->len; ++i) {
        if (v->comp(v->items + i, &max) > 0) {
            max = v->items[i];
        }
    }

    return max;
}


static void *min(axvector *v) {
    if (v->len == 0) return NULL;

    void *min = *v->items;
    for (ulong i = 1; i < v->len; ++i) {
        if (v->comp(v->items + i, &min) < 0) {
            min = v->items[i];
        }
    }

    return min;
}


static bool any(axvector *v, bool (*f)(const void *)) {
    void **val = v->items;
    void **bound = v->items + v->len;

    while (val < bound) {
        if (f(*val++)) return true;
    }

    return false;
}


static bool all(axvector *v, bool (*f)(const void *)) {
    void **val = v->items;
    void **bound = v->items + v->len;

    while (val < bound) {
        if (!f(*val++)) return false;
    }

    return true;
}


static long count(axvector *v, void *val) {
    long n = 0;
    void **curr = v->items;
    void **bound = v->items + v->len;
    while (curr < bound) n += v->comp(&val, curr++) == 0;
    return n;
}


static bool compare(axvector *v1, axvector *v2) {
    if (v1->len != v2->len) return false;

    for (ulong i = 0; i < v1->len; ++i) {
        if (v1->comp(v1->items + i, v2->items + i) != 0) {
            return false;
        }
    }

    return true;
}


static axvector *map(axvector *v, void *(*f)(void *)) {
    void **val = v->items;
    void **bound = v->items + v->len;

    while (val < bound) {
        *val = f(*val);
        ++val;
    }

    return v;
}


static axvector *filter(axvector *v, bool (*f)(const void *, void *), void *arg) {
    ulong len = 0;
    const bool shouldFree = v->destroy;

    for (ulong i = 0; i < v->len; ++i) {
        if (f(v->items[i], arg)) {
            v->items[len++] = v->items[i];
        } else if (shouldFree) {
            v->destroy(v->items[i]);
        }
    }

    v->len = len;
    return v;
}


static axvector *filterSplit(axvector *v, bool (*f)(const void *, void *), void *arg) {
    axvector *v2 = sizedNew(v->len);
    if (!v2) return NULL;

    ulong len1 = 0, len2 = 0;
    for (ulong i = 0; i < v->len; ++i) {
        if (f(v->items[i], arg)) {
            v->items[len1++] = v->items[i];
        } else {
            v2->items[len2++] = v->items[i];
        }
    }

    v->len = len1;
    v2->len = len2;
    v2->comp = v->comp;
    v2->context = v->context;
    v2->destroy = v->destroy;
    return v2;
}


static void *foreach(axvector *v, bool (*f)(void *, long, void *), void *arg) {
    const long length = len(v);
    for (long i = 0; i < length; ++i) {
        if (!f(v->items[i], i, arg)) {
            return arg;
        }
    }

    return arg;
}


static void *rforeach(axvector *v, bool (*f)(void *, long, void *), void *arg) {
    for (long i = len(v) - 1; i >= 0; --i) {
        if (!f(v->items[i], i, arg)) {
            return arg;
        }
    }

    return arg;
}


static void *forSection(axvector *v, bool (*f)(void *, long, void *), void *arg,
                        long index1, long index2) {

    long i1 = normaliseIndex(v->len, index1).s;
    long i2 = normaliseIndex(v->len, index2).s;
    i2 = MIN(len(v), i2); i2 = MAX(0, i2);

    for (long i = i1; i < i2; ++i) {
        if (!f(v->items[i], i, arg)) {
            return v;
        }
    }

    return arg;
}


static bool isSorted(axvector *v) {
    for (ulong i = 1; i < v->len; ++i) {
        if (v->comp(v->items + i - 1, v->items + i) != 0) {
            return false;
        }
    }

    return true;
}


static axvector *sort(axvector *v) {
    qsort(v->items, v->len, sizeof *v->items, v->comp);
    return v;
}


static axvector *sortSection(axvector *v, long index1, long index2) {
    ulong i1 = normaliseIndex(v->len, index1).u;
    ulong i2 = normaliseIndex(v->len, index2).u;
    qsort(v->items + i1, i2 - i1, sizeof *v->items, v->comp);
    return v;
}


static long binarySearch(axvector *v, void *val) {
    void **found = bsearch(&val, v->items, v->len, sizeof *v->items, v->comp);
    return found ? found - v->items : -1;
}


static long linearSearch(axvector *v, void *val) {
    const long length = len(v);
    for (long i = 0; i < length; ++i) {
        if (v->comp(&val, v->items + i) == 0) {
            return i;
        }
    }

    return -1;
}


static long linearSearchSection(axvector *v, void *val, long index1, long index2) {
    long i1 = normaliseIndex(v->len, index1).s;
    long i2 = normaliseIndex(v->len, index2).s;
    if (i1 >= len(v) || i2 > len(v) || i1 < 0 || i2 < 0)
        return -1;

    for (long i = i1; i < i2; ++i) {
        if (v->comp(&val, v->items + i) == 0) {
            return i;
        }
    }

    return -1;
}


static axvector *setComparator(axvector *v, int (*comp)(const void *, const void *)) {
    v->comp = comp ? comp : defaultComparator;
    return v;
}


static int (*getComparator(axvector *v))(const void *, const void *) {
    return v->comp;
}


static axvector *setDestructor(axvector *v, void (*destroy)(void *)) {
    v->destroy = destroy;
    return v;
}


static void (*getDestructor(axvector *v))(void *) {
    return v->destroy;
}


static axvector *setContext(axvector *v, void *context) {
    v->context = context;
    return v;
}


static void *getContext(axvector *v) {
    return v->context;
}


static void **data(axvector *v) {
    return v->items;
}


static long cap(axvector *v) {
    union Long cap = {v->cap};
    cap.u = cap.u << 1 >> 1;
    return cap.s;
}


#ifdef AXVECTOR_NAMESPACE
#define axv AXVECTOR_NAMESPACE
#endif

const struct axvectorFn axv = {
        sizedNew,
        new,
        destroy,
        iref,
        dref,
        refs,
        snapshot,
        push,
        pop,
        top,
        len,
        at,
        set,
        swap,
        reverse,
        reverseSection,
        rotate,
        shift,
        discard,
        clear,
        copy,
        slice,
        rslice,
        resize,
        destroyItem,
        max,
        min,
        any,
        all,
        count,
        compare,
        map,
        filter,
        filterSplit,
        foreach,
        rforeach,
        forSection,
        isSorted,
        sort,
        sortSection,
        binarySearch,
        linearSearch,
        linearSearchSection,
        setComparator,
        getComparator,
        setDestructor,
        getDestructor,
        setContext,
        getContext,
        data,
        cap
};

#undef axv
