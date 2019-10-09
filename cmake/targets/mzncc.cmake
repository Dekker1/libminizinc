#### Minizinc bytecode compiler
add_executable(mzncc
  mzncc.cpp
  lib/codegen.cpp
  lib/codegen/codegen_internal.hpp
  lib/codegen/analysis.hpp
  lib/codegen/analysis.cpp
  include/minizinc/codegen.hh
  include/minizinc/codegen_support.hh)
target_link_libraries(mzncc mzn)

install(
  TARGETS mzncc
  EXPORT libminizincTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
