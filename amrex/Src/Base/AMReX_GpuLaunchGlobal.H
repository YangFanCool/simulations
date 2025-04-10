#ifndef AMREX_GPU_LAUNCH_GLOBAL_H_
#define AMREX_GPU_LAUNCH_GLOBAL_H_
#include <AMReX_Config.H>

#include <AMReX_GpuQualifiers.H>

#if defined(AMREX_USE_CUDA) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

namespace amrex {

#if defined(AMREX_USE_CUDA) || defined(AMREX_USE_HIP)
    // We cannot take rvalue lambdas.
    template<class L>
    AMREX_GPU_GLOBAL void launch_global (L f0) { f0(); }

    template<int amrex_launch_bounds_max_threads, class L>
    __launch_bounds__(amrex_launch_bounds_max_threads)
    AMREX_GPU_GLOBAL void launch_global (L f0) { f0(); }

    template<int amrex_launch_bounds_max_threads, int min_blocks, class L>
    __launch_bounds__(amrex_launch_bounds_max_threads, min_blocks)
    AMREX_GPU_GLOBAL void launch_global (L f0) { f0(); }
#endif

}

#if defined(AMREX_USE_CUDA) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
