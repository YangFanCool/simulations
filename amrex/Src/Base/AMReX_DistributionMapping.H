#ifndef BL_DISTRIBUTIONMAPPING_H
#define BL_DISTRIBUTIONMAPPING_H
#include <AMReX_Config.H>

#include <AMReX.H>
#include <AMReX_Array.H>
#include <AMReX_Vector.H>
#include <AMReX_Box.H>
#include <AMReX_REAL.H>
#include <AMReX_ParallelDescriptor.H>

#include <map>
#include <limits>
#include <memory>
#include <cstddef>
#include <iosfwd>

namespace amrex {

class BoxArray;
class MultiFab;
template <typename T> class FabArray;
template <typename T> class LayoutData;
class FabArrayBase;

/**
* \brief Calculates the distribution of FABs to MPI processes.
*
*  This class calculates the distribution of FABs to MPI processes in a
*  FabArray in a multi-processor environment.  By distribution is meant what
*  MPI process in the multi-processor environment owns what FAB.  Only the BoxArray
*  on which the FabArray is built is used in determining the distribution.
*  The three types of distributions supported are round-robin, knapsack, and SFC.
*  In the round-robin distribution FAB i is owned by CPU i%N where N is total
*  number of CPUs.  In the knapsack distribution the FABs are partitioned
*  across CPUs such that the total volume of the Boxes in the underlying
*  BoxArray are as equal across CPUs as is possible.  The SFC distribution is
*  based on a space filling curve.
*/
class DistributionMapping
{
  public:

    template <typename T> friend class FabArray;
    friend class FabArrayBase;

    //! The distribution strategies
    enum Strategy { UNDEFINED = -1, ROUNDROBIN, KNAPSACK, SFC, RRSFC };

    struct Ref
    {
        //! Constructors to match those in DistributionMapping ....
        Ref () = default;

        explicit Ref (int len) : m_pmap(len) {}

        explicit Ref (const Vector<int>& pmap) : m_pmap(pmap) {}

        explicit Ref (Vector<int>&& pmap) noexcept : m_pmap(std::move(pmap)) {}

        //! dtor, copy-ctor, copy-op=, move-ctor, and move-op= are compiler generated.

        void clear () { m_pmap.clear();  m_index_array.clear();   m_ownership.clear(); }

        Vector<int> m_pmap; //!< index array for all boxes
        Vector<int> m_index_array;  //!< index array for local boxes owned by the team
        std::vector<bool> m_ownership; //!< true ownership
    };

    //! The default constructor.
    DistributionMapping () noexcept;

    //! The copy constructor.
    DistributionMapping (const DistributionMapping& rhs) = default;

    //! The copy assignment operator.
    DistributionMapping& operator= (const DistributionMapping& rhs) = default;

    //! The move constructor.
    DistributionMapping (DistributionMapping&& rhs) noexcept = default;

    //! The move assignment operator.
    DistributionMapping& operator= (DistributionMapping&& rhs) noexcept = default;

   //! The destructor.
    ~DistributionMapping() noexcept = default;

    /**
    * \brief Create an object with the specified mapping.
    */
    explicit DistributionMapping (const Vector<int>& pmap);
    explicit DistributionMapping (Vector<int>&& pmap) noexcept;
    //! Build mapping out of BoxArray over nprocs processors.
    explicit DistributionMapping (const BoxArray& boxes,
                                  int nprocs = ParallelDescriptor::NProcs());

    explicit DistributionMapping (std::shared_ptr<Ref> a_ref);

    /**
    * \brief This is a very specialized distribution map.
    * Do NOT use it unless you really understand what it does.
    */
    DistributionMapping (const DistributionMapping& d1,
                         const DistributionMapping& d2);

    /**
    * \brief Build mapping out of BoxArray over nprocs processors.
    * You need to call this if you built your DistributionMapping
    * with the default constructor.
    */
    void define (const BoxArray& boxes, int nprocs = ParallelDescriptor::NProcs());
    /**
    * \brief Build mapping out of an Array of ints. You need to call this if you
    * built your DistributionMapping with the default constructor.
    */
    void define (const Vector<int>& pmap);
    void define (Vector<int>&& pmap) noexcept;
    /**
    * \brief Returns a constant reference to the mapping of boxes in the
    * underlying BoxArray to the CPU that holds the FAB on that Box.
    * ProcessorMap()[i] is an integer in the interval [0, NCPU) where
    * NCPU is the number of CPUs being used.
    */
    [[nodiscard]] const Vector<int>& ProcessorMap () const noexcept;

    //! Length of the underlying processor map.
    [[nodiscard]] Long size () const noexcept { return Long(m_ref->m_pmap.size()); }
    [[nodiscard]] Long capacity () const noexcept { return Long(m_ref->m_pmap.capacity()); }
    [[nodiscard]] bool empty () const noexcept { return m_ref->m_pmap.empty(); }

    //! Number of references to this DistributionMapping
    [[nodiscard]] Long linkCount () const noexcept { return m_ref.use_count(); }

    //! Equivalent to ProcessorMap()[index].
    [[nodiscard]] int operator[] (int index) const noexcept { return m_ref->m_pmap[index]; }

    std::istream& readFrom (std::istream& is);

    std::ostream& writeOn (std::ostream& os) const;

    //! Set/get the distribution strategy.
    static void strategy (Strategy how);

    static Strategy strategy ();

    //! Set/get the space filling curve threshold.
    static void SFC_Threshold (int n);

    static int SFC_Threshold ();

    //! Are the distributions equal?
    bool operator== (const DistributionMapping& rhs) const noexcept;

    //! Are the distributions different?
    bool operator!= (const DistributionMapping& rhs) const noexcept;

    void SFCProcessorMap (const BoxArray& boxes, const std::vector<Long>& wgts, int nprocs,
                          bool sort=true);
    void SFCProcessorMap (const BoxArray& boxes, const std::vector<Long>& wgts, int nprocs,
                          Real& efficiency, bool sort=true);
    void KnapSackProcessorMap (const std::vector<Long>& wgts, int nprocs,
                               Real* efficiency=nullptr,
                               bool do_full_knapsack=true,
                               int nmax=std::numeric_limits<int>::max(),
                               bool sort=true);
    void KnapSackProcessorMap (const DistributionMapping& olddm,
                               const std::vector<Long>& wgts, Real keep_ratio,
                               Real& old_efficiency, Real& new_efficiency,
                               int nmax=std::numeric_limits<int>::max());
    void RoundRobinProcessorMap (int nboxes, int nprocs, bool sort=true);
    void RoundRobinProcessorMap (const std::vector<Long>& wgts, int nprocs, bool sort=true);

    /**
    * \brief Initializes distribution strategy from ParmParse.
    *
    * ParmParse options are:
    *
    *   DistributionMapping.strategy = ROUNDROBIN
    *   DistributionMapping.strategy = KNAPSACK
    *   DistributionMapping.strategy = SFC
    *   DistributionMapping.strategy = RRFC
    */
    static void Initialize ();

    static void Finalize ();

    static bool SameRefs (const DistributionMapping& lhs,
                          const DistributionMapping& rhs)
          { return lhs.m_ref == rhs.m_ref; }

    static DistributionMapping makeKnapSack (const MultiFab& weight,
                                             int nmax=std::numeric_limits<int>::max());
    static DistributionMapping makeKnapSack (const MultiFab& weight, Real& eff,
                                             int nmax=std::numeric_limits<int>::max());
    static DistributionMapping makeKnapSack (const Vector<Real>& rcost,
                                             int nmax=std::numeric_limits<int>::max());
    static DistributionMapping makeKnapSack (const Vector<Real>& rcost, Real& eff,
                                             int nmax=std::numeric_limits<int>::max(),
                                             bool sort=true);

    /** \brief Computes a new distribution mapping by distributing input costs
     * according to the `knapsack` algorithm.
     * @param[in] rcost_local LayoutData of costs; contains, e.g., costs for the
     *            local boxes in the FAB array, corresponding indices in the global
     *            indices in the FAB array, and the distribution mapping
     * @param[in,out] currentEfficiency writes the efficiency (i.e., mean cost over
     *                all MPI ranks, normalized to the max cost) given the current
     *                distribution mapping
     * @param[in,out] proposedEfficiency writes the efficiency for the proposed
     *                distribution mapping
     * @param[in] nmax the maximum number of boxes that can be assigned to any
     *            MPI rank by the knapsack algorithm
     * @param[in] broadcastToAll controls whether to transmit the proposed
     *            distribution mapping to all other processes; setting this to
     *            false allows to, e.g., test whether the proposed distribution
     *            mapping is an improvement relative to the current distribution
     *            mapping, before deciding to broadcast the proposed distribution
     *            mapping
     * @param[in] root which process to collect the local costs from others and
     *            compute the proposed distribution mapping
     * @param[in] keep_ratio controls the fraction of load that should be kept on
     *            the original process.
     * @return the proposed load-balanced distribution mapping
     */
    static DistributionMapping makeKnapSack (const LayoutData<Real>& rcost_local,
                                             Real& currentEfficiency, Real& proposedEfficiency,
                                             int nmax=std::numeric_limits<int>::max(),
                                             bool broadcastToAll=true,
                                             int root=ParallelDescriptor::IOProcessorNumber(),
                                             Real keep_ratio = Real(0.0));

    static DistributionMapping makeRoundRobin (const MultiFab& weight);
    static DistributionMapping makeSFC (const MultiFab& weight, bool sort=true);
    static DistributionMapping makeSFC (const MultiFab& weight, Real& eff, bool sort=true);
    static DistributionMapping makeSFC (const Vector<Real>& rcost,
                                        const BoxArray& ba, bool sort=true);
    static DistributionMapping makeSFC (const Vector<Real>& rcost,
                                        const BoxArray& ba, Real& eff, bool sort=true);

    /** \brief Computes a new distribution mapping by distributing input costs
     * according to a `space filling curve` (SFC) algorithm.
     * @param[in] rcost_local LayoutData of costs; contains, e.g., costs for the
     *            local boxes in the FAB array, corresponding indices in the global
     *            indices in the FAB array, and the distribution mapping
     * @param[in,out] currentEfficiency writes the efficiency (i.e., mean cost over
     *                all MPI ranks, normalized to the max cost) given the current
     *                distribution mapping
     * @param[in,out] proposedEfficiency writes the efficiency for the proposed
     *                distribution mapping
     * @param[in] broadcastToAll controls whether to transmit the proposed
     *            distribution mapping to all other processes; setting this to
     *            false allows to, e.g., test whether the proposed distribution
     *            mapping is an improvement relative to the current distribution
     *            mapping, before deciding to broadcast the proposed distribution
     *            mapping
     * @param[in] root which process to collect the local costs from others and
     *            compute the proposed distribution mapping
     * @return the proposed load-balanced distribution mapping
     */
    static DistributionMapping makeSFC (const LayoutData<Real>& rcost_local,
                                        Real& currentEfficiency, Real& proposedEfficiency,
                                        bool broadcastToAll=true,
                                        int root=ParallelDescriptor::IOProcessorNumber());

    /**
    * if use_box_vol is true, weight boxes by their volume in Distribute
    * otherwise, all boxes will be treated with equal weight
    */
    static std::vector<std::vector<int> > makeSFC (const BoxArray& ba,
                                                   bool use_box_vol=true,
                                                   int nprocs=ParallelContext::NProcsSub() );

    /** \brief Computes the average cost per MPI rank given a distribution mapping
     * global cost vector.
     * @param[in] dm distribution mapping (mapping from FAB to MPI processes)
     * @param[in] cost vector giving mapping from FAB to the corresponding cost
     * @param[in,out] efficiency average cost per MPI process, as computed from
     *                the given distribution mapping and cost
     */
    template <typename T>
    static void ComputeDistributionMappingEfficiency (const DistributionMapping& dm,
                                                      const std::vector<T>& cost,
                                                      Real* efficiency);

    [[nodiscard]] std::weak_ptr<Ref> getWeakRef () const;

private:

    const Vector<int>& getIndexArray ();
    const std::vector<bool>& getOwnerShip ();

    //! Ways to create the processor map.
    void RoundRobinProcessorMap (const BoxArray& boxes, int nprocs);
    void KnapSackProcessorMap   (const BoxArray& boxes, int nprocs);
    void SFCProcessorMap        (const BoxArray& boxes, int nprocs);
    void RRSFCProcessorMap      (const BoxArray& boxes, int nprocs);

    using LIpair = std::pair<Long,int>;

    struct LIpairLT
    {
        bool operator () (const LIpair& lhs,
                          const LIpair& rhs) const noexcept
            {
                return lhs.first < rhs.first;
            }
    };

    struct LIpairGT
    {
        bool operator () (const LIpair& lhs,
                          const LIpair& rhs) const noexcept
            {
                return lhs.first > rhs.first;
            }
    };

    static void Sort (std::vector<LIpair>& vec, bool reverse);

    void RoundRobinDoIt (int                  nboxes,
                         int                  nprocs,
                         std::vector<LIpair>* LIpairV = nullptr,
                         bool                 sort = true);

    void KnapSackDoIt (const std::vector<Long>& wgts,
                       int                      nprocs,
                       Real&                    efficiency,
                       bool                     do_full_knapsack,
                       int                      nmax=std::numeric_limits<int>::max(),
                       bool                     sort=true);

    void SFCProcessorMapDoIt (const BoxArray&          boxes,
                              const std::vector<Long>& wgts,
                              int                      nprocs,
                              bool                     sort=true,
                              Real*                    efficiency=nullptr);

    void RRSFCDoIt           (const BoxArray&          boxes,
                              int                      nprocs);

    //! Least used ordering of CPUs (by # of bytes of FAB data).
    static void LeastUsedCPUs (int nprocs, Vector<int>& result);
    /**
    * \brief rteam: Least used ordering of Teams
    * rworker[i]: Least used ordering of team workers for Team i
    */
    static void LeastUsedTeams (Vector<int>& rteam, Vector<Vector<int> >& rworker, int nteams, int nworkers);

    //! A useful typedef.
    using PVMF = void (DistributionMapping::*)(const BoxArray &, int);

    //! Everyone uses the same Strategy -- defaults to SFC.
    static Strategy m_Strategy;
    /**
    * \brief Pointer to one of the CreateProcessorMap() functions.
    * Corresponds to the one specified by m_Strategy.
    */
    static PVMF m_BuildMap;

    //
    //! The data -- a reference-counted pointer to a Ref.
    std::shared_ptr<Ref> m_ref;

public:
    struct RefID {
        constexpr RefID () noexcept {} // =default does not work due to a clang bug // NOLINT
        explicit RefID (Ref* data_) noexcept : data(data_) {}
        bool operator<  (const RefID& rhs) const noexcept { return std::less<>{}(data,rhs.data); }
        bool operator== (const RefID& rhs) const noexcept { return data == rhs.data; }
        bool operator!= (const RefID& rhs) const noexcept { return data != rhs.data; }
        [[nodiscard]] const Ref *dataPtr() const noexcept { return data; }
        void PrintPtr(std::ostream &os) const { os << data << '\n'; }
        friend std::ostream& operator<< (std::ostream& os, const RefID& id);
   private:
        Ref* data = nullptr;
    };

    //! This gives a unique ID of the reference, which is different from dmID above.
    [[nodiscard]] RefID getRefID () const noexcept { return RefID { m_ref.get() }; }
};

using DMRef = DistributionMapping::Ref;

//! Our output operator.
std::ostream& operator<< (std::ostream& os, const DistributionMapping& pmap);

std::ostream& operator<< (std::ostream& os, const DistributionMapping::RefID& id);

/**
 *  \brief Function that creates a DistributionMapping "similar" to that of a MultiFab.
 *
 *  "Similar" means that, if a box in "ba" intersects with any of the
 *  boxes in the BoxArray associated with "mf", taking "ngrow" ghost cells into account,
 *  then that box will be assigned to the proc owning the one it has the maximum amount
 *  of overlap with.
 *
 *  @param[in] ba The BoxArray we want to generate a DistributionMapping for.
 *  @param[in] mf The MultiFab we want said DistributionMapping to be similar to.
 *  @param[in] ng The number of grow cells to use when computing intersection / overlap
 *  @return The computed DistributionMapping.
 */
DistributionMapping MakeSimilarDM (const BoxArray& ba, const MultiFab& mf, const IntVect& ng);

/**
 *  \brief Function that creates a DistributionMapping "similar" to that of a MultiFab.
 *
 *  "Similar" means that, if a box in "ba" intersects with any of the
 *  boxes in the BoxArray associated with "mf", taking "ngrow" ghost cells into account,
 *  then that box will be assigned to the proc owning the one it has the maximum amount
 *  of overlap with.
 *
 *  @param[in] ba The BoxArray we want to generate a DistributionMapping for.
 *  @param[in] src_ba The BoxArray associated with the src DistributionMapping.
 *  @param[in] src_dm The input DistributionMapping we want the output to be similar to.
 *  @param[in] ng The number of grow cells to use when computing intersection / overlap
 *  @return The computed DistributionMapping.
 */
DistributionMapping MakeSimilarDM (const BoxArray& ba, const BoxArray& src_ba,
                                   const DistributionMapping& src_dm, const IntVect& ng);

template <typename T>
void DistributionMapping::ComputeDistributionMappingEfficiency (
    const DistributionMapping& dm, const std::vector<T>& cost, Real* efficiency)
{
    const int nprocs = ParallelDescriptor::NProcs();
    Vector<T> wgts(nprocs, T(0));

    const auto nboxes = int(dm.size());
    for (int ibox = 0; ibox < nboxes; ++ibox) {
        wgts[dm[ibox]] += cost[ibox];
    }

    T max_weight = 0;
    T sum_weight = 0;
    for (auto const& w : wgts) {
        max_weight = std::max(w, max_weight);
        sum_weight += w;
    }

    AMREX_ASSERT(nprocs > 0 && max_weight > T(0));

    *efficiency = static_cast<Real>(sum_weight) /
        (static_cast<Real>(nprocs) * static_cast<Real>(max_weight));
}

}

#endif /*BL_DISTRIBUTIONMAPPING_H*/
