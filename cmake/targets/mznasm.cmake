#### MiniZinc assembler / interpreter Binary Target

add_executable(mznasm mznasm.cpp)
target_link_libraries(mznasm mzn)

install(
  TARGETS mznasm
  EXPORT libminizincTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
