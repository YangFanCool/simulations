// Do not include this header anywhere other than AMReX_GpuLaunchMacrosC.H.
// The purpose of this file is to avoid clang-tidy.

#define AMREX_GPU_LAUNCH_HOST_DEVICE_LAMBDA_RANGE(TN,TI,block) \
    { \
        for (auto const TI : amrex::Gpu::Range(TN)) { \
            block \
        } \
    }

#define AMREX_GPU_LAUNCH_HOST_DEVICE_LAMBDA_RANGE_2(TN1,TI1,block1,TN2,TI2,block2) \
    { \
        for (auto const TI1 : amrex::Gpu::Range(TN1)) { \
            block1 \
        } \
        for (auto const TI2 : amrex::Gpu::Range(TN2)) { \
            block2 \
        } \
    }

#define AMREX_GPU_LAUNCH_HOST_DEVICE_LAMBDA_RANGE_3(TN1,TI1,block1,TN2,TI2,block2,TN3,TI3,block3) \
    { \
        for (auto const TI1 : amrex::Gpu::Range(TN1)) { \
            block1 \
        } \
        for (auto const TI2 : amrex::Gpu::Range(TN2)) { \
            block2 \
        } \
        for (auto const TI3 : amrex::Gpu::Range(TN3)) { \
            block3 \
        } \
    }

#define AMREX_GPU_LAUNCH_DEVICE_LAMBDA_RANGE(...) AMREX_GPU_LAUNCH_HOST_DEVICE_LAMBDA_RANGE(__VA_ARGS__)
#define AMREX_GPU_LAUNCH_DEVICE_LAMBDA_RANGE_2(...) AMREX_GPU_LAUNCH_HOST_DEVICE_LAMBDA_RANGE_2(__VA_ARGS__)
#define AMREX_GPU_LAUNCH_DEVICE_LAMBDA_RANGE_3(...) AMREX_GPU_LAUNCH_HOST_DEVICE_LAMBDA_RANGE_3(__VA_ARGS__)

#define AMREX_FOR_1D_IMPL(n,i,block) \
        for (auto i = decltype(n){0}; i < n; ++i) {     \
            block \
        }

#define AMREX_PARALLEL_FOR_1D_IMPL(n,i,block) \
        AMREX_PRAGMA_SIMD \
        for (auto i = decltype(n){0}; i < n; ++i) {     \
            block \
        }

#define AMREX_FOR_3D_IMPL(box,i,j,k,block) \
    { \
        const auto amrex_i_lo = amrex::lbound(box); \
        const auto amrex_i_hi = amrex::ubound(box); \
        for (int k = amrex_i_lo.z; k <= amrex_i_hi.z; ++k) { \
        for (int j = amrex_i_lo.y; j <= amrex_i_hi.y; ++j) { \
        for (int i = amrex_i_lo.x; i <= amrex_i_hi.x; ++i) { \
            block \
        }}} \
    }

#define AMREX_PARALLEL_FOR_3D_IMPL(box,i,j,k,block) \
    { \
        const auto amrex_i_lo = amrex::lbound(box); \
        const auto amrex_i_hi = amrex::ubound(box); \
        for (int k = amrex_i_lo.z; k <= amrex_i_hi.z; ++k) { \
        for (int j = amrex_i_lo.y; j <= amrex_i_hi.y; ++j) { \
        AMREX_PRAGMA_SIMD \
        for (int i = amrex_i_lo.x; i <= amrex_i_hi.x; ++i) { \
            block \
        }}} \
    }

#define AMREX_FOR_4D_IMPL(box,ncomp,i,j,k,n,block) \
    { \
        const auto amrex_i_lo = amrex::lbound(box); \
        const auto amrex_i_hi = amrex::ubound(box); \
        for (int n = 0; n < ncomp; ++n) { \
        for (int k = amrex_i_lo.z; k <= amrex_i_hi.z; ++k) { \
        for (int j = amrex_i_lo.y; j <= amrex_i_hi.y; ++j) { \
        for (int i = amrex_i_lo.x; i <= amrex_i_hi.x; ++i) { \
            block \
        }}}} \
    }

#define AMREX_PARALLEL_FOR_4D_IMPL(box,ncomp,i,j,k,n,block) \
    { \
        const auto amrex_i_lo = amrex::lbound(box); \
        const auto amrex_i_hi = amrex::ubound(box); \
        for (int n = 0; n < ncomp; ++n) { \
        for (int k = amrex_i_lo.z; k <= amrex_i_hi.z; ++k) { \
        for (int j = amrex_i_lo.y; j <= amrex_i_hi.y; ++j) { \
        AMREX_PRAGMA_SIMD \
        for (int i = amrex_i_lo.x; i <= amrex_i_hi.x; ++i) { \
            block \
        }}}} \
    }

#define AMREX_FOR_1D(...) AMREX_FOR_1D_IMPL(__VA_ARGS__);
#define AMREX_FOR_3D(...) AMREX_FOR_3D_IMPL(__VA_ARGS__);
#define AMREX_FOR_4D(...) AMREX_FOR_4D_IMPL(__VA_ARGS__);

#define AMREX_PARALLEL_FOR_1D(...) AMREX_PARALLEL_FOR_1D_IMPL(__VA_ARGS__);
#define AMREX_PARALLEL_FOR_3D(...) AMREX_PARALLEL_FOR_3D_IMPL(__VA_ARGS__);
#define AMREX_PARALLEL_FOR_4D(...) AMREX_PARALLEL_FOR_4D_IMPL(__VA_ARGS__);

#define AMREX_HOST_DEVICE_FOR_1D(...) AMREX_FOR_1D_IMPL(__VA_ARGS__);
#define AMREX_HOST_DEVICE_FOR_3D(...) AMREX_FOR_3D_IMPL(__VA_ARGS__);
#define AMREX_HOST_DEVICE_FOR_4D(...) AMREX_FOR_4D_IMPL(__VA_ARGS__);

#define AMREX_HOST_DEVICE_PARALLEL_FOR_1D(...) AMREX_PARALLEL_FOR_1D_IMPL(__VA_ARGS__);
#define AMREX_HOST_DEVICE_PARALLEL_FOR_3D(...) AMREX_PARALLEL_FOR_3D_IMPL(__VA_ARGS__);
#define AMREX_HOST_DEVICE_PARALLEL_FOR_4D(...) AMREX_PARALLEL_FOR_4D_IMPL(__VA_ARGS__);
