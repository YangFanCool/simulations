foreach(D IN LISTS AMReX_SPACEDIM)
    if(D EQUAL 1 OR NOT AMReX_LINEAR_SOLVERS_INCFLO)
       continue()
    endif()

    set(_sources main.cpp MyTest.cpp MyTest.H MyTestPlotfile.cpp)

    set(_input_files inputs-ci)

    setup_test(${D} _sources _input_files)

    unset(_sources)
    unset(_input_files)
endforeach()
