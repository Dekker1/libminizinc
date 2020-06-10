 FIND_PROGRAM(DTRACE dtrace)
 MARK_AS_ADVANCED(DTRACE)

set(DTRACE_HEADER "")
set(DTRACE_OBJECT "")

if (DTRACE)
  ADD_CUSTOM_COMMAND(
    OUTPUT ${PROJECT_BINARY_DIR}/include/minizinc/support/dtrace_probes.h
    COMMAND ${DTRACE} -h -s ${CMAKE_SOURCE_DIR}/include/minizinc/support/dtrace_probes.d -o ${PROJECT_BINARY_DIR}/include/minizinc/support/dtrace_probes.h
    DEPENDS ${PROJECT_SOURCE_DIR}/include/minizinc/support/dtrace_probes.d
  )
  set(DTRACE_HEADER ${PROJECT_BINARY_DIR}/include/minizinc/support/dtrace_probes.h)

  if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    ADD_CUSTOM_COMMAND(
      OUTPUT ${PROJECT_BINARY_DIR}/dtrace_probes.o
      COMMAND ${DTRACE} -G -s ${CMAKE_SOURCE_DIR}/include/minizinc/support/dtrace_probes.d -o ${PROJECT_BINARY_DIR}/dtrace_probes.o
      DEPENDS ${PROJECT_SOURCE_DIR}/include/minizinc/support/dtrace_probes.d
    )
    add_library(minizinc_dtrace_object OBJECT IMPORTED GLOBAL)
    set_target_properties(minizinc_dtrace_object PROPERTIES IMPORTED_OBJECTS ${PROJECT_BINARY_DIR}/dtrace_probes.o)

    set(DTRACE_OBJECT $<TARGET_OBJECTS:minizinc_dtrace_object>)
  endif()
endif()

set(DTRACE_SOURCES
  include/minizinc/support/dtrace.h
  ${DTRACE_HEADER}
  ${DTRACE_OBJECT}
)
