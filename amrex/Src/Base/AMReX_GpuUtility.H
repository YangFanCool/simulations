#ifndef AMREX_GPU_UTILITY_H_
#define AMREX_GPU_UTILITY_H_
#include <AMReX_Config.H>

#include <AMReX_GpuQualifiers.H>
#include <AMReX_GpuTypes.H>
#include <AMReX_GpuControl.H>
#include <AMReX_GpuDevice.H>
#include <AMReX_Extension.H>
#include <AMReX_REAL.H>
#include <AMReX_INT.H>
#include <AMReX_Array.H>
#include <AMReX_Array4.H>
#include <iosfwd>
#include <cmath>
#include <cstring>

#ifdef AMREX_USE_CUDA
#include <cuda.h>
#include <curand_kernel.h>   // Is this needed here?
#endif

namespace amrex {
namespace Gpu {

    template <typename T>
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    T LDG (Array4<T> const& a, int i, int j, int k) noexcept {
#if defined(AMREX_USE_CUDA)
        AMREX_IF_ON_DEVICE((return __ldg(a.ptr(i,j,k));))
        AMREX_IF_ON_HOST((return a(i,j,k);))
#else
        return a(i,j,k);
#endif
    }

    template <typename T>
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    T LDG (Array4<T> const& a, int i, int j, int k, int n) noexcept {
#if defined(AMREX_USE_CUDA)
        AMREX_IF_ON_DEVICE((return __ldg(a.ptr(i,j,k,n));))
        AMREX_IF_ON_HOST((return a(i,j,k,n);))
#else
        return a(i,j,k,n);
#endif
    }

    inline bool isManaged (void const* p) noexcept {
#ifdef AMREX_USE_CUDA
        CUpointer_attribute attrib = CU_POINTER_ATTRIBUTE_IS_MANAGED;
        unsigned int is_managed = 0;
        void* data[] = { (void*)(&is_managed) };
        CUresult r = cuPointerGetAttributes(1, &attrib, data, (CUdeviceptr)p);
        return r == CUDA_SUCCESS && is_managed;
#elif defined(AMREX_USE_SYCL)
        auto type = sycl::get_pointer_type(p,Device::syclContext());
        return type == sycl::usm::alloc::shared;
#else
        amrex::ignore_unused(p);
        return false;
#endif
    }

    inline bool isDevicePtr (void const* p) noexcept {
#if defined(AMREX_USE_HIP)
        hipPointerAttribute_t attrib;
        hipError_t r = hipPointerGetAttributes(&attrib, p);
#if defined(HIP_VERSION_MAJOR) && (HIP_VERSION_MAJOR < 6)
        return r == hipSuccess && attrib.memoryType == hipMemoryTypeDevice;
#else
        return r == hipSuccess && attrib.type == hipMemoryTypeDevice;
#endif // (HIP_VERSION_MAJOR) && (HIP_VERSION_MAJOR < 6)
#elif defined(AMREX_USE_CUDA)
        CUpointer_attribute attrib = CU_POINTER_ATTRIBUTE_MEMORY_TYPE;
        CUmemorytype mem_type = static_cast<CUmemorytype>(0);
        void* data[] = { (void*)(&mem_type) };
        CUresult r = cuPointerGetAttributes(1, &attrib, data, (CUdeviceptr)p);
        return r == CUDA_SUCCESS && mem_type == CU_MEMORYTYPE_DEVICE;
#elif defined(AMREX_USE_SYCL)
        auto type = sycl::get_pointer_type(p,Device::syclContext());
        return type == sycl::usm::alloc::device;
#else
        amrex::ignore_unused(p);
        return false;
#endif
    }

    inline bool isPinnedPtr (void const* p) noexcept {
#if defined(AMREX_USE_HIP)
        hipPointerAttribute_t attrib;
        hipError_t r = hipPointerGetAttributes(&attrib, p);
#if defined(HIP_VERSION_MAJOR) && (HIP_VERSION_MAJOR < 6)
        return r == hipSuccess && attrib.memoryType == hipMemoryTypeHost;
#else
        return r == hipSuccess && attrib.type == hipMemoryTypeHost;
#endif // (HIP_VERSION_MAJOR) && (HIP_VERSION_MAJOR < 6)
#elif defined(AMREX_USE_CUDA)
        CUpointer_attribute attrib = CU_POINTER_ATTRIBUTE_MEMORY_TYPE;
        CUmemorytype mem_type = static_cast<CUmemorytype>(0);
        void* data[] = { (void*)(&mem_type) };
        CUresult r = cuPointerGetAttributes(1, &attrib, data, (CUdeviceptr)p);
        return r == CUDA_SUCCESS && mem_type == CU_MEMORYTYPE_HOST;
#elif defined(AMREX_USE_SYCL)
        auto type = sycl::get_pointer_type(p,Device::syclContext());
        return type == sycl::usm::alloc::host;
#else
        amrex::ignore_unused(p);
        return false;
#endif
    }

    inline bool isGpuPtr (void const* p) noexcept {
#if defined(AMREX_USE_HIP)
        if (isManaged(p)) { // We might be using CUDA/NVCC
            return true;
        } else {
            hipPointerAttribute_t attrib;
            hipError_t r = hipPointerGetAttributes(&attrib, p);
#if defined(HIP_VERSION_MAJOR) && (HIP_VERSION_MAJOR < 6)
            return r == hipSuccess &&
                (attrib.memoryType == hipMemoryTypeHost   ||
                 attrib.memoryType == hipMemoryTypeDevice);
#else
            return r == hipSuccess &&
                (attrib.type == hipMemoryTypeHost   ||
                 attrib.type == hipMemoryTypeDevice);
#endif // (HIP_VERSION_MAJOR) && (HIP_VERSION_MAJOR < 6)
        }
#elif defined(AMREX_USE_CUDA)
        CUpointer_attribute attrib = CU_POINTER_ATTRIBUTE_MEMORY_TYPE;
        CUmemorytype mem_type = static_cast<CUmemorytype>(0);
        void* data[] = { (void*)(&mem_type) };
        CUresult r = cuPointerGetAttributes(1, &attrib, data, (CUdeviceptr)p);
        return r == CUDA_SUCCESS &&
            (mem_type == CU_MEMORYTYPE_HOST   ||
             mem_type == CU_MEMORYTYPE_DEVICE ||
             mem_type == CU_MEMORYTYPE_ARRAY  ||
             mem_type == CU_MEMORYTYPE_UNIFIED);
#elif defined(AMREX_USE_SYCL)
        auto type = sycl::get_pointer_type(p,Device::syclContext());
        return type != sycl::usm::alloc::unknown;
#else
        amrex::ignore_unused(p);
        return false;
#endif
    }

    template <class T>
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    bool isnan (T m) noexcept
    {
#if defined(_WIN32)
        AMREX_IF_ON_DEVICE((return m != m;))
        AMREX_IF_ON_HOST((return std::isnan(m);))
#elif defined(AMREX_USE_SYCL)
        return sycl::isnan(m);
#else
        return std::isnan(m);
#endif
    }

    template <class T>
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    bool isinf (T m) noexcept
    {
#if defined(_WIN32)
        AMREX_IF_ON_DEVICE((return (2*m == m) && (m != 0);))
        AMREX_IF_ON_HOST((return std::isinf(m);))
#elif defined(AMREX_USE_SYCL)
        return sycl::isinf(m);
#else
        return std::isinf(m);
#endif
    }

    struct StreamItInfo
    {
        bool device_sync;
        StreamItInfo () noexcept
            : device_sync(!Gpu::inNoSyncRegion()) {}
        StreamItInfo& DisableDeviceSync () noexcept {
            device_sync = false;
            return *this;
        }
    };

    class StreamIter
    {
    public:
        StreamIter (int n, bool is_thread_safe=true) noexcept;
        StreamIter (int n, const StreamItInfo& info, bool is_thread_safe=true) noexcept;

        ~StreamIter ();

        StreamIter (StreamIter const&) = delete;
        StreamIter (StreamIter &&) = delete;
        void operator= (StreamIter const&) = delete;
        void operator= (StreamIter &&) = delete;

        int operator() () const noexcept { return m_i; }

        [[nodiscard]] bool isValid () const noexcept { return m_i < m_n; }

#if !defined(AMREX_USE_GPU)
        void operator++ () noexcept { ++m_i; }
#else
        void operator++ () noexcept;
#endif

    private:
        void init () noexcept; // NOLINT

        int m_n;
        int m_i;
        bool m_threadsafe;
        bool m_sync;
    };

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void* memcpy (void* dest, const void* src, std::size_t count)
{
#if defined(__HIP_DEVICE_COMPILE__) && defined(AMREX_USE_HIP)
    return ::memcpy(dest, src, count);
#else
    return std::memcpy(dest, src, count);
#endif
}

} // namespace Gpu

#ifdef AMREX_USE_GPU
std::ostream& operator<< (std::ostream& os, const dim3& d);
#endif

using Gpu::isnan;
using Gpu::isinf;

} // namespace amrex

#endif
