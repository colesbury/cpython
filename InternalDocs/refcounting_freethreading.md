# Reference counting in free threading CPython

The free threading requires a thread-safe reference counting implementation.
Furthermore, it's important that the implementation has low execution overhead
and allows for efficient scaling with multiple threads. The free threading
build uses a combination of different reference counting techniques to address
these constraints.

* [Biased reference counting](#biased-reference-counting)

* Linear performance (overhead)
* Multithreaded scaling
* 

## Biased reference counting

## Immortal objects

## Deferred reference counting

[../Include/internal/pycore_stackref.h](pycore_stackref.h)



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