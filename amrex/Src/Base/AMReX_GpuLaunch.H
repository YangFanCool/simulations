#ifndef AMREX_GPU_LAUNCH_H_
#define AMREX_GPU_LAUNCH_H_
#include <AMReX_Config.H>

#include <AMReX_GpuQualifiers.H>
#include <AMReX_GpuKernelInfo.H>
#include <AMReX_GpuControl.H>
#include <AMReX_GpuTypes.H>
#include <AMReX_GpuError.H>
#include <AMReX_GpuRange.H>
#include <AMReX_GpuDevice.H>
#include <AMReX_GpuMemory.H>
#include <AMReX_GpuReduce.H>
#include <AMReX_Tuple.H>
#include <AMReX_Box.H>
#include <AMReX_Loop.H>
#include <AMReX_Extension.H>
#include <AMReX_BLassert.H>
#include <AMReX_TypeTraits.H>
#include <AMReX_GpuLaunchGlobal.H>
#include <AMReX_RandomEngine.H>
#include <AMReX_Algorithm.H>
#include <AMReX_Math.H>
#include <AMReX_Vector.H>
#include <cstddef>
#include <limits>
#include <algorithm>
#include <utility>

#define AMREX_GPU_NCELLS_PER_THREAD 3
#define AMREX_GPU_Y_STRIDE 1
#define AMREX_GPU_Z_STRIDE 1

#ifdef AMREX_USE_CUDA
#  define AMREX_LAUNCH_KERNEL(MT, blocks, threads, sharedMem, stream, ... ) \
        amrex::launch_global<MT><<<blocks, threads, sharedMem, stream>>>(__VA_ARGS__)
#elif defined(AMREX_USE_HIP)
#  define AMREX_LAUNCH_KERNEL(MT, blocks, threads, sharedMem, stream, ... ) \
        hipLaunchKernelGGL(launch_global<MT>, blocks, threads, sharedMem, stream, __VA_ARGS__)
#endif


namespace amrex {

// We cannot take rvalue lambdas.
// ************************************************
//  Variadic lambda function wrappers for C++ CUDA/HIP Kernel calls.

#if defined(AMREX_USE_CUDA) || defined(AMREX_USE_HIP)
    template<class L, class... Lambdas>
    AMREX_GPU_GLOBAL void launch_global (L f0, Lambdas... fs) { f0(); call_device(fs...); }

    template<class L>
    AMREX_GPU_DEVICE void call_device (L&& f0) noexcept { f0(); }

    template<class L, class... Lambdas>
    AMREX_GPU_DEVICE void call_device (L&& f0, Lambdas&&... fs) noexcept {
        f0();
        call_device(std::forward<Lambdas>(fs)...);
    }
#endif

// CPU variation

    template<class L>
    void launch_host (L&& f0) noexcept { std::forward<L>(f0)(); }

    template<class L, class... Lambdas>
    void launch_host (L&& f0, Lambdas&&... fs) noexcept {
        std::forward<L>(f0)();
        launch_host(std::forward<Lambdas>(fs)...);
    }


    template <class T> class LayoutData;
    class FabArrayBase;

namespace Gpu {

#ifdef AMREX_USE_GPU
    inline constexpr std::size_t numThreadsPerBlockParallelFor () {
        return AMREX_GPU_MAX_THREADS;
    }
#else
    inline constexpr std::size_t numThreadsPerBlockParallelFor () { return 0; }
#endif

// ************************************************

    struct ComponentBox {
        Box box;
        int ic;
        int nc;
    };

    struct GridSize {
        int numBlocks;
        int numThreads;
        int globalBlockId;
    };

// ************************************************

    AMREX_GPU_HOST_DEVICE
    inline
    Box getThreadBox (const Box& bx, Long offset) noexcept
    {
        AMREX_IF_ON_DEVICE((
            const auto len = bx.length3d();
            Long k = offset / (len[0]*len[1]);
            Long j = (offset - k*(len[0]*len[1])) / len[0];
            Long i = (offset - k*(len[0]*len[1])) - j*len[0];
            IntVect iv{AMREX_D_DECL(static_cast<int>(i),
                                    static_cast<int>(j),
                                    static_cast<int>(k))};
            iv += bx.smallEnd();
            return (bx & Box(iv,iv,bx.type()));
        ))
        AMREX_IF_ON_HOST((
            amrex::ignore_unused(offset);
            return bx;
        ))
    }

// ************************************************

#ifdef AMREX_USE_GPU
    struct ExecutionConfig {
        ExecutionConfig () noexcept {
            Gpu::Device::grid_stride_threads_and_blocks(numBlocks,numThreads);
        }
        ExecutionConfig (const Box& box) noexcept {
            // If we change this, we must make sure it doesn't break FabArrayUtility Reduce*,
            // which assumes the decomposition is 1D.
            Gpu::Device::n_threads_and_blocks( ((box.numPts()+AMREX_GPU_NCELLS_PER_THREAD-1)/AMREX_GPU_NCELLS_PER_THREAD), numBlocks, numThreads );
#if 0
            Box b = amrex::surroundingNodes(box);
            b -= box.smallEnd();
            b.coarsen(IntVect(AMREX_D_DECL(1,AMREX_GPU_Y_STRIDE,AMREX_GPU_Z_STRIDE)));
            Gpu::Device::c_threads_and_blocks(b.loVect(), b.hiVect(), numBlocks, numThreads);
#endif
        }
        ExecutionConfig (const Box& box, int comps) noexcept {
            const Box& b = amrex::surroundingNodes(box);
            Gpu::Device::c_comps_threads_and_blocks(b.loVect(), b.hiVect(), comps, numBlocks, numThreads);
        }
        ExecutionConfig (Long N) noexcept {
            Gpu::Device::n_threads_and_blocks(N, numBlocks, numThreads);
        }
        ExecutionConfig (dim3 nb, dim3 nt, std::size_t sm=0) noexcept
            : numBlocks(nb), numThreads(nt), sharedMem(sm) {}

        dim3 numBlocks;
        dim3 numThreads;
        std::size_t sharedMem = 0;
    };

    template <int MT>
    ExecutionConfig
    makeExecutionConfig (Long N) noexcept
    {
        ExecutionConfig ec(dim3{}, dim3{});
        Long numBlocks = (std::max(N,Long(1)) + MT - 1) / MT;
        // ensure that blockDim.x*gridDim.x does not overflow
        numBlocks = std::min(numBlocks, Long(std::numeric_limits<unsigned int>::max()/MT));
        // ensure that the maximum grid size of 2^31-1 won't be exceeded
        numBlocks = std::min(numBlocks, Long(std::numeric_limits<int>::max()));
        ec.numBlocks.x = numBlocks;
        ec.numThreads.x = MT;
        AMREX_ASSERT(MT % Gpu::Device::warp_size == 0);
        return ec;
    }

    template <int MT>
    ExecutionConfig
    makeExecutionConfig (const Box& box) noexcept
    {
        return makeExecutionConfig<MT>(box.numPts());
    }

    struct ExecConfig
    {
        Long start_idx;
        int nblocks;
    };

    template <int MT>
    Vector<ExecConfig> makeNExecutionConfigs (Long N) noexcept
    {
        // Max # of blocks in a kernel launch
        int numblocks_max = std::numeric_limits<int>::max();
        // Max # of threads in a kernel launch
        Long nmax = Long(MT) * numblocks_max;
        // # of launches needed for N elements without using grid-stride
        // loops inside GPU kernels.
        auto nlaunches = int((N+nmax-1)/nmax);
        Vector<ExecConfig> r(nlaunches);
        Long ndone = 0;
        for (int i = 0; i < nlaunches; ++i) {
            int nblocks;
            if (N > nmax) {
                nblocks = numblocks_max;
                N -= nmax;
            } else {
                nblocks = int((N+MT-1)/MT);
            }
            // At which element ID the kernel should start
            r[i].start_idx = ndone;
            ndone += Long(nblocks) * MT;
            // # of blocks in this launch
            r[i].nblocks = nblocks;
        }
        return r;
    }

    template <int MT, int dim>
    Vector<ExecConfig> makeNExecutionConfigs (BoxND<dim> const& box) noexcept
    {
        return makeNExecutionConfigs<MT>(box.numPts());
    }
#endif

}
}


#ifdef AMREX_USE_GPU
#include <AMReX_GpuLaunchMacrosG.H>
#include <AMReX_GpuLaunchFunctsG.H>
#else
#include <AMReX_GpuLaunchMacrosC.H>
#include <AMReX_GpuLaunchFunctsC.H>
#endif

#include <AMReX_GpuLaunch.nolint.H>

#include <AMReX_CTOParallelForImpl.H>

#endif
