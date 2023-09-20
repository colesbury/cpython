#ifndef Py_ABI4_COMPAT_H
#  error "this header file must not be included directly"
#endif

#if defined(MS_WINDOWS)
// Windows does not support weak symbols, so we need to use GetProcAddress to
// find if the new APIs are available.
#include <libloaderapi.h>
static inline FARPROC
_Py_lookup_function(const char *name)
{
    HANDLE dll = GetModuleHandle(NULL);
    FARPROC proc = GetProcAddress(dll, name);
    if (proc != NULL) {
        return proc;
    }
    dll = GetModuleHandle("python3.dll");
    if (dll) {
        proc = GetProcAddress(dll, name);
        if (proc) {
            return proc;
        }
    }
    return NULL;
}
#define _Py_LOOKUP_FUNCTION(name) _Py_lookup_function(#name)
#else
// Other platforms support weak symbols.
PyAPI_FUNC(PyTypeObject *) Py_Type(PyObject *o) _Py_ATTRIBUTE_WEAK;
PyAPI_FUNC(void) Py_SetType(PyObject *o, PyTypeObject *type) _Py_ATTRIBUTE_WEAK;
PyAPI_FUNC(Py_ssize_t) Py_Refcnt(PyObject *o) _Py_ATTRIBUTE_WEAK;
PyAPI_FUNC(void) Py_SetRefcnt(PyObject *o, Py_ssize_t refcnt) _Py_ATTRIBUTE_WEAK;
PyAPI_FUNC(Py_ssize_t) PyVarObject_Size(PyObject *o) _Py_ATTRIBUTE_WEAK;
PyAPI_FUNC(void) PyVarObject_SetSize(PyObject *o, Py_ssize_t size) _Py_ATTRIBUTE_WEAK;
#define _Py_LOOKUP_FUNCTION(name) name
#endif

// Compatibility with Python versions pre-3.13
struct _object_abi3 {
    Py_ssize_t ob_refcnt;
    PyTypeObject *ob_type;
};

struct _varobject_abi3 {
    struct _object_abi3 ob_base;
    Py_ssize_t ob_size;
};

static inline PyTypeObject *
Py_Type_abi3(PyObject *ob)
{
    return ((struct _object_abi3 *)ob)->ob_type;
}

static inline Py_ssize_t
Py_Refcnt_abi3(PyObject *ob)
{
    return ((struct _object_abi3 *)ob)->ob_refcnt;
}

static inline PyTypeObject *
PyVarObject_Size_abi3(PyObject *ob)
{
    return ((struct _varobject_abi3 *)ob)->ob_size;
}

static inline void
Py_SetType_abi3(PyObject *ob, PyTypeObject *type)
{
    ((struct _object_abi3 *)ob)->ob_type = type;
}

static inline void
Py_SetRefcnt_abi3(PyObject *ob, Py_ssize_t refcnt)
{
    ((struct _object_abi3 *)ob)->ob_refcnt = refcnt;
}

static inline void
PyVarObject_SetSize_abi3(PyObject *ob, Py_ssize_t size)
{
    ((struct _varobject_abi3 *)ob)->ob_size = size;
}

#define _Py_ABI4_DISPATCH(name, return_type, signature, args)               \
static inline return_type name ## _dispatch(signature);                     \
static return_type (* _ ## name ## _ptr)(signature) = &name ## _dispatch;   \
static inline return_type                                                   \
name ## _dispatch(signature)                                                \
{                                                                           \
    typedef return_type (*func_ptr)(signature);                             \
    func_ptr func = (func_ptr)_Py_LOOKUP_FUNCTION(name);                    \
    if (func == NULL) {                                                     \
        func = &_ ## name ## _abi3;                                         \
    }                                                                       \
    _ ## name ## _ptr = func;                                               \
    return func(args);                                                      \
}

// handles expressions with commas
#define _Py_ARG(...) __VA_ARGS__

_Py_ABI4_DISPATCH(Py_Type, PyTypeObject*, PyObject* op, op);
_Py_ABI4_DISPATCH(Py_Refcnt, Py_ssize_t, PyObject*, (PyObject *op), op);
_Py_ABI4_DISPATCH(PyVarObject_Size, Py_ssize_t, PyObject*, (PyObject *op), op);
_Py_ABI4_DISPATCH(Py_SetType, void, _Py_ARG(PyObject *op, PyTypeObject *type), _Py_ARG(op, type));
_Py_ABI4_DISPATCH(Py_SetRefcnt, void, _Py_ARG(PyObject *op, Py_ssize_t size), _Py_ARG(op, size));
_Py_ABI4_DISPATCH(PyVarObject_SetSize, void, _Py_ARG(PyObject *op, Py_ssize_t size), _Py_ARG(op, size));
