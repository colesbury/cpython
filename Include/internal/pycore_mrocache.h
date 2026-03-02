/* Per-type MRO cache for free-threaded builds.
 *
 * Supplements the global type cache with a per-type open-addressed hash table
 * that maps interned attribute names to resolved values. Lock-free reads via
 * acquire/relaxed atomics; writes under TYPE_LOCK. Old bucket arrays freed
 * via QSBR (_PyMem_FreeDelayed / _PyObject_XDecRefDelayed).
 */

#ifndef Py_INTERNAL_MROCACHE_H
#define Py_INTERNAL_MROCACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#ifdef Py_GIL_DISABLED

#include "pycore_object.h"       // _Py_TryIncrefCompare
#include "pycore_pymem.h"        // _PyMem_FreeDelayed

/* Sentinel for negative cache entries (attribute confirmed absent) */
#define _Py_MRO_CACHE_ABSENT ((PyObject *)(uintptr_t)1)

typedef struct {
    PyObject *name;    /* interned string (identity-compared), or NULL = empty */
    PyObject *value;   /* strong ref, or _Py_MRO_CACHE_ABSENT, or NULL = empty */
} _Py_mro_cache_entry;

typedef struct {
    _Py_mro_cache_entry *entries;  /* power-of-two sized array */
    uint32_t mask;                  /* capacity - 1 */
    uint32_t used;                  /* occupied slot count */
} _Py_mro_cache_buckets;

/* Shared empty singleton — avoids NULL checks on the hot path */
extern _Py_mro_cache_buckets _Py_mro_cache_empty_buckets;

/* --- API (all require TYPE_LOCK except lookup) --- */

/* Initialize type->_tp_mro_cache to empty. Called from type_ready(). */
extern void _Py_mro_cache_init(PyTypeObject *type);

/* Free all entries and the bucket array. Called from type_dealloc() /
   fini_static_type(). Type must be unreachable to other threads. */
extern void _Py_mro_cache_fini(PyTypeObject *type);

/* Insert or update an entry. Called under TYPE_LOCK after find_name_in_mro().
   |value| may be NULL (stored as _Py_MRO_CACHE_ABSENT for negative cache). */
extern void _Py_mro_cache_insert(PyTypeObject *type,
                                  PyObject *name, PyObject *value);

/* Erase all entries (swap to empty buckets, QSBR-free old ones).
   Called under TYPE_LOCK from type_modified_unlocked(). */
extern void _Py_mro_cache_erase(PyTypeObject *type);

/* --- Inline hot-path lookup (lock-free) --- */

/* Try to look up |name| in type's per-type MRO cache.
 *
 * Returns 1 on hit:  *result is a new strong reference, or NULL for
 *                     negative cache (_Py_MRO_CACHE_ABSENT).
 * Returns 0 on miss: caller should fall through to global cache / MRO walk.
 */
static inline int
_Py_mro_cache_lookup(void *cache_ptr, PyObject *name, PyObject **result)
{
    /* cache_ptr is already loaded via acquire by the calling macro
       (_Py_mro_cache_lookup_acq).  NULL means the type hasn't been
       through type_ready yet — treat as miss. */
    _Py_mro_cache_buckets *buckets = (_Py_mro_cache_buckets *)cache_ptr;
    if (buckets == NULL) {
        return 0;
    }

    uint32_t mask = buckets->mask;
    if (mask == 0) {
        return 0;  /* empty singleton */
    }

    _Py_mro_cache_entry *entries = buckets->entries;
    uint32_t idx = ((uintptr_t)name >> 4) & mask;

    for (uint32_t i = 0; i <= mask; i++) {
        _Py_mro_cache_entry *entry = &entries[idx];
        PyObject *entry_name = _Py_atomic_load_ptr_acquire(&entry->name);

        if (entry_name == name) {
            /* Identity match on interned name */
            PyObject *value = _Py_atomic_load_ptr_acquire(&entry->value);
            if (value == _Py_MRO_CACHE_ABSENT) {
                *result = NULL;
                return 1;  /* negative hit */
            }
            if (_Py_TryIncrefCompare(&entry->value, value)) {
                *result = value;
                return 1;  /* positive hit */
            }
            /* Couldn't incref — entry is being modified, treat as miss */
            return 0;
        }

        if (entry_name == NULL) {
            return 0;  /* empty slot — name is not cached */
        }

        idx = (idx + 1) & mask;
    }

    return 0;  /* table full (shouldn't happen) */
}

/* Macro that performs the acquire load of the cache pointer for the caller */
#define _Py_mro_cache_lookup_acq(type, name, result) \
    _Py_mro_cache_lookup( \
        _Py_atomic_load_ptr_acquire(&(type)->_tp_mro_cache), \
        (name), (result))

#endif /* Py_GIL_DISABLED */

#ifdef __cplusplus
}
#endif

#endif /* !Py_INTERNAL_MROCACHE_H */
