#ifndef Py_INTERNAL_GENOBJECT_H
#define Py_INTERNAL_GENOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_frame.h"
#include "pycore_typedefs.h"      // _PyInterpreterFrame


#ifdef Py_GIL_DISABLED
# define _PY_GEN_OUT_FRAME_STATE(prefix) int8_t *prefix##_out_frame_state;
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
    _PY_GEN_OUT_FRAME_STATE(prefix)                                         \
    char prefix##_hooks_inited;                                             \
    char prefix##_closed;                                                   \
    char prefix##_running_async;                                            \
    /* The frame */                                                         \
    int8_t prefix##_frame_state;                                            \
    _PyInterpreterFrame prefix##_iframe;                                    \

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

static inline void
_PyGen_SetFrameState(PyGenObject *gen, int8_t state)
{
#ifdef Py_GIL_DISABLED
    if (gen->gi_out_frame_state) {
        *gen->gi_out_frame_state = state;
        gen->gi_out_frame_state = NULL;
    }
    _Py_atomic_store_int8(&gen->gi_frame_state, state);
#else
    gen->gi_frame_state = state;
#endif
}

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
