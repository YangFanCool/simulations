// ---------------------------------------------------------------
// Programmer: Daniel R. Reynolds @ SMU
// ---------------------------------------------------------------
// SUNDIALS Copyright Start
// Copyright (c) 2002-2024, Lawrence Livermore National Security
// and Southern Methodist University.
// All rights reserved.
//
// See the top-level LICENSE and NOTICE files for details.
//
// SPDX-License-Identifier: BSD-3-Clause
// SUNDIALS Copyright End
// ---------------------------------------------------------------
// Swig interface file
// ---------------------------------------------------------------

%module fsunadaptcontroller_mrihtol_mod

// include code common to all implementations
%include "fsunadaptcontroller.i"

%{
#include "sunadaptcontroller/sunadaptcontroller_mrihtol.h"
%}

%sunadaptcontroller_impl(MRIHTol)

// Process and wrap functions in the following files
%include "sunadaptcontroller/sunadaptcontroller_mrihtol.h"
