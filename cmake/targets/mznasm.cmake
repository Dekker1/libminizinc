#### MiniZinc assembler / interpreter Binary Target

add_executable(mznasm mznasm.cpp)
target_link_libraries(mznasm minizinc_solver)

install(
  TARGETS mznasm
  EXPORT libminizincTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
