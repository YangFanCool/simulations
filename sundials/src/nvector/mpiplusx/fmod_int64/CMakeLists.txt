# ---------------------------------------------------------------
# Programmer(s): Cody J. Balos @ LLNL
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
# CMakeLists.txt file for the F2003 MPIPlusX NVECTOR object library
# ---------------------------------------------------------------

if(MPI_C_COMPILER)
  # use MPI wrapper as the compiler
  set(CMAKE_C_COMPILER ${MPI_C_COMPILER})
elseif()
  # add MPI_INCLUDE_PATH to include directories
  include_directories(${MPI_INCLUDE_PATH})
endif()
if(MPI_Fortran_COMPILER)
  # use MPI wrapper as the compiler
  set(CMAKE_Fortran_COMPILER ${MPI_Fortran_COMPILER})
endif()

sundials_add_f2003_library(
  sundials_fnvecmpiplusx_mod
  SOURCES fnvector_mpiplusx_mod.f90 fnvector_mpiplusx_mod.c
  LINK_LIBRARIES PUBLIC sundials_fcore_mod
  OBJECT_LIBRARIES
  OUTPUT_NAME sundials_fnvecmpiplusx_mod
  VERSION ${nveclib_VERSION}
  SOVERSION ${nveclib_SOVERSION})

message(STATUS "Added NVECTOR_MPIPLUSX F2003 interface")
