#ifndef AMREX_PhysBCFunct_H_
#define AMREX_PhysBCFunct_H_
#include <AMReX_Config.H>

#include <AMReX_BCRec.H>
#include <AMReX_Geometry.H>
#include <AMReX_MultiFab.H>
#include <AMReX_ArrayLim.H>
#include <AMReX_FilCC_C.H>
#include <AMReX_FilND_C.H>
#include <AMReX_FilFC_C.H>
#include <AMReX_TypeTraits.H>

namespace amrex {

extern "C"
{
    using BndryFuncDefault = void (*)(Real* data, AMREX_ARLIM_P(lo), AMREX_ARLIM_P(hi),
                                      const int* dom_lo, const int* dom_hi,
                                      const Real* dx, const Real* grd_lo,
                                      const Real* time, const int* bc);
    using BndryFunc3DDefault = void (*)(Real* data, const int* lo, const int* hi,
                                        const int* dom_lo, const int* dom_hi,
                                        const Real* dx, const Real* grd_lo,
                                        const Real* time, const int* bc);
}

using UserFillBox = void (*)(Box const& bx, Array4<Real> const& dest,
                             int dcomp, int numcomp,
                             GeometryData const& geom, Real time,
                             const BCRec* bcr, int bcomp,
                             int orig_comp);

//! This version calls function working on array
class BndryFuncArray
{
public:
    BndryFuncArray () noexcept = default;
    BndryFuncArray (BndryFuncDefault inFunc) noexcept : m_func(inFunc) {}
    BndryFuncArray (BndryFunc3DDefault inFunc) noexcept : m_func3D(inFunc) {}

    void operator() (Box const& bx, FArrayBox& dest,
                     int dcomp, int numcomp,
                     Geometry const& geom, Real time,
                     const Vector<BCRec>& bcr, int bcomp,
                     int orig_comp);

    [[nodiscard]] bool RunOnGPU () const noexcept { return m_run_on_gpu; }
//    void setRunOnGPU (bool b) noexcept { m_run_on_gpu = b; }

protected:
    BndryFuncDefault   m_func   = nullptr;
    BndryFunc3DDefault m_func3D = nullptr;
    bool m_run_on_gpu = false;
};

/**
* In this gpu version, F is provided by the user.  It needs to have a
* __device__ operator() that can work on a cell/node for boundaries not
* handled by amrex::fab_filcc/fab_filnd.
*/
template <class F>
class GpuBndryFuncFab
{
public:
    GpuBndryFuncFab () = default;
    GpuBndryFuncFab (F const& a_f) : m_user_f(a_f) {}
    GpuBndryFuncFab (F&& a_f) : m_user_f(std::move(a_f)) {}

    void operator() (Box const& bx, FArrayBox& dest,
                     int dcomp, int numcomp,
                     Geometry const& geom, Real time,
                     const Vector<BCRec>& bcr, int bcomp,
                     int orig_comp);

    template <class FF>
    void ccfcdoit (Box const& bx, FArrayBox& dest,
                   int dcomp, int numcomp,
                   Geometry const& geom, Real time,
                   const Vector<BCRec>& bcr, int bcomp,
                   int orig_comp, FF const& fillfunc);

    void nddoit (Box const& bx, FArrayBox& dest,
                 int dcomp, int numcomp,
                 Geometry const& geom, Real time,
                 const Vector<BCRec>& bcr, int bcomp,
                 int orig_comp);

protected:
    F m_user_f;
};

struct FabFillNoOp
{
    AMREX_GPU_DEVICE
    void operator() (const amrex::IntVect&, amrex::Array4<amrex::Real> const&,
                     int, int, amrex::GeometryData const&, amrex::Real,
                     const amrex::BCRec*, int, int) const
    {}
};

//! This cpu version calls function working on FArrayBox
class CpuBndryFuncFab
{
public:
    CpuBndryFuncFab () = default;
    CpuBndryFuncFab (UserFillBox a_f) : f_user(a_f) {}

    void operator() (Box const& bx, FArrayBox& dest,
                     int dcomp, int numcomp,
                     Geometry const& geom, Real time,
                     const Vector<BCRec>& bcr, int bcomp,
                     int orig_comp);

protected:
    UserFillBox f_user = nullptr;
};

class PhysBCFunctNoOp
{
public:
    void operator() (MultiFab& /*mf*/, int /*dcomp*/, int /*ncomp*/, IntVect const& /*nghost*/,
                     Real /*time*/, int /*bccomp*/) {}
};

class PhysBCFunctUseCoarseGhost
{
public:

    PhysBCFunctUseCoarseGhost (IntVect const& a_fp1_src_ghost)
        : fp1_src_ghost(a_fp1_src_ghost) {}

    template <typename MF, typename Interp>
    PhysBCFunctUseCoarseGhost (MF const& cmf, IntVect const& a_nghost,
                               IntVect const& a_nghost_outside_domain,
                               IntVect const& ratio, Interp* mapper)
        : nghost(a_nghost),
          nghost_outside_domain(a_nghost_outside_domain),
          cghost(cmf.nGrowVect())
    {
        IndexType typ = cmf.ixType();

        const auto& coarsener = mapper->BoxCoarsener(ratio);
        Box tmp(-nghost, IntVect(32), typ);
        Box tmp2 = coarsener.doit(tmp);
        src_ghost = -tmp2.smallEnd();

        tmp = Box(-nghost_outside_domain, IntVect(32), typ);
        tmp2 = coarsener.doit(tmp);
        src_ghost_outside_domain = -tmp2.smallEnd();

        cghost.min(src_ghost);
        AMREX_ALWAYS_ASSERT(cghost.allGE(src_ghost_outside_domain));
    }

    void operator() (MultiFab& /*mf*/, int /*dcomp*/, int /*ncomp*/, IntVect const& /*nghost*/,
                     Real /*time*/, int /*bccomp*/) {}

    IntVect nghost; // # of fine ghosts inside domain to be filled

    IntVect nghost_outside_domain; // # of fine ghosts outside domain to be filled

    // This is the number of coarse ghost cells needed to interpolate fine
    // ghost cells inside the domain
    IntVect src_ghost;

    // This is the number of coarse ghost cells needed to interpolate
    // nghost_outside_domain fine ghost cells outside the domain
    IntVect src_ghost_outside_domain;

    // This is the minimum number of ghost cells needed in coarse MF
    IntVect cghost;

    IntVect fp1_src_ghost; // Used to pass information into FillPatchSingleLevel
};

template <class F>
class PhysBCFunct
{
public:
    PhysBCFunct () = default;

    PhysBCFunct (const Geometry& geom, const Vector<BCRec>& bcr, F const& f)
        : m_geom(geom), m_bcr(bcr), m_f(f)
        {}

    PhysBCFunct (const Geometry& geom, const Vector<BCRec>& bcr, F&& f)
        : m_geom(geom), m_bcr(bcr), m_f(std::move(f))
        {}

    void define (const Geometry& geom, const Vector<BCRec>& bcr, F const& f) {
        m_geom = geom; m_bcr = bcr; m_f = f;
    }

    void define (const Geometry& geom, const Vector<BCRec>& bcr, F&& f) {
        m_geom = geom; m_bcr = bcr; m_f = std::move(f);
    }

    void operator() (MultiFab& mf, int icomp, int ncomp, IntVect const& nghost,
                     Real time, int bccomp)
    {
        if (m_geom.isAllPeriodic()) { return; }

        BL_PROFILE("PhysBCFunct::()");

        AMREX_ASSERT(nghost.allLE(mf.nGrowVect()));

        //! create a grown domain box containing valid + periodic cells
        const Box& domain = m_geom.Domain();
        Box gdomain = amrex::convert(domain, mf.boxArray().ixType());
        for (int i = 0; i < AMREX_SPACEDIM; ++i) {
            if (m_geom.isPeriodic(i)) {
                gdomain.grow(i, nghost[i]);
            }
        }

#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        {
            Vector<BCRec> bcrs(ncomp);
            for (MFIter mfi(mf); mfi.isValid(); ++mfi)
            {
                FArrayBox& dest = mf[mfi];
                const Box& bx = grow(mfi.validbox(),nghost);

                //! if there are cells not in the valid + periodic grown box
                //! we need to fill them here
                //!
                if (!gdomain.contains(bx))
                {
                    //! Based on BCRec for the domain, we need to make BCRec for this Box
                    amrex::setBC(bx, domain, bccomp, 0, ncomp, m_bcr, bcrs);

                    //! Note that we pass 0 as starting component of bcrs.
                    m_f(bx, dest, icomp, ncomp, m_geom, time, bcrs, 0, bccomp);
                }
            }
        }
    }

    // For backward compatibility
    void FillBoundary (MultiFab& mf, int icomp, int ncomp, IntVect const& nghost,
                       Real time, int bccomp) {
        // NOLINTNEXTLINE(readability-suspicious-call-argument)
        this->operator()(mf,icomp,ncomp,nghost,time,bccomp);
    }

private:
    Geometry      m_geom;
    Vector<BCRec> m_bcr;
    F             m_f;
};

template <class F>
void
GpuBndryFuncFab<F>::operator() (Box const& bx, FArrayBox& dest,
                                int dcomp, int numcomp,
                                Geometry const& geom, Real time,
                                const Vector<BCRec>& bcr, int bcomp,
                                int orig_comp)
{
#if defined(__CUDACC__) && (__CUDACC_VER_MAJOR__ == 11) && (__CUDACC_VER_MINOR__ == 6)
    // The compiler is allowed to always reject static_assert(false,..) even
    // without instantiating this function.  The following is to work around
    // the issue.  Now the compiler is not allowed to reject the code unless
    // GpuBndryFuncFab::operator() is instantiated.
    static_assert(std::is_same<F,int>::value,
                  "CUDA 11.6 bug: https://github.com/AMReX-Codes/amrex/issues/2607");
#endif
    if (bx.ixType().cellCentered()) {
        ccfcdoit(bx,dest,dcomp,numcomp,geom,time,bcr,bcomp,orig_comp, FilccCell{});
    } else if (bx.ixType().nodeCentered()) {
        nddoit(bx,dest,dcomp,numcomp,geom,time,bcr,bcomp,orig_comp);
    } else {
        ccfcdoit(bx,dest,dcomp,numcomp,geom,time,bcr,bcomp,orig_comp, FilfcFace{});
    }
}

template <class F>
void
GpuBndryFuncFab<F>::nddoit (Box const& bx, FArrayBox& dest,
                            int dcomp, int numcomp,
                            Geometry const& geom, Real time,
                            const Vector<BCRec>& bcr, int bcomp,
                            int orig_comp)
{
    const IntVect& len = bx.length();

    Box const& domain = amrex::convert(geom.Domain(),IntVect::TheNodeVector());
    Box gdomain = domain;
    for (int idim = 0; idim < AMREX_SPACEDIM; ++idim) {
        if (geom.isPeriodic(idim)) {
            gdomain.grow(idim,len[idim]);
        }
    }

    if (gdomain.contains(bx)) { return; }

    Array4<Real> const& fab = dest.array();
    const auto geomdata = geom.data();

    AsyncArray<BCRec> bcr_aa(bcr.data()+bcomp, numcomp);
    BCRec* bcr_p = bcr_aa.data();

    const auto f_user = m_user_f;

    // xlo
    if (!geom.isPeriodic(0) && bx.smallEnd(0) < domain.smallEnd(0)) {
        Box bndry = bx;
        int dxlo = domain.smallEnd(0);
        bndry.setBig(0,dxlo-1);
        amrex::ParallelFor(bndry, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
        {
            for (int n = dcomp; n < dcomp+numcomp; ++n) {
                fab(i,j,k,n) = fab(dxlo,j,k,n);
            }
            f_user(IntVect(AMREX_D_DECL(i,j,k)), fab, dcomp, numcomp, geomdata, time,
                   bcr_p, 0, orig_comp);
        });
    }

    // xhi
    if (!geom.isPeriodic(0) && bx.bigEnd(0) > domain.bigEnd(0)) {
        Box bndry = bx;
        int dxhi = domain.bigEnd(0);
        bndry.setSmall(0,dxhi+1);
        amrex::ParallelFor(bndry, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
        {
            for (int n = dcomp; n < dcomp+numcomp; ++n) {
                fab(i,j,k,n) = fab(dxhi,j,k,n);
            }
            f_user(IntVect(AMREX_D_DECL(i,j,k)), fab, dcomp, numcomp, geomdata, time,
                   bcr_p, 0, orig_comp);
        });
    }

#if (AMREX_SPACEDIM >= 2)
    // ylo
    if (!geom.isPeriodic(1) && bx.smallEnd(1) < domain.smallEnd(1)) {
        Box bndry = bx;
        int dylo = domain.smallEnd(1);
        bndry.setBig(1,dylo-1);
        amrex::ParallelFor(bndry, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
        {
            for (int n = dcomp; n < dcomp+numcomp; ++n) {
                fab(i,j,k,n) = fab(i,dylo,k,n);
            }
            f_user(IntVect(AMREX_D_DECL(i,j,k)), fab, dcomp, numcomp, geomdata, time,
                   bcr_p, 0, orig_comp);
        });
    }

    // yhi
    if (!geom.isPeriodic(1) && bx.bigEnd(1) > domain.bigEnd(1)) {
        Box bndry = bx;
        int dyhi = domain.bigEnd(1);
        bndry.setSmall(1,dyhi+1);
        amrex::ParallelFor(bndry, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
        {
            for (int n = dcomp; n < dcomp+numcomp; ++n) {
                fab(i,j,k,n) = fab(i,dyhi,k,n);
            }
            f_user(IntVect(AMREX_D_DECL(i,j,k)), fab, dcomp, numcomp, geomdata, time,
                   bcr_p, 0, orig_comp);
        });
    }
#endif

#if (AMREX_SPACEDIM == 3)
    // zlo
    if (!geom.isPeriodic(2) && bx.smallEnd(2) < domain.smallEnd(2)) {
        Box bndry = bx;
        int dzlo = domain.smallEnd(2);
        bndry.setBig(2,dzlo-1);
        amrex::ParallelFor(bndry, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
        {
            for (int n = dcomp; n < dcomp+numcomp; ++n) {
                fab(i,j,k,n) = fab(i,j,dzlo,n);
            }
            f_user(IntVect(AMREX_D_DECL(i,j,k)), fab, dcomp, numcomp, geomdata, time,
                   bcr_p, 0, orig_comp);
        });
    }

    // zhi
    if (!geom.isPeriodic(2) && bx.bigEnd(2) > domain.bigEnd(2)) {
        Box bndry = bx;
        int dzhi = domain.bigEnd(2);
        bndry.setSmall(2,dzhi+1);
        amrex::ParallelFor(bndry, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
        {
            for (int n = dcomp; n < dcomp+numcomp; ++n) {
                fab(i,j,k,n) = fab(i,j,dzhi,n);
            }
            f_user(IntVect(AMREX_D_DECL(i,j,k)), fab, dcomp, numcomp, geomdata, time,
                   bcr_p, 0, orig_comp);
        });
    }
#endif
}

template <class F>
template <class FF>
void
GpuBndryFuncFab<F>::ccfcdoit (Box const& bx, FArrayBox& dest,
                              int dcomp, int numcomp,
                              Geometry const& geom, Real time,
                              const Vector<BCRec>& bcr, int bcomp,
                              int orig_comp, FF const& fillfunc)
{
    const IntVect& len = bx.length();

    IndexType idxType = bx.ixType();
    Box const& domain = amrex::convert(geom.Domain(),idxType);
    Box gdomain = domain;
    for (int idim = 0; idim < AMREX_SPACEDIM; ++idim) {
        if (geom.isPeriodic(idim)) {
            gdomain.grow(idim,len[idim]);
        }
    }

    if (gdomain.contains(bx)) { return; }

    Array4<Real> const& fab = dest.array();
    const auto geomdata = geom.data();

#ifdef AMREX_USE_GPU
    AsyncArray<BCRec> bcr_aa(bcr.data()+bcomp, numcomp);
    BCRec* bcr_p = bcr_aa.data();

    const auto f_user = m_user_f;

    // fill on the faces first
    {
        Array<Box,2*AMREX_SPACEDIM> dom_face_boxes
            = { AMREX_D_DECL(amrex::convert(amrex::adjCellLo(gdomain, 0, len[0]),idxType),
                             amrex::convert(amrex::adjCellLo(gdomain, 1, len[1]),idxType),
                             amrex::convert(amrex::adjCellLo(gdomain, 2, len[2]),idxType)),
                AMREX_D_DECL(amrex::convert(amrex::adjCellHi(gdomain, 0, len[0]),idxType),
                             amrex::convert(amrex::adjCellHi(gdomain, 1, len[1]),idxType),
                             amrex::convert(amrex::adjCellHi(gdomain, 2, len[2]),idxType)) };

        Vector<Box> face_boxes;
        for (const Box& b : dom_face_boxes) {
            Box tmp = b & bx;
            if (tmp.ok()) { face_boxes.push_back(tmp); }
        }
        const int n_face_boxes = face_boxes.size();
        if (n_face_boxes == 1) {
            amrex::ParallelFor(face_boxes[0],
            [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                AMREX_D_PICK(amrex::ignore_unused(j,k),amrex::ignore_unused(k),(void)0);
                IntVect const idx(AMREX_D_DECL(i,j,k));
                fillfunc(idx, fab, dcomp, numcomp, domain, bcr_p, 0);
                f_user(idx, fab, dcomp, numcomp, geomdata, time,
                       bcr_p, 0, orig_comp);
            });
        } else if (n_face_boxes > 1) {
            AsyncArray<Box> face_boxes_aa(face_boxes.data(), n_face_boxes);
            Box* boxes_p = face_boxes_aa.data();
            Long ncounts = 0;
            for (const auto& b : face_boxes) {
                ncounts += b.numPts();
            }
            amrex::ParallelFor(ncounts,
            [=] AMREX_GPU_DEVICE (Long icount) noexcept
            {
                const auto& idx = getCell(boxes_p, n_face_boxes, icount);
                fillfunc(idx, fab, dcomp, numcomp, domain, bcr_p, 0);
                f_user(idx, fab, dcomp, numcomp, geomdata, time,
                       bcr_p, 0, orig_comp);
            });
        }
    }

#if (AMREX_SPACEDIM >= 2)
    // fill on the box edges
    {
#if (AMREX_SPACEDIM == 2)
        Array<Box,4> dom_edge_boxes
            = {{ amrex::convert(amrex::adjCellLo(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),idxType) }};
#else
        Array<Box,12> dom_edge_boxes
            = {{ amrex::convert(amrex::adjCellLo(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),idxType),
                 //
                 amrex::convert(amrex::adjCellLo(amrex::adjCellLo(gdomain,0,len[0]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellHi(gdomain,0,len[0]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellLo(gdomain,0,len[0]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellHi(gdomain,0,len[0]),2,len[2]),idxType),
                 //
                 amrex::convert(amrex::adjCellLo(amrex::adjCellLo(gdomain,1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellHi(gdomain,1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellLo(gdomain,1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellHi(gdomain,1,len[1]),2,len[2]),idxType) }};
#endif
        Vector<Box> edge_boxes;
        for (const Box& b : dom_edge_boxes) {
            Box tmp = b & bx;
            if (tmp.ok()) { edge_boxes.push_back(tmp); }
        }
        const int n_edge_boxes = edge_boxes.size();
        if (n_edge_boxes == 1) {
            amrex::ParallelFor(edge_boxes[0],
            [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                AMREX_D_PICK(amrex::ignore_unused(j,k),amrex::ignore_unused(k),(void)0);
                IntVect const idx(AMREX_D_DECL(i,j,k));
                fillfunc(idx, fab, dcomp, numcomp, domain, bcr_p, 0);
                f_user(idx, fab, dcomp, numcomp, geomdata, time,
                       bcr_p, 0, orig_comp);
            });
        } else if (n_edge_boxes > 1) {
            AsyncArray<Box> edge_boxes_aa(edge_boxes.data(), n_edge_boxes);
            Box* boxes_p = edge_boxes_aa.data();
            Long ncounts = 0;
            for (const auto& b : edge_boxes) {
                ncounts += b.numPts();
            }
            amrex::ParallelFor(ncounts,
            [=] AMREX_GPU_DEVICE (Long icount) noexcept
            {
                const auto& idx = getCell(boxes_p, n_edge_boxes, icount);
                fillfunc(idx, fab, dcomp, numcomp, domain, bcr_p, 0);
                f_user(idx, fab, dcomp, numcomp, geomdata, time,
                       bcr_p, 0, orig_comp);
            });
        }
    }
#endif

#if (AMREX_SPACEDIM == 3)
    // fill on corners
    {
        Array<Box,8> dom_corner_boxes
            = {{ amrex::convert(amrex::adjCellLo(amrex::adjCellLo(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellLo(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellHi(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellHi(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellLo(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellLo(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellHi(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellHi(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType) }};

        Vector<Box> corner_boxes;
        for (const Box& b : dom_corner_boxes) {
            Box tmp = b & bx;
            if (tmp.ok()) { corner_boxes.push_back(tmp); }
        }
        const int n_corner_boxes = corner_boxes.size();
        if (n_corner_boxes == 1) {
            amrex::ParallelFor(corner_boxes[0],
            [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                IntVect const idx(AMREX_D_DECL(i,j,k));
                fillfunc(idx, fab, dcomp, numcomp, domain, bcr_p, 0);
                f_user(idx, fab, dcomp, numcomp, geomdata, time,
                       bcr_p, 0, orig_comp);
            });
        } else if (n_corner_boxes > 1) {
            AsyncArray<Box> corner_boxes_aa(corner_boxes.data(), n_corner_boxes);
            Box* boxes_p = corner_boxes_aa.data();
            Long ncounts = 0;
            for (const auto& b : corner_boxes) {
                ncounts += b.numPts();
            }
            amrex::ParallelFor(ncounts,
            [=] AMREX_GPU_DEVICE (Long icount) noexcept
            {
                const auto& idx = getCell(boxes_p, n_corner_boxes, icount);
                fillfunc(idx, fab, dcomp, numcomp, domain, bcr_p, 0);
                f_user(idx, fab, dcomp, numcomp, geomdata, time,
                       bcr_p, 0, orig_comp);
            });
        }
    }
#endif

#else
    BCRec const* bcr_p = bcr.data()+bcomp;

    const auto& f_user = m_user_f;

    // fill on the box faces first
    {
        Array<Box,2*AMREX_SPACEDIM> dom_face_boxes
            = {{ AMREX_D_DECL(amrex::convert(amrex::adjCellLo(gdomain, 0, len[0]),idxType),
                              amrex::convert(amrex::adjCellLo(gdomain, 1, len[1]),idxType),
                              amrex::convert(amrex::adjCellLo(gdomain, 2, len[2]),idxType)),
                 AMREX_D_DECL(amrex::convert(amrex::adjCellHi(gdomain, 0, len[0]),idxType),
                              amrex::convert(amrex::adjCellHi(gdomain, 1, len[1]),idxType),
                              amrex::convert(amrex::adjCellHi(gdomain, 2, len[2]),idxType)) }};

        for (const Box& b : dom_face_boxes) {
            Box tmp = b & bx;
            amrex::For(tmp, [=] (int i, int j, int k) noexcept
            {
                amrex::ignore_unused(j,k);
                IntVect const idx(AMREX_D_DECL(i,j,k));
                fillfunc(idx, fab, dcomp, numcomp, domain, bcr_p, 0);
                f_user(idx, fab, dcomp, numcomp, geomdata, time,
                       bcr_p, 0, orig_comp);
            });
        }
    }

#if (AMREX_SPACEDIM >= 2)
    // fill on the box edges
    {
#if (AMREX_SPACEDIM == 2)
        Array<Box,4> dom_edge_boxes
            = {{ amrex::convert(amrex::adjCellLo(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),idxType) }};
#else
        Array<Box,12> dom_edge_boxes
            = {{ amrex::convert(amrex::adjCellLo(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),idxType),
                 //
                 amrex::convert(amrex::adjCellLo(amrex::adjCellLo(gdomain,0,len[0]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellHi(gdomain,0,len[0]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellLo(gdomain,0,len[0]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellHi(gdomain,0,len[0]),2,len[2]),idxType),
                 //
                 amrex::convert(amrex::adjCellLo(amrex::adjCellLo(gdomain,1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellHi(gdomain,1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellLo(gdomain,1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellHi(gdomain,1,len[1]),2,len[2]),idxType) }};
#endif

        for (const Box& b : dom_edge_boxes) {
            Box tmp = b & bx;
            amrex::For(tmp, [=] (int i, int j, int k) noexcept
            {
                amrex::ignore_unused(j,k);
                IntVect const idx(AMREX_D_DECL(i,j,k));
                fillfunc(idx, fab, dcomp, numcomp, domain, bcr_p, 0);
                f_user(idx, fab, dcomp, numcomp, geomdata, time,
                       bcr_p, 0, orig_comp);
            });
        }
    }
#endif

#if (AMREX_SPACEDIM == 3)
    // fill on box corners
    {
        Array<Box,8> dom_corner_boxes
            = {{ amrex::convert(amrex::adjCellLo(amrex::adjCellLo(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellLo(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellHi(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellLo(amrex::adjCellHi(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellLo(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellLo(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellHi(amrex::adjCellLo(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType),
                 amrex::convert(amrex::adjCellHi(amrex::adjCellHi(amrex::adjCellHi(gdomain,0,len[0]),1,len[1]),2,len[2]),idxType) }};

        for (const Box& b : dom_corner_boxes) {
            Box tmp = b & bx;
            amrex::For(tmp, [=] (int i, int j, int k) noexcept
            {
                IntVect const idx(AMREX_D_DECL(i,j,k));
                fillfunc(idx, fab, dcomp, numcomp, domain, bcr_p, 0);
                f_user(idx, fab, dcomp, numcomp, geomdata, time,
                       bcr_p, 0, orig_comp);
            });
        }
    }
#endif

#endif
}

}
#endif
