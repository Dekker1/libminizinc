import contextlib
import json
import sys
from ctypes.util import find_library

import cffi

DEBUG = False


def debugprint(*args):
    if DEBUG:
        print(*args, file=sys.stderr, flush=True)


ffi = cffi.FFI()
ffi.cdef(
    """
    struct _MZNInstance;
    typedef struct _MZNInstance* MZNInstance;

    MZNInstance minizinc_instance_init(const char* mza_file, const char* solver);
    void minizinc_instance_destroy(MZNInstance);

    void minizinc_add_call(MZNInstance, const char* call, ...);

    void minizinc_push_state(MZNInstance);
    void minizinc_pop_state(MZNInstance);

    const char* minizinc_solve(MZNInstance);
    """
)
# Set LD_LIBRARY_PATH (DYLD_LIBRARY_PATH on macOS) to the folder containing the mza library.
lib = ffi.dlopen("mza")


class Instance:
    def __init__(self, mza_file, solver):
        debugprint(
            f'MZNInstance inst = minizinc_instance_init("{mza_file}", "{solver}");'
        )
        self._ptr = lib.minizinc_instance_init(mza_file.encode(), solver.encode())

    def __del__(self):
        debugprint(f"minizinc_instance_destroy(inst);")
        lib.minizinc_instance_destroy(self._ptr)
        self._ptr = None

    @contextlib.contextmanager
    def branch(self):
        try:
            debugprint(f"minizinc_push_state(inst);")
            lib.minizinc_push_state(self._ptr)
            yield self
        finally:
            debugprint(f"minizinc_pop_state(inst);")
            lib.minizinc_pop_state(self._ptr)

    def add_call(self, call: str, *args):
        debugprint(
            f"minizinc_add_call(inst, \"{call}\"{''.join([', '+ str(i) for i in args])});"
        )
        lib.minizinc_add_call(
            self._ptr, call.encode(), *[ffi.cast("int", i) for i in args]
        )

    def solve(self):
        debugprint(f"minizinc_solve(inst);")
        res = ffi.string(lib.minizinc_solve(self._ptr))
        tmp = json.loads(res)
        return tmp["status"], tmp["solution"]
