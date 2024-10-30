# Reference counting in free threading CPython

The free threading build requires a thread-safe reference counting implementation.
Furthermore, it's important that the implementation has low execution overhead
and allows for efficient scaling with multiple threads. The free threading
build uses a combination of different reference counting techniques to address
these constraints:

* [Biased reference counting](#biased-reference-counting)
* [Immortal objects](#immortal-objects)
* [Deferred reference counting](#deferred-reference-counting)
* [Per-thread reference counting](#per-thread-reference-counting)


## `PyObject` representation

```c
struct _object {
    uintptr_t ob_tid;
    uint16_t _padding;
    PyMutex ob_mutex;           
    uint8_t ob_gc_bits;         
    uint32_t ob_ref_local;      
    Py_ssize_t ob_ref_shared;
    PyTypeObject *ob_type;
};
```


## Biased reference counting

## Immortal objects

Immortal objects use `ob_ref_local`

## Deferred reference counting

[../Include/internal/pycore_stackref.h](pycore_stackref.h)

```c
typedef union _PyStackRef {
    uintptr_t bits;
} _PyStackRef;
```



## Per-thread reference counting

* `_Py_INCREF_TYPE`
* `_Py_DECREF_TYPE`
* `_Py_INCREF_CODE`
* `_Py_DECREF_CODE`
* `_Py_INCREF_DICT`
* `_Py_DECREF_DICT`
* `_Py_INCREF_BUILTINS`
* `_Py_DECREF_BUILTINS`


# APIs

* 
