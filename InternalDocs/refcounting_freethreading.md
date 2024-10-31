# Reference counting in free threading CPython

The free threading build of CPython requires a thread-safe reference counting
implementation that has low execution overhead and allows for efficient scaling
with multiple threads. CPython uses a combination of different
reference counting techniques to address these constraints:

* [Biased reference counting](#biased-reference-counting)
* [Immortal objects](#immortal-objects)
* [Deferred reference counting](#deferred-reference-counting)
* [Per-thread reference counting](#per-thread-reference-counting)

In this context, "execution overhead" refers to the cost of a executing an operation on a single CPU.

## Biased reference counting

Biased reference counting [^brc] is a thread-safe reference counting technique with lower execution overhead than plain atomic reference counting. It is based on the observation that most objects are only accessed by a single thread, even in multi-threaded programs.

### `PyObject` representation

The free threading build uses a different `PyObject` structure to support biased reference counting and other free-threading features.

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

typedef struct _object PyObject;
```


#### `ob_tid`

The `ob_tid` field typically stores the id of the owning thread as determined by `_Py_ThreadId()`. It is zero if the object is not owned by any thread. The field is also used by the GC to temporarily maintain a linked list of objects.

`_Py_ThreadId()` is a fast, platform and OS-specific function that returns a unique identifier for the current thread. CPython's use of `ob_tid` places a few restrictions on the implementation of `_Py_ThreadId()`:

* `0` must not be a valid thread id
* Thread IDs must be distinct from CPython maintained pointers

Typically `_Py_ThreadId()` is a pointer to some thread-local data, like Window's [Thread Information Block](https://en.wikipedia.org/wiki/Win32_Thread_Information_Block) or GNU C's thread control block. The slower, cross-platform fallback implementation returns a pointer to some thread-local data.

#### `ob_ref_local`

The `ob_ref_local` field stores the owning thread's local reference count. Immortal objects have `ob_ref_local = UINT32_MAX`. Objects not owned by any thread have a local reference count of zero.

#### `ob_ref_shared`

The `ob_ref_shared` packs the the reference count for all non-owning threads along with two bits of state in the least significant bits. Consequently,  `ob_ref_shared` should be shifted right by two to extract the shared reference count. Note that `ob_ref_shared` is signed and may be negative, although the combined toal reference count with `ob_ref_local` must not be negative.

If `ob_ref_shared` becomes negative, the object is placed in a queue for the owning thread to merge. See [Python/brc.c](../Python/brc.c) for the implementation.

The possible reference counting states are:

* `0b00` - `_Py_REF_SHARED_INIT`: The initial state allows for fast deallocation without atomic operations in the common case where `ob_ref_shared` and `ob_ref_local` are zero.
* `0b01` - `_Py_REF_MAYBE_WEAKREF`: Forces the slower deallocation code path that uses an atomic compare-exchange to transition the object to `_Py_REF_MERGED` before deallocation.
* `0b10` - `_Py_REF_QUEUED`: An intermediate state that marks the object as being enqueued to be merged (but not yet merged).
* `0b11` - `_Py_REF_MERGED`: The obect is not owned by any thread and `ob_ref_shared` alone stores the reference count.

The motivation for `_Py_REF_MAYBE_WEAKREF` and the slower, atomic deallocation code path is to support `_Py_TryIncref(PyObject *)`, which tries to increment the reference count of an object that might have it's last reference concurrently dropped and be deallocated. The canonical use case is to support weak references, hence the name, but it's also used by the non-locking code path for `dict` and `list`. `_Py_TryIncref()` will not succeed for a non-owning thread if the `ob_ref_shared` is zero and in the `_Py_REF_SHARED_INIT` state for thread-safety reasons, so creating a weak reference will transition an object from `_Py_REF_SHARED_INIT` to `_Py_REF_MAYBE_WEAKREF`.

#### `ob_gc_bits`

The `ob_gc_bits` bit field primarily stores GC related state, but it also identifies objects that support deferred reference counting by the `_PyGC_BITS_DEFERRED` bit.


## Immortal objects

Both the default and free threading builds support immortal objects, but they
used different internal representations. In the free threading build, immortal
objects are represented by `ob_ref_local = UINT32_MAX`.

The following internal-only APIs are related to immortal objects:

* `_Py_IsImmortal(PyObject *)` - tests if an object is immortal
* `_Py_SetImmortal(PyObject *)` - marks an object as immortal

Additionally the `PyObject_HEAD_INIT(type)` macro initializes the object as immortal. This macro is used for statically initialized objects.

Marking an object as immortal means that it is not deallocated, except in some cases at interpreter shutdown. Consequently, immortalizing heap objects effectively "leaks" those objects.

In addition to statically allocated objects, free-threaded CPython also immortalizes constants objects in code objects' `co_consts` fields to avoid scaling bottlenecks when using numeric or string literals (or tuples of literals).


## Deferred reference counting

[../Include/internal/pycore_stackref.h](pycore_stackref.h)

```c
typedef union _PyStackRef {
    uintptr_t bits;
} _PyStackRef;
```

* `PyStackRef_DUP()`
* `PyStackRef_CLOSE()`
* `PyStackRef_FromPyObjectSteal(PyObject *obj)`
* `PyStackRef_FromPyObjectNew(PyObject *obj)`
* `PyStackRef_AsPyObjectBorrow(_PyStackRef stackref)`
* `PyStackRef_AsPyObjectSteal(_PyStackRef stackref)`

* `PyStackRef_AsStrongReference()`


## Per-thread reference counting

For some types of objects, deferred reference counting is not sufficient to address multi-threaded scaling bottlenecks because the references are on the heap, not the interpreter's evaluation stack. These include the references from instances to their type objects and from functions to their code, globals, and builtins objects.



* `_Py_INCREF_TYPE(PyTypeObject *type)`
* `_Py_DECREF_TYPE(PyTypeObject *type)`
* `_Py_INCREF_CODE(PyCodeObject *code)`
* `_Py_DECREF_CODE(PyCodeObject *code)`
* `_Py_INCREF_DICT(PyObject *dict)`
* `_Py_DECREF_DICT(PyObject *dict)`
* `_Py_INCREF_BUILTINS(PyObject *builtins)`
* `_Py_DECREF_BUILTINS(PyObject *builtins)`


# APIs

* 

[^brc]: “Biased reference counting: minimizing atomic operations in garbage collection”. Jiho Choi, Thomas Shull, and Josep Torrellas. PACT 2018. https://dl.acm.org/doi/abs/10.1145/3243176.3243195.
