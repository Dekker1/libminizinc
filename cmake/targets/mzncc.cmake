#### Minizinc bytecode compiler
add_executable(mzncc mzncc.cpp lib/codegen.cpp include/minizinc/codegen.hh)
target_link_libraries(mzncc minizinc_solver)

install(
  TARGETS mzncc
  EXPORT libminizincTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
