import cffi
import os

from ctypes.util import find_library

ffi = cffi.FFI()
ffi.cdef(
    """
    struct _MZNInstance;
    typedef struct _MZNInstance* MZNInstance;

    MZNInstance minizinc_instance_init(const char* mza_file, const char* solver);

    void minizinc_add_call(MZNInstance, int call);

    void minizinc_push_state(MZNInstance);
    void minizinc_pop_state(MZNInstance);

    const char* minizinc_solve(MZNInstance);
    """
)
# Set LD_LIBRARY_PATH (DYLD_LIBRARY_PATH on macOS) to the folder containing the mza library.
lib = ffi.dlopen("mza")

inst = lib.minizinc_instance_init("temp.mza".encode(), "geas".encode())
res = lib.minizinc_solve(inst)
print(ffi.string(res).decode())
