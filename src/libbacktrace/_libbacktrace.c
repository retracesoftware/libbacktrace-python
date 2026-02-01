/*
 * _libbacktrace.c - Python C extension for libbacktrace
 * 
 * Provides Python bindings to libbacktrace for native stack traces
 * with DWARF symbol resolution.
 * 
 * SIGNAL HANDLER SAFETY:
 * ----------------------
 * The crash signal handler is designed to NOT depend on Python being
 * in an operable state. After enable_faulthandler() is called, crashes
 * will print stack traces using only:
 * 
 *   - Async-signal-safe syscalls: write(), open(), close(), getpid()
 *   - Pre-allocated libbacktrace state (created during enable_faulthandler)
 *   - Stack-allocated buffers (no malloc in signal handler)
 * 
 * Note: On macOS, libbacktrace itself uses malloc internally
 * (BACKTRACE_USES_MALLOC=1), which is technically not async-signal-safe.
 * However, since we pre-create the backtrace state before the crash,
 * this usually works in practice. On Linux, libbacktrace uses mmap
 * which is safer.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <backtrace.h>
#include <backtrace-supported.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

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

/*
 * Signal handler / faulthandler support
 */

static struct backtrace_state *signal_handler_state = NULL;
static int signal_handler_enabled = 0;
static char crash_report_path[512] = {0};

/* Callback for printing frames in signal handler (async-signal-safe) */
static int signal_print_callback(void *data, uintptr_t pc,
                                  const char *filename, int lineno,
                                  const char *function) {
    int fd = *(int *)data;
    char buf[512];
    int len;
    
    if (function && filename) {
        len = snprintf(buf, sizeof(buf), "  #%p %s at %s:%d\n", 
                       (void *)pc, function, filename, lineno);
    } else if (function) {
        len = snprintf(buf, sizeof(buf), "  #%p %s\n", (void *)pc, function);
    } else if (filename) {
        len = snprintf(buf, sizeof(buf), "  #%p ??? at %s:%d\n", 
                       (void *)pc, filename, lineno);
    } else {
        len = snprintf(buf, sizeof(buf), "  #%p ???\n", (void *)pc);
    }
    
    if (len > 0) {
        (void)write(fd, buf, (size_t)len);
    }
    return 0;
}

static void signal_error_callback(void *data, const char *msg, int errnum) {
    (void)data;
    (void)errnum;
    if (msg) {
        (void)write(STDERR_FILENO, "  [backtrace error: ", 20);
        (void)write(STDERR_FILENO, msg, strlen(msg));
        (void)write(STDERR_FILENO, "]\n", 2);
    }
}

/* Async-signal-safe signal name lookup (strsignal is NOT safe) */
static const char *safe_signame(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGFPE:  return "SIGFPE";
#ifdef SIGBUS
        case SIGBUS:  return "SIGBUS";
#endif
#ifdef SIGILL
        case SIGILL:  return "SIGILL";
#endif
#ifdef SIGTRAP
        case SIGTRAP: return "SIGTRAP";
#endif
#ifdef SIGSYS
        case SIGSYS:  return "SIGSYS";
#endif
        default:      return "UNKNOWN";
    }
}

static void write_crash_header(int fd, int sig) {
    const char *header = 
        "\n================================================================\n"
        "              NATIVE CRASH REPORT (libbacktrace)\n"
        "================================================================\n\n";
    (void)write(fd, header, strlen(header));
    
    char buf[128];
    /* Use safe_signame instead of strsignal (which is NOT async-signal-safe) */
    int len = snprintf(buf, sizeof(buf), "Signal: %d (%s)\nPID: %d\n\n",
                       sig, safe_signame(sig), getpid());
    if (len > 0) {
        (void)write(fd, buf, (size_t)len);
    }
    
    const char *trace_header = 
        "Native Stack Trace:\n"
        "------------------------------------------------------------\n";
    (void)write(fd, trace_header, strlen(trace_header));
}

static void write_crash_footer(int fd) {
    const char *footer = 
        "\n------------------------------------------------------------\n"
        "Tip: Enable Python's faulthandler for Python stack traces:\n"
        "     python -X faulthandler your_script.py\n"
        "================================================================\n\n";
    (void)write(fd, footer, strlen(footer));
}

static void crash_signal_handler(int sig) {
    /* Write to stderr */
    write_crash_header(STDERR_FILENO, sig);
    if (signal_handler_state) {
        int fd = STDERR_FILENO;
        backtrace_full(signal_handler_state, 2, signal_print_callback, 
                       signal_error_callback, &fd);
    } else {
        const char *msg = "  (backtrace state not initialized)\n";
        (void)write(STDERR_FILENO, msg, strlen(msg));
    }
    write_crash_footer(STDERR_FILENO);
    
    /* Also write to file if configured */
    if (crash_report_path[0]) {
        int fd = open(crash_report_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            write_crash_header(fd, sig);
            if (signal_handler_state) {
                backtrace_full(signal_handler_state, 2, signal_print_callback,
                               signal_error_callback, &fd);
            }
            write_crash_footer(fd);
            close(fd);
            
            const char *saved_msg = "Crash report saved to: ";
            (void)write(STDERR_FILENO, saved_msg, strlen(saved_msg));
            (void)write(STDERR_FILENO, crash_report_path, strlen(crash_report_path));
            (void)write(STDERR_FILENO, "\n", 1);
        }
    }
    
    /* Re-raise signal with default handler to get proper exit code */
    signal(sig, SIG_DFL);
    raise(sig);
}

/* Track which signals we've installed handlers for */
static int installed_signals[32] = {0};
static int num_installed_signals = 0;

/* Signal name to number mapping */
typedef struct {
    const char *name;
    int signum;
} signal_info_t;

static const signal_info_t known_signals[] = {
    {"SIGSEGV", SIGSEGV},
    {"SIGABRT", SIGABRT},
    {"SIGFPE", SIGFPE},
#ifdef SIGBUS
    {"SIGBUS", SIGBUS},
#endif
#ifdef SIGILL
    {"SIGILL", SIGILL},
#endif
#ifdef SIGTRAP
    {"SIGTRAP", SIGTRAP},
#endif
#ifdef SIGSYS
    {"SIGSYS", SIGSYS},
#endif
    {NULL, 0}
};

static int signal_name_to_num(const char *name) {
    for (int i = 0; known_signals[i].name != NULL; i++) {
        if (strcmp(known_signals[i].name, name) == 0) {
            return known_signals[i].signum;
        }
    }
    return -1;
}

static void install_signal_handler(int signum) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = crash_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;  /* One-shot: reset after firing */
    
    if (sigaction(signum, &sa, NULL) == 0) {
        if (num_installed_signals < 32) {
            installed_signals[num_installed_signals++] = signum;
        }
    }
}

static void uninstall_signal_handlers(void) {
    for (int i = 0; i < num_installed_signals; i++) {
        signal(installed_signals[i], SIG_DFL);
    }
    num_installed_signals = 0;
}

/* Default signals to handle */
static const char *default_signals[] = {"SIGSEGV", "SIGABRT", "SIGFPE", "SIGBUS", NULL};

/*
 * enable_faulthandler(signals=None, report_path=None) -> bool
 * 
 * Install signal handlers to print native stack traces on crash.
 * signals: list of signal names (default: SIGSEGV, SIGABRT, SIGFPE, SIGBUS)
 */
static PyObject *py_enable_faulthandler(PyObject *self, PyObject *args, PyObject *kwargs) {
    (void)self;
    static char *kwlist[] = {"signals", "report_path", NULL};
    PyObject *signals_obj = NULL;
    const char *report_path = NULL;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|Oz", kwlist, &signals_obj, &report_path)) {
        return NULL;
    }
    
    /* Disable any existing handlers first */
    uninstall_signal_handlers();
    
    /* Set report path if provided */
    if (report_path) {
        strncpy(crash_report_path, report_path, sizeof(crash_report_path) - 1);
        crash_report_path[sizeof(crash_report_path) - 1] = '\0';
    } else {
        crash_report_path[0] = '\0';
    }
    
    /* Initialize backtrace state if needed */
    if (!signal_handler_state) {
        signal_handler_state = backtrace_create_state(NULL, 1, error_callback, NULL);
    }
    
    /* Install handlers for specified signals */
    if (signals_obj == NULL || signals_obj == Py_None) {
        /* Use defaults */
        for (int i = 0; default_signals[i] != NULL; i++) {
            int signum = signal_name_to_num(default_signals[i]);
            if (signum > 0) {
                install_signal_handler(signum);
            }
        }
    } else {
        /* Use provided list */
        PyObject *iter = PyObject_GetIter(signals_obj);
        if (!iter) {
            PyErr_SetString(PyExc_TypeError, "signals must be iterable");
            return NULL;
        }
        
        PyObject *item;
        while ((item = PyIter_Next(iter)) != NULL) {
            const char *name = PyUnicode_AsUTF8(item);
            if (!name) {
                Py_DECREF(item);
                Py_DECREF(iter);
                PyErr_SetString(PyExc_TypeError, "signal names must be strings");
                return NULL;
            }
            
            int signum = signal_name_to_num(name);
            if (signum < 0) {
                Py_DECREF(item);
                Py_DECREF(iter);
                PyErr_Format(PyExc_ValueError, "unknown signal: %s", name);
                return NULL;
            }
            
            install_signal_handler(signum);
            Py_DECREF(item);
        }
        Py_DECREF(iter);
        
        if (PyErr_Occurred()) {
            return NULL;
        }
    }
    
    signal_handler_enabled = 1;
    
    Py_RETURN_TRUE;
}

/*
 * disable_faulthandler() -> bool
 * 
 * Remove signal handlers and restore defaults.
 */
static PyObject *py_disable_faulthandler(PyObject *self, PyObject *args) {
    (void)self;
    (void)args;
    
    uninstall_signal_handlers();
    signal_handler_enabled = 0;
    crash_report_path[0] = '\0';
    
    Py_RETURN_TRUE;
}

/*
 * faulthandler_enabled() -> bool
 * 
 * Check if faulthandler is currently enabled.
 */
static PyObject *py_faulthandler_enabled(PyObject *self, PyObject *args) {
    (void)self;
    (void)args;
    return PyBool_FromLong(signal_handler_enabled);
}

/*
 * get_signals() -> list
 * 
 * Get list of available signal names.
 */
static PyObject *py_get_signals(PyObject *self, PyObject *args) {
    (void)self;
    (void)args;
    
    /* Count signals */
    int count = 0;
    while (known_signals[count].name != NULL) count++;
    
    PyObject *result = PyList_New(count);
    if (!result) return NULL;
    
    for (int i = 0; i < count; i++) {
        PyObject *name = PyUnicode_FromString(known_signals[i].name);
        if (!name) {
            Py_DECREF(result);
            return NULL;
        }
        PyList_SET_ITEM(result, i, name);
    }
    
    return result;
}

/*
 * get_default_signals() -> list
 * 
 * Get list of default signal names used when none specified.
 */
static PyObject *py_get_default_signals(PyObject *self, PyObject *args) {
    (void)self;
    (void)args;
    
    /* Count defaults */
    int count = 0;
    while (default_signals[count] != NULL) count++;
    
    PyObject *result = PyList_New(count);
    if (!result) return NULL;
    
    for (int i = 0; i < count; i++) {
        PyObject *name = PyUnicode_FromString(default_signals[i]);
        if (!name) {
            Py_DECREF(result);
            return NULL;
        }
        PyList_SET_ITEM(result, i, name);
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
    {"enable_faulthandler", (PyCFunction)py_enable_faulthandler, 
     METH_VARARGS | METH_KEYWORDS,
     "Enable native crash handler.\n\n"
     "Installs signal handlers to print native stack traces on crash.\n\n"
     "Args:\n"
     "    signals: List of signal names (default: SIGSEGV, SIGABRT, SIGFPE, SIGBUS)\n"
     "    report_path: Optional file path to save crash reports\n\n"
     "Returns:\n"
     "    True on success"},
    {"disable_faulthandler", py_disable_faulthandler, METH_NOARGS,
     "Disable native crash handler and restore default signal handlers."},
    {"faulthandler_enabled", py_faulthandler_enabled, METH_NOARGS,
     "Check if native crash handler is currently enabled."},
    {"get_signals", py_get_signals, METH_NOARGS,
     "Get list of all available signal names."},
    {"get_default_signals", py_get_default_signals, METH_NOARGS,
     "Get list of default signals handled when none specified."},
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
