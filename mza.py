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

    void set_rnd_seed(int seed);

    MZNInstance minizinc_instance_init(const char* mza_file, const char* data_file, const char* solver);
    void minizinc_instance_destroy(MZNInstance);

    void minizinc_add_call(MZNInstance, const char* call, ...);
    void minizinc_set_solution(MZNInstance, int def, int sol);
    void minizinc_output_dict(MZNInstance, bool);
    void minizinc_print_hedge(MZNInstance);
    void minizinc_set_limit(MZNInstance, int limit);

    void minizinc_push_state(MZNInstance);
    void minizinc_pop_state(MZNInstance);

    const char* minizinc_solve(MZNInstance);
    """
)
# Set LD_LIBRARY_PATH (DYLD_LIBRARY_PATH on macOS) to the folder containing the mza library.
lib = ffi.dlopen("mza")


def set_rnd_seed(seed: int):
    debugprint(f"set_rnd_seed({seed});")
    lib.set_rnd_seed(seed)


class Instance:
    def __init__(self, mza_file, data_file, solver):
        debugprint(
            f'MZNInstance inst = minizinc_instance_init("{mza_file}", "{data_file}", "{solver}");'
        )
        self._ptr = lib.minizinc_instance_init(
            mza_file.encode(), data_file.encode(), solver.encode()
        )

    def __del__(self):
        debugprint(f"minizinc_instance_destroy(inst);")
        lib.minizinc_instance_destroy(self._ptr)
        self._ptr = None

    def output_dict(self, b: bool):
        debugprint(f"minizinc_output_dict(inst, {int(b)});")
        lib.minizinc_output_dict(self._ptr, b)

    def set_incumbent(self, sol):
        for k, v in sol.items():
            debugprint(f"minizinc_set_solution(inst, {k}, {v});")
            lib.minizinc_set_solution(self._ptr, int(k), v)

    def set_limit(self, limit):
        debugprint(f"minizinc_set_limit(inst, {limit});")
        lib.minizinc_set_limit(self._ptr, limit)

    def print(self):
        debugprint(f"minizinc_print_hedge(inst);")
        lib.minizinc_print_hedge(self._ptr)

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
