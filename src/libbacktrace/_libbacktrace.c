/*
 * _libbacktrace.c - Python C extension for libbacktrace
 * 
 * Provides Python bindings to libbacktrace for native stack traces
 * with DWARF symbol resolution.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <backtrace.h>
#include <backtrace-supported.h>

/* Maximum frames to capture */
#define MAX_FRAMES 128

/* Frame data collected during backtrace */
typedef struct {
    uintptr_t pc;
    const char *function;
    const char *filename;
    int lineno;
} frame_data_t;

/* Context for backtrace callback */
typedef struct {
    frame_data_t frames[MAX_FRAMES];
    int count;
    int skip;
} backtrace_context_t;

/* Callback for each frame with full symbol info */
static int full_callback(void *data, uintptr_t pc,
                         const char *filename, int lineno,
                         const char *function) {
    backtrace_context_t *ctx = (backtrace_context_t *)data;
    
    /* Skip requested frames */
    if (ctx->skip > 0) {
        ctx->skip--;
        return 0;
    }
    
    if (ctx->count >= MAX_FRAMES) {
        return 1;  /* Stop iteration */
    }
    
    frame_data_t *frame = &ctx->frames[ctx->count++];
    frame->pc = pc;
    frame->function = function;
    frame->filename = filename;
    frame->lineno = lineno;
    
    return 0;
}

/* Error callback */
static void error_callback(void *data, const char *msg, int errnum) {
    (void)data;
    (void)msg;
    (void)errnum;
    /* Silently ignore errors - we just won't have symbols */
}

/* Python object wrapping backtrace_state */
typedef struct {
    PyObject_HEAD
    struct backtrace_state *state;
} StateObject;

static void State_dealloc(StateObject *self) {
    /* Note: libbacktrace doesn't provide a way to free state */
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyTypeObject StateType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libbacktrace.State",
    .tp_doc = "Backtrace state object",
    .tp_basicsize = sizeof(StateObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)State_dealloc,
};

/*
 * create_state(filename=None, threaded=True) -> State
 * 
 * Create a new backtrace state for the given executable.
 */
static PyObject *create_state(PyObject *self, PyObject *args) {
    (void)self;
    const char *filename = NULL;
    int threaded = 1;
    
    if (!PyArg_ParseTuple(args, "|zp", &filename, &threaded)) {
        return NULL;
    }
    
    StateObject *state_obj = PyObject_New(StateObject, &StateType);
    if (!state_obj) {
        return NULL;
    }
    
    state_obj->state = backtrace_create_state(filename, threaded, error_callback, NULL);
    if (!state_obj->state) {
        Py_DECREF(state_obj);
        PyErr_SetString(PyExc_RuntimeError, "Failed to create backtrace state");
        return NULL;
    }
    
    return (PyObject *)state_obj;
}

/*
 * backtrace_full(state, skip=0) -> list of (pc, function, filename, lineno)
 * 
 * Get a full backtrace with symbol information.
 */
static PyObject *py_backtrace_full(PyObject *self, PyObject *args) {
    (void)self;
    StateObject *state_obj;
    int skip = 0;
    
    if (!PyArg_ParseTuple(args, "O!|i", &StateType, &state_obj, &skip)) {
        return NULL;
    }
    
    backtrace_context_t ctx = {0};
    ctx.skip = skip + 1;  /* Skip this function */
    
    backtrace_full(state_obj->state, 0, full_callback, error_callback, &ctx);
    
    /* Build Python list of tuples */
    PyObject *result = PyList_New(ctx.count);
    if (!result) {
        return NULL;
    }
    
    for (int i = 0; i < ctx.count; i++) {
        frame_data_t *f = &ctx.frames[i];
        PyObject *tuple = Py_BuildValue(
            "(kzzl)",
            (unsigned long)f->pc,
            f->function,
            f->filename,
            (long)f->lineno
        );
        if (!tuple) {
            Py_DECREF(result);
            return NULL;
        }
        PyList_SET_ITEM(result, i, tuple);
    }
    
    return result;
}

/* Module methods */
static PyMethodDef module_methods[] = {
    {"create_state", create_state, METH_VARARGS,
     "Create a new backtrace state.\n\n"
     "Args:\n"
     "    filename: Path to executable (None for current process)\n"
     "    threaded: Whether to support multi-threaded access\n\n"
     "Returns:\n"
     "    State object"},
    {"backtrace_full", py_backtrace_full, METH_VARARGS,
     "Get a full backtrace with symbol information.\n\n"
     "Args:\n"
     "    state: State object from create_state()\n"
     "    skip: Number of frames to skip\n\n"
     "Returns:\n"
     "    List of (pc, function, filename, lineno) tuples"},
    {NULL, NULL, 0, NULL}
};

/* Module definition */
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_libbacktrace",
    "Native bindings for libbacktrace",
    -1,
    module_methods
};

PyMODINIT_FUNC PyInit__libbacktrace(void) {
    /* Initialize State type */
    if (PyType_Ready(&StateType) < 0) {
        return NULL;
    }
    
    PyObject *module = PyModule_Create(&moduledef);
    if (!module) {
        return NULL;
    }
    
    /* Add constants */
    PyModule_AddIntConstant(module, "BACKTRACE_SUPPORTED", BACKTRACE_SUPPORTED);
    PyModule_AddIntConstant(module, "BACKTRACE_USES_MALLOC", BACKTRACE_USES_MALLOC);
    PyModule_AddIntConstant(module, "BACKTRACE_SUPPORTS_THREADS", BACKTRACE_SUPPORTS_THREADS);
    PyModule_AddIntConstant(module, "BACKTRACE_SUPPORTS_DATA", BACKTRACE_SUPPORTS_DATA);
    
    /* Add State type */
    Py_INCREF(&StateType);
    if (PyModule_AddObject(module, "State", (PyObject *)&StateType) < 0) {
        Py_DECREF(&StateType);
        Py_DECREF(module);
        return NULL;
    }
    
    return module;
}
