/* Per-type MRO cache implementation for free-threaded builds.
 *
 * See Include/internal/pycore_mrocache.h for design overview.
 */

#include "Python.h"
#include "pycore_mrocache.h"
#include "pycore_pymem.h"       // _PyMem_FreeDelayed
#include "pycore_object.h"      // _Py_TryIncrefCompare

#ifdef Py_GIL_DISABLED

#define MRO_CACHE_INITIAL_SIZE  8
#define MRO_CACHE_MAX_SIZE      65536
#define MRO_CACHE_LOAD_FACTOR   3  /* resize at 2/3 full: used * 3 >= cap * 2 */

/* Shared empty singleton */
static _Py_mro_cache_entry _empty_entry = { NULL, NULL };

_Py_mro_cache_buckets _Py_mro_cache_empty_buckets = {
    .entries = &_empty_entry,
    .mask = 0,
    .used = 0,
};

static _Py_mro_cache_buckets *
alloc_buckets(uint32_t capacity)
{
    assert((capacity & (capacity - 1)) == 0);  /* power of two */
    assert(capacity >= MRO_CACHE_INITIAL_SIZE);

    _Py_mro_cache_buckets *buckets = PyMem_Malloc(sizeof(_Py_mro_cache_buckets));
    if (buckets == NULL) {
        return NULL;
    }

    _Py_mro_cache_entry *entries = PyMem_Calloc(capacity, sizeof(_Py_mro_cache_entry));
    if (entries == NULL) {
        PyMem_Free(buckets);
        return NULL;
    }

    buckets->entries = entries;
    buckets->mask = capacity - 1;
    buckets->used = 0;
    return buckets;
}

/* Insert into a bucket array without reference counting.
   Used during resize to re-insert existing entries. */
static void
raw_insert(_Py_mro_cache_entry *entries, uint32_t mask,
           PyObject *name, PyObject *value)
{
    uint32_t idx = ((uintptr_t)name >> 4) & mask;
    while (1) {
        if (entries[idx].name == NULL) {
            entries[idx].name = name;
            entries[idx].value = value;
            return;
        }
        idx = (idx + 1) & mask;
    }
}

static _Py_mro_cache_buckets *
resize_buckets(_Py_mro_cache_buckets *old, uint32_t new_capacity)
{
    _Py_mro_cache_buckets *new_buckets = alloc_buckets(new_capacity);
    if (new_buckets == NULL) {
        return NULL;
    }

    /* Re-insert all existing entries (move refs, no incref/decref) */
    uint32_t old_cap = old->mask + 1;
    _Py_mro_cache_entry *old_entries = old->entries;
    for (uint32_t i = 0; i < old_cap; i++) {
        if (old_entries[i].name != NULL) {
            raw_insert(new_buckets->entries, new_buckets->mask,
                       old_entries[i].name, old_entries[i].value);
            new_buckets->used++;
        }
    }

    return new_buckets;
}

void
_Py_mro_cache_init(PyTypeObject *type)
{
    _Py_atomic_store_ptr_release(&type->_tp_mro_cache,
                                 &_Py_mro_cache_empty_buckets);
}

void
_Py_mro_cache_fini(PyTypeObject *type)
{
    _Py_mro_cache_buckets *buckets =
        (_Py_mro_cache_buckets *)type->_tp_mro_cache;

    if (buckets == &_Py_mro_cache_empty_buckets) {
        type->_tp_mro_cache = &_Py_mro_cache_empty_buckets;
        return;
    }

    /* Type is unreachable — safe to free directly */
    uint32_t cap = buckets->mask + 1;
    _Py_mro_cache_entry *entries = buckets->entries;
    for (uint32_t i = 0; i < cap; i++) {
        if (entries[i].name != NULL) {
            Py_DECREF(entries[i].name);
            if (entries[i].value != _Py_MRO_CACHE_ABSENT) {
                Py_XDECREF(entries[i].value);
            }
        }
    }

    PyMem_Free(entries);
    PyMem_Free(buckets);
    type->_tp_mro_cache = &_Py_mro_cache_empty_buckets;
}

void
_Py_mro_cache_insert(PyTypeObject *type, PyObject *name, PyObject *value)
{
    /* Must be called under TYPE_LOCK */
    _Py_mro_cache_buckets *buckets =
        (_Py_mro_cache_buckets *)type->_tp_mro_cache;

    PyObject *store_value = (value != NULL) ? value : (PyObject *)_Py_MRO_CACHE_ABSENT;

    /* Check if name is already present (update in place) */
    if (buckets != &_Py_mro_cache_empty_buckets) {
        uint32_t mask = buckets->mask;
        _Py_mro_cache_entry *entries = buckets->entries;
        uint32_t idx = ((uintptr_t)name >> 4) & mask;

        for (uint32_t i = 0; i <= mask; i++) {
            _Py_mro_cache_entry *entry = &entries[idx];
            if (entry->name == name) {
                /* Update existing entry.
                   Write value first (relaxed), name is already set. */
                PyObject *old_value = entry->value;
                if (store_value != _Py_MRO_CACHE_ABSENT) {
                    Py_INCREF(store_value);
                }
                _Py_atomic_store_ptr_relaxed(&entry->value, store_value);
                if (old_value != _Py_MRO_CACHE_ABSENT && old_value != NULL) {
                    _PyObject_XDecRefDelayed(old_value);
                }
                return;
            }
            if (entry->name == NULL) {
                break;
            }
            idx = (idx + 1) & mask;
        }
    }

    /* Need to insert a new entry. Check if we need to allocate or resize. */
    if (buckets == &_Py_mro_cache_empty_buckets) {
        /* First insertion — allocate initial buckets */
        _Py_mro_cache_buckets *new_buckets = alloc_buckets(MRO_CACHE_INITIAL_SIZE);
        if (new_buckets == NULL) {
            return;  /* silently fail — cache is optional */
        }
        buckets = new_buckets;
        _Py_atomic_store_ptr_release(&type->_tp_mro_cache, buckets);
    }
    else {
        /* Check load factor: resize at 2/3 */
        uint32_t cap = buckets->mask + 1;
        if (buckets->used * 3 >= cap * 2 && cap < MRO_CACHE_MAX_SIZE) {
            uint32_t new_cap = cap * 2;
            _Py_mro_cache_buckets *new_buckets = resize_buckets(buckets, new_cap);
            if (new_buckets == NULL) {
                return;  /* silently fail */
            }

            _Py_mro_cache_buckets *old_buckets = buckets;
            buckets = new_buckets;

            /* Publish new buckets with release store */
            _Py_atomic_store_ptr_release(&type->_tp_mro_cache, buckets);

            /* QSBR-free old entries and bucket struct.
               Refs were moved (not copied), so no decrefs needed. */
            _PyMem_FreeDelayed(old_buckets->entries,
                               cap * sizeof(_Py_mro_cache_entry));
            _PyMem_FreeDelayed(old_buckets,
                               sizeof(_Py_mro_cache_buckets));
        }
    }

    /* Insert new entry */
    uint32_t mask = buckets->mask;
    _Py_mro_cache_entry *entries = buckets->entries;
    uint32_t idx = ((uintptr_t)name >> 4) & mask;

    while (entries[idx].name != NULL) {
        idx = (idx + 1) & mask;
    }

    /* Write ordering: value first (relaxed), then name (release).
       Readers who see name != NULL are guaranteed to see the value. */
    if (store_value != _Py_MRO_CACHE_ABSENT) {
        Py_INCREF(store_value);
    }
    Py_INCREF(name);
    _Py_atomic_store_ptr_relaxed(&entries[idx].value, store_value);
    _Py_atomic_store_ptr_release(&entries[idx].name, name);
    buckets->used++;
}

void
_Py_mro_cache_erase(PyTypeObject *type)
{
    /* Must be called under TYPE_LOCK */
    _Py_mro_cache_buckets *old_buckets =
        (_Py_mro_cache_buckets *)type->_tp_mro_cache;

    if (old_buckets == &_Py_mro_cache_empty_buckets) {
        return;
    }

    /* Swap to empty with release store */
    _Py_atomic_store_ptr_release(&type->_tp_mro_cache,
                                 &_Py_mro_cache_empty_buckets);

    /* Delayed-decref all cached names and values, then free the arrays.
       Readers may still be probing the old entries; QSBR ensures all
       reads complete before the actual free/decref happens. */
    uint32_t cap = old_buckets->mask + 1;
    _Py_mro_cache_entry *entries = old_buckets->entries;

    for (uint32_t i = 0; i < cap; i++) {
        if (entries[i].name != NULL) {
            _PyObject_XDecRefDelayed(entries[i].name);
            if (entries[i].value != _Py_MRO_CACHE_ABSENT) {
                _PyObject_XDecRefDelayed(entries[i].value);
            }
        }
    }

    _PyMem_FreeDelayed(entries, cap * sizeof(_Py_mro_cache_entry));
    _PyMem_FreeDelayed(old_buckets, sizeof(_Py_mro_cache_buckets));
}

#endif /* Py_GIL_DISABLED */
