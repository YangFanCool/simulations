# ---------------------------------------------------------------
# Programmer(s): Slaven Peles and Cody J. Balos @ LLNL
# ---------------------------------------------------------------
# SUNDIALS Copyright Start
# Copyright (c) 2002-2024, Lawrence Livermore National Security
# and Southern Methodist University.
# All rights reserved.
#
# See the top-level LICENSE and NOTICE files for details.
#
# SPDX-License-Identifier: BSD-3-Clause
# SUNDIALS Copyright End
# ---------------------------------------------------------------
# CMakeLists.txt file for raja nvector examples
# ---------------------------------------------------------------

# Example lists are tuples "name\;args\;nodes\;tasks\;type" where the type is
# develop for examples excluded from 'make test' in releases
set(examples_list
    "test_nvector_mpiraja.cpp\;1000 0\;\;\;" # run sequentially
    "test_nvector_mpiraja.cpp\;1000 0\;1\;4\;" # run parallel on 4 procs
)

# Add source directory to include directories
include_directories(. ..)

if(SUNDIALS_RAJA_BACKENDS MATCHES "CUDA")
  set(_lang CUDA)
else()
  set(_lang CXX)
endif()

if((RAJA_BACKENDS MATCHES "TARGET_OPENMP") OR (RAJA_BACKENDS MATCHES "OPENMP"))
  set(OTHER_LIBS OpenMP::OpenMP_CXX)
endif()

# Add the build and install targets for each example
foreach(example_tuple ${nvector_raja_examples})

  # parse the example tuple
  list(GET example_tuple 0 example)
  list(GET example_tuple 1 example_args)
  list(GET example_tuple 2 number_of_nodes)
  list(GET example_tuple 3 number_of_tasks)
  list(GET example_tuple 4 example_type)

  # extract the file name without extension
  get_filename_component(example_target ${example} NAME_WE)

  # check if this example has already been added, only need to add example
  # source files once for testing with different inputs
  if(NOT TARGET ${example})

    # example source files
    add_executable(${example_target} ${example})

    set_source_files_properties(${example_target} PROPERTIES LANGUAGE ${_lang})

    set_target_properties(${example_target} PROPERTIES FOLDER "Examples")

    target_include_directories(${example_target} PUBLIC ${MPI_CXX_INCLUDE_DIRS})

    target_link_libraries(
      ${example_target}
      PRIVATE test_nvector_obj
              test_nvectormpi_obj
              sundials_nvecmpiplusx
              sundials_nvecraja
              ${MPI_CXX_LIBRARIES}
              ${EXE_EXTRA_LINK_LIBS}
              ${OTHER_LIBS})

  endif()

  # check if example args are provided and set the test name
  if("${example_args}" STREQUAL "")
    if("${number_of_tasks}" STREQUAL "")
      set(test_name ${example})
    else()
      string(REGEX REPLACE " " "_" test_name ${example}_${number_of_tasks})
    endif()
  else()
    if("${number_of_tasks}" STREQUAL "")
      string(REGEX REPLACE " " "_" test_name ${example}_${example_args})
    else()
      string(REGEX REPLACE " " "_" test_name
                           ${example}_${number_of_tasks}_${example_args})
    endif()
  endif()

  # add example to regression tests
  sundials_add_test(
    ${test_name} ${example_target}
    TEST_ARGS ${example_args}
    MPI_NPROCS ${number_of_tasks}
    EXAMPLE_TYPE ${example_type}
    NODIFF)

endforeach()

if(EXAMPLES_INSTALL)

  if((RAJA_BACKENDS MATCHES "TARGET_OPENMP") OR (RAJA_BACKENDS MATCHES "OPENMP"
                                                ))
    set(EXAMPLES_FIND_PACKAGE "find_package(OpenMP REQUIRED)\n")
    set(_openmp_target OpenMP::OpenMP_CXX)
  endif()

  if(RAJA_NEEDS_THREADS)
    set(EXAMPLES_FIND_PACKAGE
        "${EXAMPLES_FIND_PACKAGE}find_package(Threads REQUIRED)\n")
  endif()

  sundials_install_examples(
    nvec_mpiraja examples_list
    EXAMPLES_DEPENDENCIES test_nvector.c test_mpinvector.c
    EXTRA_FILES
      ${SUNDIALS_SOURCE_DIR}/test/unit_tests/nvector/test_nvector.c
      ${SUNDIALS_SOURCE_DIR}/test/unit_tests/nvector/test_nvector.h
      ${SUNDIALS_SOURCE_DIR}/test/unit_tests/nvector/test_mpinvector.c
    CMAKE_TEMPLATE cmakelists_${_lang}_MPI_ex.in
    SOLVER_LIBRARY sundials_ida
    SUNDIALS_TARGETS generic nvecmpiplusx nvecraja
    OTHER_TARGETS ${_openmp_target}
    DESTINATION nvector/mpiraja)

endif()
