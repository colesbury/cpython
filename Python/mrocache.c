#include "Python.h"

#include "pycore_critical_section.h"    // _Py_CRITICAL_SECTION_ASSERT_MUTEX_LOCKED
#include "pycore_initconfig.h"          // _PyStatus_OK()
#include "pycore_mrocache.h"            // _Py_mro_cache_entry
#include "pycore_pymem.h"               // _PyMem_FreeDelayed()

#include <stdbool.h>
#include <stddef.h>

#define _Py_MRO_CACHE_MIN_SIZE 8
#define _Py_MRO_CACHE_MAX_SIZE 65536

#define ASSERT_TYPE_LOCK_HELD() \
    _Py_CRITICAL_SECTION_ASSERT_MUTEX_LOCKED(&PyInterpreterState_Get()->types.mutex)

/* NOTE: mask is used to index array in bytes */
static uint32_t
mask_from_capacity(size_t capacity)
{
    assert((capacity & (capacity - 1)) == 0);
    assert(capacity >= _Py_MRO_CACHE_MIN_SIZE);

    return (uint32_t)((capacity - 1) * sizeof(_Py_mro_cache_entry));
}

static size_t
capacity_from_mask(Py_ssize_t mask)
{
    return (mask / sizeof(_Py_mro_cache_entry)) + 1;
}

static void
enqueue_bucket_to_free(struct _Py_mro_cache_state *mro_state, _Py_mro_cache_buckets *b)
{
    if (mro_state->buckets_to_free.head == NULL) {
        mro_state->buckets_to_free.head = b;
        mro_state->buckets_to_free.tail = b;
    }
    else {
        mro_state->buckets_to_free.tail->next = b;
        mro_state->buckets_to_free.tail = b;
    }
    _Py_mro_process_freed_buckets(_PyInterpreterState_GET());
}

static void
decref_empty_bucket(_Py_mro_cache_buckets *buckets)
{
    assert(buckets->u.refcount > 0);
    buckets->u.refcount--;
    if (buckets->u.refcount == 0) {
        PyInterpreterState *interp = _PyInterpreterState_GET();
        enqueue_bucket_to_free(&interp->mro_state, buckets);
    }
}

static void
clear_buckets(_Py_mro_cache_buckets *buckets)
{
    if (buckets->used == 0 && buckets->available == 0) {
        decref_empty_bucket(buckets);
    }
    else {
        PyInterpreterState *interp = _PyInterpreterState_GET();
        enqueue_bucket_to_free(&interp->mro_state, buckets);
    }
}

static void
buckets_free(void *ptr)
{
    _Py_mro_cache_buckets *buckets = (_Py_mro_cache_buckets *)ptr;
    Py_ssize_t capacity = buckets->u.capacity;
    for (Py_ssize_t i = 0; i < capacity; i++) {
        PyObject *value = (PyObject *)(buckets->array[i].value & ~1);
        Py_XDECREF(value);
    }
    PyMem_Free(buckets);
}

void
_Py_mro_process_freed_buckets(PyInterpreterState *interp)
{
    struct _Py_mro_cache_state *mro_state = &interp->mro_state;
    _Py_mro_cache_buckets *buckets = mro_state->buckets_to_free.head;
    mro_state->buckets_to_free.head = NULL;
    mro_state->buckets_to_free.tail = NULL;
    while (buckets != NULL) {
        _Py_mro_cache_buckets *next = buckets->next;
        if (buckets->used == 0 && buckets->available == 0) {
            // empty bucket; no contents to decref
            // _PyMem_FreeDelayed(buckets);
            PyMem_Free(buckets);
        }
        else {
            buckets_free(buckets);
        }
        buckets = next;
    }
}

static _Py_mro_cache_buckets *
allocate_empty_buckets(Py_ssize_t n)
{
    _Py_mro_cache_buckets *buckets;
    buckets = PyMem_Calloc(1, sizeof(*buckets) + n * sizeof(*buckets->array));
    if (buckets != NULL) {
        buckets->u.refcount = 1;
        buckets->cap2 = n;
    }
    return buckets;
}

static _Py_mro_cache_buckets *
get_buckets(_Py_mro_cache *cache)
{
    return _Py_CONTAINER_OF(cache->buckets, _Py_mro_cache_buckets, array);
}

static _Py_mro_cache_buckets *
allocate_buckets(Py_ssize_t capacity)
{
    if (capacity > _Py_MRO_CACHE_MAX_SIZE) {
        return NULL;
    }

    /* Ensure that there is an empty buckets array of at least the same capacity. */
    PyInterpreterState *interp = _PyInterpreterState_GET();
    if (capacity > interp->mro_state.empty_buckets_capacity) {
        _Py_mro_cache_buckets *old = interp->mro_state.empty_buckets;
        _Py_mro_cache_buckets *new = allocate_empty_buckets(capacity);
        if (new == NULL) {
            return NULL;
        }
        interp->mro_state.empty_buckets = new;
        interp->mro_state.empty_buckets_capacity = capacity;
        decref_empty_bucket(old);
    }

    Py_ssize_t size = sizeof(_Py_mro_cache_buckets) + capacity * sizeof(_Py_mro_cache_entry);
    _Py_mro_cache_buckets *buckets = PyMem_Calloc(1, size);
    if (buckets == NULL) {
        return NULL;
    }
    buckets->u.capacity = capacity;
    buckets->available = (capacity + 1) * 7 / 8;
    buckets->used = 0;
    return buckets;
}

void
_Py_mro_cache_erase(_Py_mro_cache *cache)
{
    // if (!Py_IsFinalizing()) {
    //     ASSERT_TYPE_LOCK_HELD();
    // }
    if (cache->buckets == NULL) {
        // The cache is not yet initialized
        return;
    }
    _Py_mro_cache_buckets *old = get_buckets(cache);
    if (old->available == 0 && old->used == 0) {
        // The empty bucket doesn't need to be erased
        return;
    }

    PyInterpreterState *interp = _PyInterpreterState_GET();
    struct _Py_mro_cache_state *mro_state = &interp->mro_state;
    assert(capacity_from_mask(cache->mask) <= (size_t)mro_state->empty_buckets_capacity);

    _Py_mro_cache_buckets *empty_buckets = mro_state->empty_buckets;
    empty_buckets->u.refcount++;
    _Py_atomic_store_ptr_release(&cache->buckets, empty_buckets->array);

    enqueue_bucket_to_free(mro_state, old);
    _Py_mro_process_freed_buckets(interp);
}

static int
resize(_Py_mro_cache *cache, _Py_mro_cache_buckets *buckets)
{
    size_t old_capacity = capacity_from_mask(cache->mask);
    size_t new_capacity;
    if (buckets->used == 0) {
        /* empty bucket */
        new_capacity = old_capacity;
    }
    else {
        new_capacity = old_capacity * 2;
    }
    uint32_t new_mask = mask_from_capacity(new_capacity);

    _Py_mro_cache_buckets *new_buckets = allocate_buckets(new_capacity);
    if (new_buckets == NULL) {
        return -1;
    }

    // First store the new buckets.
    _Py_atomic_store_ptr_release(&cache->buckets, new_buckets->array);

    // Then update the mask (with at least release semantics) so that
    // the buckets is visible first.
    _Py_atomic_store_uint32(&cache->mask, new_mask);

    clear_buckets(buckets);
    return 0;
}

void
_Py_mro_cache_insert(_Py_mro_cache *cache, PyObject *name, PyObject *value)
{
    assert(PyUnicode_CheckExact(name) && PyUnicode_CHECK_INTERNED(name));
    ASSERT_TYPE_LOCK_HELD();

    _Py_mro_cache_buckets *buckets = get_buckets(cache);
    if (buckets->available == 0) {
        if (resize(cache, buckets) < 0) {
            // allocation failure: don't cache the value
            return;
        }
        buckets = get_buckets(cache);
        assert(buckets->available > 0);
    }

    assert(buckets->available < UINT32_MAX/10);

    Py_hash_t hash = ((PyASCIIObject *)name)->hash;
    Py_ssize_t capacity = capacity_from_mask(cache->mask);
    Py_ssize_t ix = (hash & cache->mask) / sizeof(_Py_mro_cache_entry);
    for (;;) {
        if (buckets->array[ix].name == NULL) {
            uintptr_t v = value ? (uintptr_t)Py_NewRef(value) : 1;
            _Py_atomic_store_ptr_relaxed(&buckets->array[ix].name, name);
            _Py_atomic_store_uintptr_relaxed(&buckets->array[ix].value, v);
            assert(buckets->available > 0);
            buckets->available--;
            buckets->used++;
            return;
        }
        else if (buckets->array[ix].name == name) {
            /* someone else added the entry before us. */
            return;
        }
        ix = (ix == 0) ? capacity - 1 : ix - 1;
    }
}

PyObject *
_Py_mro_cache_as_dict(_Py_mro_cache *cache)
{
    PyObject *dict = PyDict_New();
    if (dict == NULL) {
        return NULL;
    }

    ASSERT_TYPE_LOCK_HELD();
    _Py_mro_cache_entry *entry = cache->buckets;
    Py_ssize_t capacity = capacity_from_mask(cache->mask);
    for (Py_ssize_t i = 0; i < capacity; i++, entry++) {
        if (entry->name) {
            PyObject *value = (PyObject *)(entry->value & ~1);
            if (value == NULL) {
                value = Py_None;
            }
            int err = PyDict_SetItem(dict, entry->name, value);
            if (err < 0) {
                Py_CLEAR(dict);
                return NULL;
            }
        }
    }

    return dict;
}

void
_Py_mro_cache_init_type(PyTypeObject *type)
{
    ASSERT_TYPE_LOCK_HELD();
    PyInterpreterState *interp = _PyInterpreterState_GET();
    _Py_mro_cache *cache;
    if (type->tp_flags & _Py_TPFLAGS_STATIC_BUILTIN) {
        cache = &_PyStaticType_GetState(interp, type)->tp_mro_cache;
    }
    else {
        cache = &type->tp_mro_cache;
    }
    if (cache->buckets == NULL) {
        cache->buckets = interp->mro_state.empty_buckets->array;
        interp->mro_state.empty_buckets->u.refcount++;
        cache->mask = mask_from_capacity(_Py_MRO_CACHE_MIN_SIZE);
    }
}

void
_Py_mro_cache_fini_type(PyTypeObject *type)
{
    _Py_mro_cache *cache;
    if (type->tp_flags & _Py_TPFLAGS_STATIC_BUILTIN) {
        PyInterpreterState *interp = _PyInterpreterState_GET();
        cache = &_PyStaticType_GetState(interp, type)->tp_mro_cache;
    }
    else {
        cache = &type->tp_mro_cache;
    }
    if (cache->buckets != NULL) {
        _Py_mro_cache_buckets *buckets = get_buckets(cache);
        cache->buckets = NULL;
        cache->mask = 0;
        clear_buckets(buckets);
    }
}

int
_Py_mro_cache_visit(_Py_mro_cache *cache, visitproc visit, void *arg)
{
    _Py_mro_cache_entry *entry = cache->buckets;
    if (entry == NULL) {
        return 0;
    }
    Py_ssize_t capacity = capacity_from_mask(cache->mask);
    for (Py_ssize_t i = 0; i < capacity; i++, entry++) {
        PyObject *value = (PyObject *)(entry->value & ~1);
        if (value) {
            int err = visit(value, arg);
            if (err != 0) {
                return err;
            }
        }
    }
    return 0;
}

PyStatus
_Py_mro_cache_init(PyInterpreterState *interp)
{
    _Py_mro_cache_buckets *b = allocate_empty_buckets(_Py_MRO_CACHE_MIN_SIZE);
    if (b == NULL) {
        return _PyStatus_NO_MEMORY();
    }
    interp->mro_state.empty_buckets = b;
    interp->mro_state.empty_buckets_capacity = _Py_MRO_CACHE_MIN_SIZE;
    return _PyStatus_OK();
}

void
_Py_mro_cache_fini(PyInterpreterState *interp)
{
    _Py_mro_cache_buckets *b = interp->mro_state.empty_buckets;
    if (b != NULL) {
        interp->mro_state.empty_buckets = NULL;
        interp->mro_state.empty_buckets_capacity = 0;
        decref_empty_bucket(b);
        _Py_mro_process_freed_buckets(interp);
    }
}
