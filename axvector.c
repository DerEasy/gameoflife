//
// Created by easy on 27.10.23.
//

#include "axvector.h"
#include <stdlib.h>
#include <string.h>

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))


struct axvector {
    void **items;
    uint64_t len;
    uint64_t cap;
    int (*cmp)(const void *, const void *);
    void (*destroy)(void *);
    void *context;
    int64_t refcount;
};


union Int64 {
    uint64_t u;
    int64_t s;
};


static union Int64 normaliseIndex(uint64_t len, int64_t index) {
    union Int64 i = {.s = index};
    i.u += (i.s < 0) * len;
    return i;
}


static uint64_t toItemSize(uint64_t n) {
    return n * sizeof(void *);
}


static int defaultComparator(const void *a, const void *b) {
    const void *x = *(const void **) a;
    const void *y = *(const void **) b;
    return (x > y) - (x < y);
}


axvector *axv_sizedNew(uint64_t size) {
    size = MAX(1, size);
    axvector *v = malloc(sizeof *v);
    if (v) v->items = malloc(toItemSize(size));

    if (!v || !v->items) {
        free(v);
        return NULL;
    }

    v->len = 0;
    v->cap = size;
    v->cmp = defaultComparator;
    v->context = NULL;
    v->destroy = NULL;
    v->refcount = 1;
    return v;
}


axvector *axv_new(void) {
    return axv_sizedNew(7);
}


void *axv_destroy(axvector *v) {
    if (v->destroy) while (v->len) {
        v->destroy(v->items[--v->len]);
    }

    void *context = v->context;
    free(v->items);
    free(v);
    return context;
}


axvector *axv_iref(axvector *v) {
    ++v->refcount;
    return v;
}


bool axv_dref(axvector *v) {
    const bool destroyed = --v->refcount <= 0;
    if (destroyed) axv_destroy(v);
    return destroyed;
}


int64_t axv_refs(axvector *v) {
    return v->refcount;
}


axvsnap axv_snapshot(axvector *v) {
    return (axvsnap) {
        .i = 0,
        .len = axv_len(v),
        .vec = v->items
    };
}


bool axv_push(axvector *v, void *val) {
    if (v->len >= v->cap) {
        uint64_t cap = (v->cap << 1) | 1;  // add another bit
        void **items = realloc(v->items, toItemSize(cap));
        if (!items) return true;
        v->items = items;
        v->cap = cap;
    }

    v->items[v->len++] = val;
    return false;
}


void *axv_pop(axvector *v) {
    return v->len ? v->items[--v->len] : NULL;
}


void *axv_top(axvector *v) {
    return v->len ? v->items[v->len - 1] : NULL;
}


int64_t axv_len(axvector *v) {
    return (const union Int64) {v->len}.s;
}


void *axv_at(axvector *v, int64_t index) {
    uint64_t i = normaliseIndex(v->len, index).u;
    return i < v->len ? v->items[i] : NULL;
}


bool axv_set(axvector *v, int64_t index, void *val) {
    uint64_t i = normaliseIndex(v->len, index).u;
    if (i >= v->len) return true;
    v->items[i] = val;
    return false;
}


bool axv_swap(axvector *v, int64_t index1, int64_t index2) {
    uint64_t i1 = normaliseIndex(v->len, index1).u;
    uint64_t i2 = normaliseIndex(v->len, index2).u;
    if (i1 >= v->len || i2 >= v->len)
        return true;

    void *tmp = v->items[i1];
    v->items[i1] = v->items[i2];
    v->items[i2] = tmp;
    return false;
}


axvector *axv_reverse(axvector *v) {
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


bool axv_reverseSection(axvector *v, int64_t index1, int64_t index2) {
    uint64_t i1 = normaliseIndex(v->len, index1).u;
    uint64_t i2 = normaliseIndex(v->len, index2).u;
    if (i1 >= v->len || i2 > v->len)
        return true;

    void **l = v->items + i1;
    void **r = v->items + i2 - 1;

    while (l < r) {
        void *tmp = *l;
        *l = *r;
        *r = tmp;
        ++l; --r;
    }

    return false;
}


axvector *axv_rotate(axvector *v, int64_t k) {
    k %= axv_len(v);
    if (k == 0) return v;
    axv_reverse(v);
    axv_reverseSection(v, 0, k);
    axv_reverseSection(v, k, axv_len(v));
    return v;
}


bool axv_shift(axvector *v, int64_t index, int64_t n) {
    if (n == 0)
        return false;

    if (n > 0) {
        const uint64_t oldlen = v->len;
        if (v->len + n > v->cap && axv_resize(v, v->len + n))
            return true;
        memmove(v->items + index + n, v->items + index, toItemSize(oldlen - n - 1));
        memset(v->items + index, 0, toItemSize(n));
        if (oldlen == v->len)
            v->len += n;
    } else {
        n = MIN(-n, axv_len(v) - index);
        if (v->destroy) {
            for (int64_t i = index; i < index + n; ++i)
                v->destroy(v->items[i]);
        }
        memmove(v->items + index, v->items + index + n, toItemSize(v->len - index - n));
        v->len -= n;
    }

    return false;
}


axvector *axv_discard(axvector *v, uint64_t n) {
    n = v->len - MIN(v->len, n);

    if (v->destroy) while (v->len > n) {
        v->destroy(v->items[--v->len]);
    }

    return v;
}


axvector *axv_clear(axvector *v) {
    if (v->destroy) while (v->len) {
        v->destroy(v->items[--v->len]);
    }

    v->len = 0;
    return v;
}


axvector *axv_copy(axvector *v) {
    axvector *v2 = axv_sizedNew(v->cap);
    if (!v2) return NULL;

    memcpy(v2->items, v->items, toItemSize(v->len));
    v2->len = v->len;
    v2->cmp = v->cmp;
    v2->context = v->context;
    v2->destroy = NULL;
    return v2;
}


bool axv_extend(axvector *v1, axvector *v2) {
    if (v1 == v2)
        return false;

    const uint64_t extlen = v1->len + v2->len;
    if (extlen > v1->cap && axv_resize(v1, extlen))
        return true;

    memcpy(v1->items + v1->len, v2->items, toItemSize(v2->len));
    v1->len = extlen;
    v2->len = 0;
    return false;
}


bool axv_concat(axvector *v1, axvector *v2) {
    const uint64_t extlen = v1->len + v2->len;
    if (extlen > v1->cap && axv_resize(v1, extlen))
        return true;

    memcpy(v1->items + v1->len, v2->items, toItemSize(v2->len));
    v1->len = extlen;
    return false;
}


axvector *axv_slice(axvector *v, int64_t index1, int64_t index2) {
    int64_t i1 = index1 + (index1 < 0) * axv_len(v);
    int64_t i2 = index2 + (index2 < 0) * axv_len(v);
    i1 = MAX(0, i1); i1 = MIN(i1, axv_len(v));
    i2 = MAX(0, i2); i2 = MIN(i2, axv_len(v));

    axvector *v2 = axv_sizedNew(v->len);
    if (!v2) return NULL;

    memcpy(v2->items, v->items + i1, toItemSize(i2 - i1));
    v2->len = i2 - i1;
    v2->cmp = v->cmp;
    v2->context = v->context;
    v2->destroy = NULL;
    return v2;
}


axvector *axv_rslice(axvector *v, int64_t index1, int64_t index2) {
    int64_t i1 = index1 + (index1 < 0) * axv_len(v);
    int64_t i2 = index2 + (index2 < 0) * axv_len(v);
    i1 = MAX(0, i1); i1 = MIN(i1, axv_len(v));
    i2 = MAX(0, i2); i2 = MIN(i2, axv_len(v));

    axvector *v2 = axv_sizedNew(v->len);
    if (!v2) return NULL;

    void **save = v2->items;
    for (int64_t i = i2 - 1; i >= i1; --i) {
        *save++ = v->items[i];
    }

    v2->len = i2 - i1;
    v2->cmp = v->cmp;
    v2->context = v->context;
    v2->destroy = NULL;
    return v2;
}


bool axv_resize(axvector *v, uint64_t size) {
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


axvector *axv_destroyItem(axvector *v, void *val) {
    if (v->destroy) v->destroy(val);
    return v;
}


void *axv_max(axvector *v) {
    if (v->len == 0) return NULL;

    void *max = *v->items;
    for (uint64_t i = 1; i < v->len; ++i) {
        if (v->cmp(v->items + i, &max) > 0) {
            max = v->items[i];
        }
    }

    return max;
}


void *axv_min(axvector *v) {
    if (v->len == 0) return NULL;

    void *min = *v->items;
    for (uint64_t i = 1; i < v->len; ++i) {
        if (v->cmp(v->items + i, &min) < 0) {
            min = v->items[i];
        }
    }

    return min;
}


bool axv_any(axvector *v, bool (*f)(const void *, void *), void *arg) {
    void **val = v->items;
    void **bound = v->items + v->len;

    while (val < bound) {
        if (f(*val++, arg)) return true;
    }

    return false;
}


bool axv_all(axvector *v, bool (*f)(const void *, void *), void *arg) {
    void **val = v->items;
    void **bound = v->items + v->len;

    while (val < bound) {
        if (!f(*val++, arg)) return false;
    }

    return true;
}


int64_t axv_count(axvector *v, void *val) {
    int64_t n = 0;
    void **curr = v->items;
    void **bound = v->items + v->len;
    while (curr < bound) n += v->cmp(&val, curr++) == 0;
    return n;
}


bool axv_compare(axvector *v1, axvector *v2) {
    if (v1->len != v2->len) return false;

    for (uint64_t i = 0; i < v1->len; ++i) {
        if (v1->cmp(v1->items + i, v2->items + i) != 0) {
            return false;
        }
    }

    return true;
}


axvector *axv_map(axvector *v, void *(*f)(void *)) {
    void **val = v->items;
    void **bound = v->items + v->len;

    while (val < bound) {
        *val = f(*val);
        ++val;
    }

    return v;
}


axvector *axv_filter(axvector *v, bool (*f)(const void *, void *), void *arg) {
    uint64_t len = 0;
    const bool shouldFree = v->destroy;

    for (uint64_t i = 0; i < v->len; ++i) {
        if (f(v->items[i], arg)) {
            v->items[len++] = v->items[i];
        } else if (shouldFree) {
            v->destroy(v->items[i]);
        }
    }

    v->len = len;
    return v;
}


axvector *axv_filterSplit(axvector *v, bool (*f)(const void *, void *), void *arg) {
    axvector *v2 = axv_sizedNew(v->len);
    if (!v2) return NULL;

    uint64_t len1 = 0, len2 = 0;
    for (uint64_t i = 0; i < v->len; ++i) {
        if (f(v->items[i], arg)) {
            v->items[len1++] = v->items[i];
        } else {
            v2->items[len2++] = v->items[i];
        }
    }

    v->len = len1;
    v2->len = len2;
    v2->cmp = v->cmp;
    v2->context = v->context;
    v2->destroy = v->destroy;
    return v2;
}


axvector *axv_foreach(axvector *v, bool (*f)(void *, void *), void *arg) {
    const int64_t length = axv_len(v);
    for (int64_t i = 0; i < length; ++i) {
        if (!f(v->items[i], arg)) {
            return arg;
        }
    }

    return arg;
}


axvector *axv_rforeach(axvector *v, bool (*f)(void *, void *), void *arg) {
    for (int64_t i = axv_len(v) - 1; i >= 0; --i) {
        if (!f(v->items[i], arg)) {
            return arg;
        }
    }

    return arg;
}


axvector *axv_forSection(axvector *v, bool (*f)(void *, void *), void *arg,
                         int64_t index1, int64_t index2) {

    int64_t i1 = normaliseIndex(v->len, index1).s;
    int64_t i2 = normaliseIndex(v->len, index2).s;
    i2 = MIN(axv_len(v), i2); i2 = MAX(0, i2);

    for (int64_t i = i1; i < i2; ++i) {
        if (!f(v->items[i], arg)) {
            return v;
        }
    }

    return arg;
}


bool axv_isSorted(axvector *v) {
    for (uint64_t i = 1; i < v->len; ++i) {
        if (v->cmp(v->items + i - 1, v->items + i) != 0) {
            return false;
        }
    }

    return true;
}


axvector *axv_sort(axvector *v) {
    qsort(v->items, v->len, sizeof *v->items, v->cmp);
    return v;
}


axvector *axv_sortSection(axvector *v, int64_t index1, int64_t index2) {
    uint64_t i1 = normaliseIndex(v->len, index1).u;
    uint64_t i2 = normaliseIndex(v->len, index2).u;
    qsort(v->items + i1, i2 - i1, sizeof *v->items, v->cmp);
    return v;
}


int64_t axv_binarySearch(axvector *v, void *val) {
    void **found = bsearch(&val, v->items, v->len, sizeof *v->items, v->cmp);
    return found ? found - v->items : -1;
}


int64_t axv_linearSearch(axvector *v, void *val) {
    const int64_t length = axv_len(v);
    for (int64_t i = 0; i < length; ++i) {
        if (v->cmp(&val, v->items + i) == 0) {
            return i;
        }
    }

    return -1;
}


int64_t axv_linearSearchSection(axvector *v, void *val, int64_t index1, int64_t index2) {
    int64_t i1 = normaliseIndex(v->len, index1).s;
    int64_t i2 = normaliseIndex(v->len, index2).s;
    if (i1 >= axv_len(v) || i2 > axv_len(v) || i1 < 0 || i2 < 0)
        return -1;

    for (int64_t i = i1; i < i2; ++i) {
        if (v->cmp(&val, v->items + i) == 0) {
            return i;
        }
    }

    return -1;
}


axvector *axv_setComparator(axvector *v, int (*cmp)(const void *, const void *)) {
    v->cmp = cmp ? cmp : defaultComparator;
    return v;
}


int (*axv_getComparator(axvector *v))(const void *, const void *) {
    return v->cmp;
}


axvector *axv_setDestructor(axvector *v, void (*destroy)(void *)) {
    v->destroy = destroy;
    return v;
}


void (*axv_getDestructor(axvector *v))(void *) {
    return v->destroy;
}


axvector *axv_setContext(axvector *v, void *context) {
    v->context = context;
    return v;
}


void *axv_getContext(axvector *v) {
    return v->context;
}


void **axv_data(axvector *v) {
    return v->items;
}


int64_t axv_cap(axvector *v) {
    return (const union Int64) {v->cap}.s;
}
