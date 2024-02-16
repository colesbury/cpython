#include "Python.h"

#include "pycore_lock.h"
#include "pycore_critical_section.h"

static_assert(_Alignof(_PyCriticalSection) >= 4,
              "critical section must be aligned to at least 4 bytes");

// static Py_ssize_t slow_acquisitions = 0;

// __attribute__((destructor))
// static void print_slow_acquisitions(void)
// {
//     if (slow_acquisitions > 0) {
//         fprintf(stderr, "slow acquisitions: %zd\n", slow_acquisitions);
//     }
// }

#ifdef Py_CRITICAL_SECTION_DEBUG
static void
debug_slow_path(_PyCriticalSection *c, PyMutex *m);
#endif

void
_PyCriticalSection_BeginSlow(_PyCriticalSection *c, PyMutex *m)
{
#ifdef Py_CRITICAL_SECTION_DEBUG
    debug_slow_path(c, m);
#endif
    PyThreadState *tstate = _PyThreadState_GET();
    c->mutex = NULL;
    c->prev = (uintptr_t)tstate->critical_section;
    tstate->critical_section = (uintptr_t)c;

    _PyMutex_LockSlow(m);
    c->mutex = m;
}

void
_PyCriticalSection2_BeginSlow(_PyCriticalSection2 *c, PyMutex *m1, PyMutex *m2,
                              int is_m1_locked)
{
    PyThreadState *tstate = _PyThreadState_GET();
    c->base.mutex = NULL;
    c->mutex2 = NULL;
    c->base.prev = tstate->critical_section;
    tstate->critical_section = (uintptr_t)c | _Py_CRITICAL_SECTION_TWO_MUTEXES;

    if (!is_m1_locked) {
        PyMutex_Lock(m1);
    }
    PyMutex_Lock(m2);
    c->base.mutex = m1;
    c->mutex2 = m2;
}

static _PyCriticalSection *
untag_critical_section(uintptr_t tag)
{
    return (_PyCriticalSection *)(tag & ~_Py_CRITICAL_SECTION_MASK);
}

// Release all locks held by critical sections. This is called by
// _PyThreadState_Detach.
void
_PyCriticalSection_SuspendAll(PyThreadState *tstate)
{
    uintptr_t *tagptr = &tstate->critical_section;
    while (_PyCriticalSection_IsActive(*tagptr)) {
        _PyCriticalSection *c = untag_critical_section(*tagptr);

        if (c->mutex) {
            PyMutex_Unlock(c->mutex);
            if ((*tagptr & _Py_CRITICAL_SECTION_TWO_MUTEXES)) {
                _PyCriticalSection2 *c2 = (_PyCriticalSection2 *)c;
                if (c2->mutex2) {
                    PyMutex_Unlock(c2->mutex2);
                }
            }
        }

        *tagptr |= _Py_CRITICAL_SECTION_INACTIVE;
        tagptr = &c->prev;
    }
}

void
_PyCriticalSection_Resume(PyThreadState *tstate)
{
    uintptr_t p = tstate->critical_section;
    _PyCriticalSection *c = untag_critical_section(p);
    assert(!_PyCriticalSection_IsActive(p));

    PyMutex *m1 = c->mutex;
    c->mutex = NULL;

    PyMutex *m2 = NULL;
    _PyCriticalSection2 *c2 = NULL;
    if ((p & _Py_CRITICAL_SECTION_TWO_MUTEXES)) {
        c2 = (_PyCriticalSection2 *)c;
        m2 = c2->mutex2;
        c2->mutex2 = NULL;
    }

    if (m1) {
        PyMutex_Lock(m1);
    }
    if (m2) {
        PyMutex_Lock(m2);
    }

    c->mutex = m1;
    if (m2) {
        c2->mutex2 = m2;
    }

    tstate->critical_section &= ~_Py_CRITICAL_SECTION_INACTIVE;
}

#ifdef Py_CRITICAL_SECTION_DEBUG
static _PyCriticalSection *
find_critical_section(PyThreadState *tstate, PyMutex *m)
{
    uintptr_t tag = tstate->critical_section;
    while (_PyCriticalSection_IsActive(tag)) {
        _PyCriticalSection *c = untag_critical_section(tag);

        if (c->mutex == m) {
            return c;
        }
        else if (tag & _Py_CRITICAL_SECTION_TWO_MUTEXES) {
            _PyCriticalSection2 *c2 = (_PyCriticalSection2 *)c;
            if (c2->mutex2 == m) {
                return &c2->base;
            }
        }

        tag = c->prev;
    }
    return NULL;
}

#define MAX_REACQUISITIONS 100

struct reacquisition {
    const char *file1;
    const char *file2;
    int line1;
    int line2;
    Py_ssize_t counter;
};

struct _critical_section_stats {
    PyMutex mutex;
    uint64_t slow;
    uint64_t contended;
    uint64_t dropped;
    struct reacquisition reacquisitions[MAX_REACQUISITIONS];
};

static struct _critical_section_stats stats;

static int
reacquisition_cmp(const void *aa, const void *bb)
{
    struct reacquisition *a = (struct reacquisition *)aa;
    struct reacquisition *b = (struct reacquisition *)bb;
    return b->counter - a->counter;
}

__attribute__((destructor))
static void print_stats(void)
{
    FILE *out = stderr; 
    fprintf(out, "CriticalSection slow: %" PRIu64 " contended %" PRIu64 " dropped %" PRIu64 "\n",
            stats.slow, stats.contended, stats.dropped);

    qsort(stats.reacquisitions, MAX_REACQUISITIONS, sizeof(struct reacquisition),
          &reacquisition_cmp);

    for (int i = 0; i < MAX_REACQUISITIONS; i++) {
        struct reacquisition *r = &stats.reacquisitions[i];
        if (r->file1 == NULL) {
            break;
        }
        fprintf(out, "CriticalSection %4" PRIu64 " %40s:%-4d -> %s:%d\n",
                r->counter, r->file1, r->line1, r->file2, r->line2);
    }
}

static void
debug_slow_path(_PyCriticalSection *c, PyMutex *m)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyCriticalSection *original = find_critical_section(tstate, m);

    PyMutex_LockFlags(&stats.mutex, _Py_LOCK_DONT_DETACH);
    stats.slow++;
    if (original == NULL) {
        stats.contended++;
    }
    else {
        int i = 0;
        for (i = 0; i < MAX_REACQUISITIONS; i++) {
            struct reacquisition *r = &stats.reacquisitions[i];
            if (r->file1 == NULL) {
                r->file1 = original->filename;
                r->file2 = c->filename;
                r->line1 = original->lineno;
                r->line2 = c->lineno;
                r->counter = 1;
                break;
            }
            else if (r->file1 == original->filename &&
                     r->file2 == c->filename &&
                     r->line1 == original->lineno &&
                     r->line2 == c->lineno) {
                r->counter++;
                break;
            }
        }
        if (i == MAX_REACQUISITIONS) {
            stats.dropped++;
        }
    }
    PyMutex_Unlock(&stats.mutex);
}
#endif