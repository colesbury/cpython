#ifndef Py_INTERNAL_GENOBJECT_H
#define Py_INTERNAL_GENOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_frame.h"
#include "pycore_pyatomic_ft_wrappers.h"

#ifdef Py_GIL_DISABLED
# define _PY_GEN_OUT_FRAME_STATE(prefix) int8_t *prefix##_out_frame_state
#else
# define _PY_GEN_OUT_FRAME_STATE(prefix)
#endif

/* _PyGenObject_HEAD defines the initial segment of generator
   and coroutine objects. */
#define _PyGenObject_HEAD(prefix)                                           \
    PyObject_HEAD                                                           \
    /* List of weak reference. */                                           \
    PyObject *prefix##_weakreflist;                                         \
    /* Name of the generator. */                                            \
    PyObject *prefix##_name;                                                \
    /* Qualified name of the generator. */                                  \
    PyObject *prefix##_qualname;                                            \
    _PyErr_StackItem prefix##_exc_state;                                    \
    PyObject *prefix##_origin_or_finalizer;                                 \
    _PY_GEN_OUT_FRAME_STATE(prefix);                                        \
    char prefix##_hooks_inited;                                             \
    char prefix##_closed;                                                   \
    char prefix##_running_async;                                            \
    /* The frame */                                                         \
    int8_t prefix##_frame_state;                                            \
    struct _PyInterpreterFrame prefix##_iframe;                             \

struct _PyGenObject {
    /* The gi_ prefix is intended to remind of generator-iterator. */
    _PyGenObject_HEAD(gi)
};

struct _PyCoroObject {
    _PyGenObject_HEAD(cr)
};

struct _PyAsyncGenObject {
    _PyGenObject_HEAD(ag)
};

#undef _PyGenObject_HEAD

static inline
PyGenObject *_PyGen_GetGeneratorFromFrame(_PyInterpreterFrame *frame)
{
    assert(frame->owner == FRAME_OWNED_BY_GENERATOR);
    size_t offset_in_gen = offsetof(PyGenObject, gi_iframe);
    return (PyGenObject *)(((char *)frame) - offset_in_gen);
}

// Set the generator's frame state unconditionally.
static inline void
_PyGen_SetFrameState(PyGenObject *gen, int8_t state)
{
#ifdef Py_GIL_DISABLED
    if (gen->gi_out_frame_state) {
        *gen->gi_out_frame_state = state;
        gen->gi_out_frame_state = NULL;
    }
    _Py_atomic_store_int8_release(&gen->gi_frame_state, state);
#else
    gen->gi_frame_state = state;
#endif
}

// Try to set the frame state of the generator object if the current state is
// equal to 'from_state'. This is unconditional in the GIL-enabled build.
static inline int
_PyGen_TransitionFrameState(PyGenObject *gen, int8_t *from_state, int8_t state)
{
#ifdef Py_GIL_DISABLED
    return _Py_atomic_compare_exchange_int8(&gen->gi_frame_state, from_state, state);
#else
    assert(gen->gi_frame_state == *from_state);
    gen->gi_frame_state = state;
    return 1;
#endif
}

// Set the generator's frame state as `FRAME_EXECUTING` if the generator is
// not finished or executing. This behaves atomically in the free threading
// build.
static inline int
_PyGen_TrySetFrameStateExecuting(PyGenObject *gen)
{
    assert(PyGen_CheckExact(gen) || PyCoro_CheckExact(gen));
    int8_t frame_state = FT_ATOMIC_LOAD_INT8_RELAXED(gen->gi_frame_state);
    if (frame_state >= FRAME_EXECUTING) {
        return 0;
    }
    return _PyGen_TransitionFrameState(gen, &frame_state, FRAME_EXECUTING);
}

PyAPI_FUNC(PyObject *)_PyGen_yf(PyGenObject *);
extern void _PyGen_Finalize(PyObject *self);

// Export for '_asyncio' shared extension
PyAPI_FUNC(int) _PyGen_SetStopIterationValue(PyObject *);

// Export for '_asyncio' shared extension
PyAPI_FUNC(int) _PyGen_FetchStopIterationValue(PyObject **);

PyAPI_FUNC(PyObject *)_PyCoro_GetAwaitableIter(PyObject *o);
extern PyObject *_PyAsyncGenValueWrapperNew(PyThreadState *state, PyObject *);

extern PyTypeObject _PyCoroWrapper_Type;
extern PyTypeObject _PyAsyncGenWrappedValue_Type;
extern PyTypeObject _PyAsyncGenAThrow_Type;

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_GENOBJECT_H */
