add_library(mza SHARED
  lib/c_interface.cpp
  include/minizinc/c_interface.h
)

target_link_libraries(mza mzn)