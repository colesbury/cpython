#ifndef Py_INTERNAL_MODULEOBJECT_H
#define Py_INTERNAL_MODULEOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

extern void _PyModule_Clear(PyObject *);
extern void _PyModule_ClearDict(PyObject *);
extern int _PyModuleSpec_IsInitializing(PyObject *);

extern int _PyModule_IsExtension(PyObject *obj);

extern PyStatus _PyModule_InitGlobalObjects(PyInterpreterState *);
extern void _PyModule_Fini(PyInterpreterState *);

typedef struct {
    PyObject_HEAD
    PyObject *md_dict;
    PyModuleDef *md_def;
    void *md_state;
    PyObject *md_weaklist;
    // for logging purposes after md_dict is cleared
    PyObject *md_name;
} PyModuleObject;

#ifdef Py_NOGIL
// Wrapper around PyModule_Def for PyModuleDef_Init
typedef struct {
    PyObject_HEAD
    PyModuleDef *def;
} PyModuleDefObject;
#else
typedef PyModuleDef PyModuleDefObject;
#endif

static inline PyModuleDef* _PyModule_GetDef(PyObject *mod) {
    assert(PyModule_Check(mod));
    return ((PyModuleObject *)mod)->md_def;
}

static inline void* _PyModule_GetState(PyObject* mod) {
    assert(PyModule_Check(mod));
    return ((PyModuleObject *)mod)->md_state;
}

static inline PyObject* _PyModule_GetDict(PyObject *mod) {
    assert(PyModule_Check(mod));
    PyObject *dict = ((PyModuleObject *)mod) -> md_dict;
    // _PyModule_GetDict(mod) must not be used after calling module_clear(mod)
    assert(dict != NULL);
    return dict;  // borrowed reference
}

static inline PyModuleDef* _PyModuleDefObject_GetDef(PyObject *obj)
{
    assert(Py_IS_TYPE(obj, &PyModuleDef_Type));
#ifdef Py_NOGIL
    return ((PyModuleDefObject *)obj)->def;
#else
    return (PyModuleDef *)obj;
#endif
}

PyObject* _Py_module_getattro_impl(PyModuleObject *m, PyObject *name, int suppress);
PyObject* _Py_module_getattro(PyModuleObject *m, PyObject *name);

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_MODULEOBJECT_H */
