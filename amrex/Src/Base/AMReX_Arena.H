#ifndef AMREX_ARENA_H_
#define AMREX_ARENA_H_
#include <AMReX_Config.H>

#include <AMReX_BLassert.H>
#include <AMReX_INT.H>

#ifdef AMREX_TINY_PROFILING
#include <AMReX_TinyProfiler.H>
#else
namespace amrex {
    struct MemStat {};
}
#endif

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <map>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace amrex {

/**
 * \brief Given a minimum required size of size bytes, this returns
 * the next largest arena size that will align to align_requirement bytes
 */
inline std::size_t aligned_size (std::size_t align_requirement, std::size_t size) noexcept
{
    return ((size + (align_requirement-1)) / align_requirement) * align_requirement;
}

inline bool is_aligned (const void* p, std::size_t alignment) noexcept
{
    auto* q = const_cast<void*>(p);
    auto space = alignment;
    return std::align(alignment, alignment, q, space) == p;
}

class Arena;

Arena* The_Arena ();
Arena* The_Async_Arena ();
Arena* The_Device_Arena ();
Arena* The_Managed_Arena ();
Arena* The_Pinned_Arena ();
Arena* The_Comms_Arena ();
Arena* The_Cpu_Arena ();

struct ArenaInfo
{
    Long release_threshold = std::numeric_limits<Long>::max();
    bool use_cpu_memory = false;
    bool device_use_managed_memory = true;
    bool device_set_readonly = false;
    bool device_set_preferred = false;
    bool device_use_hostalloc = false;
    ArenaInfo& SetReleaseThreshold (Long rt) noexcept {
        release_threshold = rt;
        return *this;
    }
    ArenaInfo& SetDeviceMemory () noexcept {
        device_use_managed_memory = false;
        device_use_hostalloc = false;
        return *this;
    }
    ArenaInfo& SetReadOnly () noexcept {
        BL_ASSERT(device_use_managed_memory);
        device_set_readonly = true;
        return *this;
    }
    ArenaInfo& SetPreferred () noexcept {
        BL_ASSERT(device_use_managed_memory);
        device_set_preferred = true;
        return *this;
    }
    ArenaInfo& SetHostAlloc () noexcept {
        device_use_hostalloc = true;
        device_use_managed_memory = false;
        return *this;
    }
    ArenaInfo& SetCpuMemory () noexcept {
        use_cpu_memory = true;
        device_use_managed_memory = false;
        device_set_readonly = false;
        device_set_preferred = false;
        device_use_hostalloc = false;
        return *this;
    }
};

/**
* \brief
* A virtual base class for objects that manage their own dynamic
* memory allocation.
*/
class Arena
{
public:

    virtual ~Arena () = default;
    Arena () noexcept = default;

    Arena (const Arena& rhs) = delete;
    Arena (Arena&& rhs) = delete;
    Arena& operator= (const Arena& rhs) = delete;
    Arena& operator= (Arena&& rhs) = delete;

    /**
    * Allocate a dynamic memory arena
    * \param sz size of the memory request
    * \return a pointer to the allocated memory
    */
    [[nodiscard]] virtual void* alloc (std::size_t sz) = 0;

    /**
     * Try to allocate in-place by extending the capacity of given pointer.
     */
    [[nodiscard]] virtual std::pair<void*,std::size_t>
    alloc_in_place (void* /*pt*/, std::size_t /*szmin*/, std::size_t szmax)
    {
        auto* p = alloc(szmax);
        return std::make_pair(p, szmax);
    }

    /**
     * Try to shrink in-place
     */
    [[nodiscard]] virtual void*
    shrink_in_place (void* /*pt*/, std::size_t sz)
    {
        return alloc(sz);
    }

    /**
    * \brief A pure virtual function for deleting the arena pointed to by pt
    */
    virtual void free (void* pt) = 0;

    /**
    * \brief Free unused memory back to the system.  Return value is the
    * amount memory freed.
    */
    virtual std::size_t freeUnused () { return 0; }

    // isDeviceAccessible and isHostAccessible can both be true.
    [[nodiscard]] virtual bool isDeviceAccessible () const;
    [[nodiscard]] virtual bool isHostAccessible () const;

    // Note that isManaged, isDevice and isPinned are mutually exclusive.
    // For memory allocated by cudaMalloc* etc., one of them returns true.
    // Otherwise, neither is true.
    [[nodiscard]] virtual bool isManaged () const;
    [[nodiscard]] virtual bool isDevice () const;
    [[nodiscard]] virtual bool isPinned () const;

    /**
     * \brief Does the device have enough free memory for allocating this
     * much memory?  For CPU builds, this always return true.  This is not a
     * const function because it may attempt to release memory back to the
     * system.
     */
    [[nodiscard]] virtual bool hasFreeDeviceMemory (std::size_t sz);

    /**
     * \brief Add this Arena to the list of Arenas that are profiled by TinyProfiler.
     * \param memory_name The name of this arena in the TinyProfiler output.
     */
    void registerForProfiling (const std::string& memory_name);

    /**
     * \brief Remove this Arena from the list of Arenas that are profiled by TinyProfiler.
     * This is equivalent to destructing and re-constructing the Arena
     */
    void deregisterFromProfiling ();

#ifdef AMREX_USE_GPU
    //! Is this GPU stream ordered memory allocator?
    [[nodiscard]] virtual bool isStreamOrderedArena () const { return false; }
#endif

    /**
    * \brief Given a minimum required arena size of sz bytes, this returns
    * the next largest arena size that will align to align_size bytes
    */
    static std::size_t align (std::size_t sz);

    static void Initialize (bool minimal);
    static void PrintUsage ();
    static void PrintUsageToFiles (std::string const& filename, std::string const& message);
    static void Finalize ();

#if 0
    union Word
    {
        void*  p;
        long long ll;
        long double ld;
        void (*f) ();
    };
    static const std::size_t align_size = sizeof(Word);
#endif

    static const std::size_t align_size = 16;

    /**
     *  \brief Return the ArenaInfo object for querying
     */
    [[nodiscard]] const ArenaInfo& arenaInfo () const { return arena_info; }

protected:

    ArenaInfo arena_info;

    virtual std::size_t freeUnused_protected () { return 0; }
    void* allocate_system (std::size_t nbytes);
    void deallocate_system (void* p, std::size_t nbytes);

    struct ArenaProfiler {
        //! If this arena is profiled by TinyProfiler
        bool m_do_profiling = false;
        //! Mutex for the profiling
        std::mutex m_arena_profiler_mutex;
        //! Data structure used for profiling with TinyProfiler
        std::map<std::string, MemStat> m_profiling_stats;
        //! Track the currently allocated memory, not used by CArena
        std::unordered_map<void*, std::pair<MemStat*, std::size_t>> m_currently_allocated;

        ~ArenaProfiler ();
        ArenaProfiler () noexcept = default;
        ArenaProfiler (const ArenaProfiler& rhs) = delete;
        ArenaProfiler (ArenaProfiler&& rhs) = delete;
        ArenaProfiler& operator= (const ArenaProfiler& rhs) = delete;
        ArenaProfiler& operator= (ArenaProfiler&& rhs) = delete;

        void profile_alloc (void* ptr, std::size_t nbytes);

        void profile_free (void* ptr);

    } m_profiler;
};

}

#endif /*BL_ARENA_H*/
