if(GECODE_FOUND AND USE_GECODE)
  add_library(mza SHARED
    lib/c_interface.cpp
    include/minizinc/c_interface.h
  )
  target_link_libraries(mza mzn)

	install(
		TARGETS mza
		EXPORT libminizincTargets
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
	)
endif()
