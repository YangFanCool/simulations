if (NOT (3 IN_LIST AMReX_SPACEDIM))
    return()
endif()

set(_sources  main.cpp)

set(_input_files)

setup_test(3 _sources _input_files)

unset(_sources)
unset(_input_files)
