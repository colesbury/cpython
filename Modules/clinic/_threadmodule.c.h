/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"          // PyGC_Head
#  include "pycore_runtime.h"     // _Py_ID()
#endif
#include "pycore_modsupport.h"    // _PyArg_UnpackKeywords()

PyDoc_STRVAR(_thread_Event__at_fork_reinit__doc__,
"_at_fork_reinit($self, /)\n"
"--\n"
"\n");

#define _THREAD_EVENT__AT_FORK_REINIT_METHODDEF    \
    {"_at_fork_reinit", (PyCFunction)_thread_Event__at_fork_reinit, METH_NOARGS, _thread_Event__at_fork_reinit__doc__},

static PyObject *
_thread_Event__at_fork_reinit_impl(eventobject *self);

static PyObject *
_thread_Event__at_fork_reinit(eventobject *self, PyObject *Py_UNUSED(ignored))
{
    return _thread_Event__at_fork_reinit_impl(self);
}

PyDoc_STRVAR(_thread_Event_is_set__doc__,
"is_set($self, /)\n"
"--\n"
"\n"
"Return true if and only if the internal flag is true.");

#define _THREAD_EVENT_IS_SET_METHODDEF    \
    {"is_set", (PyCFunction)_thread_Event_is_set, METH_NOARGS, _thread_Event_is_set__doc__},

static int
_thread_Event_is_set_impl(eventobject *self);

static PyObject *
_thread_Event_is_set(eventobject *self, PyObject *Py_UNUSED(ignored))
{
    PyObject *return_value = NULL;
    int _return_value;

    _return_value = _thread_Event_is_set_impl(self);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    return return_value;
}

PyDoc_STRVAR(_thread_Event_set__doc__,
"set($self, /)\n"
"--\n"
"\n"
"Set the internal flag to true.\n"
"\n"
"All threads waiting for it to become true are awakened. Threads\n"
"that call wait() once the flag is true will not block at all.");

#define _THREAD_EVENT_SET_METHODDEF    \
    {"set", (PyCFunction)_thread_Event_set, METH_NOARGS, _thread_Event_set__doc__},

static PyObject *
_thread_Event_set_impl(eventobject *self);

static PyObject *
_thread_Event_set(eventobject *self, PyObject *Py_UNUSED(ignored))
{
    return _thread_Event_set_impl(self);
}

PyDoc_STRVAR(_thread_Event_clear__doc__,
"clear($self, /)\n"
"--\n"
"\n"
"Reset the internal flag to false.\n"
"\n"
"Subsequently, threads calling wait() will block until set() is called to\n"
"set the internal flag to true again.");

#define _THREAD_EVENT_CLEAR_METHODDEF    \
    {"clear", (PyCFunction)_thread_Event_clear, METH_NOARGS, _thread_Event_clear__doc__},

static PyObject *
_thread_Event_clear_impl(eventobject *self);

static PyObject *
_thread_Event_clear(eventobject *self, PyObject *Py_UNUSED(ignored))
{
    return _thread_Event_clear_impl(self);
}

PyDoc_STRVAR(_thread_Event_wait__doc__,
"wait($self, /, timeout=None)\n"
"--\n"
"\n"
"Block until the internal flag is true.\n"
"\n"
"If the internal flag is true on entry, return immediately. Otherwise,\n"
"block until another thread calls set() to set the flag to true, or until\n"
"the optional timeout occurs.\n"
"\n"
"When the timeout argument is present and not None, it should be a\n"
"floating point number specifying a timeout for the operation in seconds\n"
"(or fractions thereof).\n"
"\n"
"This method returns the internal flag on exit, so it will always return\n"
"True except if a timeout is given and the operation times out.");

#define _THREAD_EVENT_WAIT_METHODDEF    \
    {"wait", _PyCFunction_CAST(_thread_Event_wait), METH_FASTCALL|METH_KEYWORDS, _thread_Event_wait__doc__},

static int
_thread_Event_wait_impl(eventobject *self, PyObject *timeout);

static PyObject *
_thread_Event_wait(eventobject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_item = { &_Py_ID(timeout), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"timeout", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "wait",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 0;
    PyObject *timeout = Py_None;
    int _return_value;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 0, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    timeout = args[0];
skip_optional_pos:
    _return_value = _thread_Event_wait_impl(self, timeout);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    return return_value;
}
/*[clinic end generated code: output=5f9ba867fb3ce887 input=a9049054013a1b77]*/
