#ifndef AMREX_MLNODELAP_1D_K_H_
#define AMREX_MLNODELAP_1D_K_H_
#include <AMReX_Config.H>

namespace amrex {

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_zero_fine (int i, int, int, Array4<Real> const& phi,
                        Array4<int const> const& msk, int fine_flag) noexcept
{
    // Testing if the node is covered by a fine level in computing
    // coarse sync residual
    if (msk(i-1,0,0) == fine_flag &&
        msk(i  ,0,0) == fine_flag)
    {
        phi(i,0,0) = Real(0.0);
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_avgdown_coeff_x (int i, int, int, Array4<Real> const& crse,
                              Array4<Real const> const& fine) noexcept
{
    Real a = fine(2*i  ,0,0);
    Real b = fine(2*i+1,0,0);
    crse(i,0,0) = Real(2.0)*a*b/(a+b);
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_semi_avgdown_coeff (int i, int j, int k, Array4<Real> const& crse,
                                 Array4<Real const> const& fine, int) noexcept
{
    mlndlap_avgdown_coeff_x(i,j,k,crse,fine);
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real mlndlap_adotx_c (int i, int, int, Array4<Real const> const& x,
                      Real sigma, Array4<int const> const& msk,
                      GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    if (msk(i,0,0)) {
        return Real(0.0);
    } else {
        Real y  = x(i-1,0,0)
               +  x(i+1,0,0)
               +  x(i  ,0,0)*Real(-2.0);
        return dxinv[0]*dxinv[0] * y * sigma;
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real mlndlap_adotx_ha (int i, int, int, Array4<Real const> const& x,
                       Array4<Real const> const& sx, Array4<int const> const& msk,
                       GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    if (msk(i,0,0)) {
        return Real(0.0);
    } else {
        Real y = (x(i+1,0,0) - x(i  ,0,0)) * sx(i  ,0,0)
            -    (x(i  ,0,0) - x(i-1,0,0)) * sx(i-1,0,0);
        return dxinv[0]*dxinv[0] * y;
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real mlndlap_adotx_aa (int i, int j, int k, Array4<Real const> const& x,
                       Array4<Real const> const& sx, Array4<int const> const& msk,
                       GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    return mlndlap_adotx_ha(i,j,k,x,sx,msk,dxinv);
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_normalize_ha (int i, int, int, Array4<Real> const& x,
                           Array4<Real const> const& sx, Array4<int const> const& msk,
                           GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    if (!msk(i,0,0)) {
        x(i,0,0) /= -dxinv[0]*dxinv[0] * (sx(i-1,0,0)+sx(i,0,0));
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_normalize_aa (int i, int j, int k, Array4<Real> const& x,
                           Array4<Real const> const& sx, Array4<int const> const& msk,
                           GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    mlndlap_normalize_ha(i,j,k,x,sx,msk,dxinv);
}

AMREX_GPU_DEVICE AMREX_FORCE_INLINE
void mlndlap_jacobi_ha (int i, int, int, Array4<Real> const& sol, Real Ax,
                        Array4<Real const> const& rhs, Array4<Real const> const& sx,
                        Array4<int const> const& msk,
                        GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    if (msk(i,0,0)) {
        sol(i,0,0) = Real(0.0);
    } else {
        sol(i,0,0) += Real(2.0/3.0) * (rhs(i,0,0) - Ax)
            / (-dxinv[0]*dxinv[0] * (sx(i-1,0,0)+sx(i,0,0)));
    }
}

inline
void mlndlap_jacobi_ha (Box const& bx, Array4<Real> const& sol, Array4<Real const> const& Ax,
                        Array4<Real const> const& rhs, Array4<Real const> const& sx,
                        Array4<int const> const& msk,
                        GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    Real fac = -dxinv[0]*dxinv[0];

    amrex::LoopConcurrentOnCpu(bx, [&] (int i, int, int) noexcept
    {
        if (msk(i,0,0)) {
            sol(i,0,0) = Real(0.0);
        } else {
            sol(i,0,0) += Real(2.0/3.0) * (rhs(i,0,0) - Ax(i,0,0))
                / (fac * (sx(i-1,0,0)+sx(i,0,0)));
        }
    });
}

AMREX_GPU_DEVICE AMREX_FORCE_INLINE
void mlndlap_jacobi_aa (int i, int j, int k, Array4<Real> const& sol, Real Ax,
                        Array4<Real const> const& rhs, Array4<Real const> const& sig,
                        Array4<int const> const& msk, GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    mlndlap_jacobi_ha(i,j,k,sol,Ax,rhs,sig,msk,dxinv);
}

inline
void mlndlap_jacobi_aa (Box const& bx, Array4<Real> const& sol, Array4<Real const> const& Ax,
                        Array4<Real const> const& rhs, Array4<Real const> const& sig,
                        Array4<int const> const& msk, GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    mlndlap_jacobi_ha(bx,sol,Ax,rhs,sig,msk,dxinv);
}

AMREX_GPU_DEVICE AMREX_FORCE_INLINE
void mlndlap_jacobi_c (int i, int, int, Array4<Real> const& sol, Real Ax,
                       Array4<Real const> const& rhs, Real sig,
                       Array4<int const> const& msk, GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    if (msk(i,0,0)) {
        sol(i,0,0) = Real(0.0);
    } else {
        sol(i,0,0) += Real(2.0/3.0) * (rhs(i,0,0) - Ax)
            / (-dxinv[0]*dxinv[0]*Real(2.0)*sig);
    }
}

inline
void mlndlap_jacobi_c (Box const& bx, Array4<Real> const& sol, Array4<Real const> const& Ax,
                       Array4<Real const> const& rhs, Real sig,
                       Array4<int const> const& msk, GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    amrex::LoopConcurrentOnCpu(bx, [&] (int i, int, int) noexcept
    {
        if (msk(i,0,0)) {
            sol(i,0,0) = Real(0.0);
        } else {
            sol(i,0,0) += Real(2.0/3.0) * (rhs(i,0,0) - Ax(i,0,0))
                / (-dxinv[0]*dxinv[0]*Real(2.0)*sig);
        }
    });
}

inline
void mlndlap_gauss_seidel_ha (Box const& bx, Array4<Real> const& sol,
                              Array4<Real const> const& rhs,
                              Array4<Real const> const& sx,
                              Array4<int const> const& msk,
                              GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    Real fac = dxinv[0]*dxinv[0];

    amrex::LoopOnCpu(bx, [&] (int i, int, int) noexcept
    {
        if (msk(i,0,0)) {
            sol(i,0,0) = Real(0.0);
        } else {
            Real s0 = Real(-1.0) * fac * (sx(i-1,0,0)+sx(i,0,0));
            Real Ax = sol(i-1,0,0)*fac*sx(i-1,0,0)
                +     sol(i+1,0,0)*fac*sx(i  ,0,0)
                +     sol(i  ,0,0)*s0;
            sol(i,0,0) += (rhs(i,0,0) - Ax) / s0;
        }
    });
}

inline
void mlndlap_gauss_seidel_aa (Box const& bx, Array4<Real> const& sol,
                              Array4<Real const> const& rhs,
                              Array4<Real const> const& sx,
                              Array4<int const> const& msk,
                              GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    mlndlap_gauss_seidel_ha(bx,sol,rhs,sx,msk,dxinv);
}

inline
void mlndlap_gauss_seidel_c (Box const& bx, Array4<Real> const& sol,
                             Array4<Real const> const& rhs, Real sig,
                             Array4<int const> const& msk,
                             GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    Real fac = dxinv[0]*dxinv[0];

    amrex::LoopOnCpu(bx, [&] (int i, int, int) noexcept
    {
        if (msk(i,0,0)) {
            sol(i,0,0) = Real(0.0);
        } else {
            Real s0 = Real(-2.0) * fac * sig;
            Real Ax = sol(i-1,0,0)*fac*sig
                +     sol(i+1,0,0)*fac*sig
                +     sol(i  ,0,0)*s0;
            sol(i,0,0) += (rhs(i,0,0) - Ax) / s0;
        }
    });
}

inline
void mlndlap_gauss_seidel_with_line_solve_aa(Box const&, Array4<Real> const&,
                                             Array4<Real const> const&, Array4<Real const> const&,
                                             Array4<int const> const&, GpuArray<Real,AMREX_SPACEDIM> const&) noexcept
{
    amrex::Abort("mlndlap_gauss_seidel_with_line_solve_aa: not implemented in 1D");
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_interpadd_c (int i, int , int, Array4<Real> const& fine,
                          Array4<Real const> const& crse,
                          Array4<int const> const& msk) noexcept
{
    if (!msk(i,0,0)) {
        int ic = amrex::coarsen(i,2);
        bool i_is_odd = (ic*2 != i);
        if (i_is_odd) {
            // Node on X line
            fine(i,0,0) += Real(0.5) * (crse(ic,0,0)+crse(ic+1,0,0));
        } else {
            // Node coincident with coarse node
            fine(i,0,0) += crse(ic,0,0);
        }
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_interpadd_aa (int i, int , int, Array4<Real> const& fine,
                           Array4<Real const> const& crse, Array4<Real const> const& sig,
                           Array4<int const> const& msk) noexcept
{
    if (!msk(i,0,0)) {
        int ic = amrex::coarsen(i,2);
        bool i_is_odd = (ic*2 != i);
        if (i_is_odd) {
            // Node on X line
            fine(i,0,0) += (sig(i-1,0,0)*crse(ic,0,0) + sig(i,0,0)*crse(ic+1,0,0))
                / (sig(i-1,0,0) + sig(i,0,0));
        } else {
            // Node coincident with coarse node
            fine(i,0,0) += crse(ic,0,0);
        }
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_semi_interpadd_aa (int /*i*/, int /*j*/, int /*k*/, Array4<Real> const&,
                           Array4<Real const> const&, Array4<Real const> const&,
                           Array4<int const> const&, int) noexcept
{}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_interpadd_ha (int i, int j, int k, Array4<Real> const& fine,
                           Array4<Real const> const& crse, Array4<Real const> const& sig,
                           Array4<int const> const& msk) noexcept
{
    mlndlap_interpadd_aa(i,j,k,fine,crse,sig,msk);
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_divu (int i, int, int, Array4<Real> const& rhs, Array4<Real const> const& vel,
                   Array4<int const> const& msk, GpuArray<Real,AMREX_SPACEDIM> const& dxinv,
                   Box const&,
                   GpuArray<LinOpBCType, AMREX_SPACEDIM> const&,
                   GpuArray<LinOpBCType, AMREX_SPACEDIM> const&) noexcept
{
    Real fac = dxinv[0];
    if (msk(i,0,0)) {
        rhs(i,0,0) = Real(0.0);
    } else {
        rhs(i,0,0) = fac*(vel(i,0,0)-vel(i-1,0,0));
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real mlndlap_rhcc (int i, int, int, Array4<Real const> const& rhcc,
                   Array4<int const> const& msk) noexcept
{
    Real r;
    if (msk(i,0,0)) {
        r = Real(0.0);
    } else {
        r = Real(0.5) * (rhcc(i-1,0,0)+rhcc(i,0,0));
    }
    return r;
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_mknewu (int i, int, int, Array4<Real> const& u,
                     Array4<Real const> const& p,
                     Array4<Real const> const& sig,
                     GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    Real fac = dxinv[0];
    u(i,0,0) -= sig(i,0,0)*fac*(p(i+1,0,0)-p(i,0,0));
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_mknewu_c (int i, int, int, Array4<Real> const& u,
                       Array4<Real const> const& p,
                       Real sig, GpuArray<Real,AMREX_SPACEDIM> const& dxinv) noexcept
{
    Real fac = dxinv[0];
    u(i,0,0) -= sig*fac*(p(i+1,0,0)-p(i,0,0));
}

template <int rr>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_rhcc_fine_contrib (int /*i*/, int /*j*/, int /*k*/, Box const&,
                                Array4<Real> const&, Array4<Real const> const&,
                                Array4<int const> const&) noexcept
{}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_divu_cf_contrib (int, int, int, Array4<Real> const&,
                              Array4<Real const> const&, Array4<Real const> const&,
                              Array4<Real const> const&, Array4<int const> const&,
                              Array4<int const> const&, Array4<int const> const&,
                              GpuArray<Real,AMREX_SPACEDIM> const&,
                              Box const&, GpuArray<LinOpBCType,AMREX_SPACEDIM> const&,
                              GpuArray<LinOpBCType,AMREX_SPACEDIM> const&,
                              bool) noexcept
{}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_crse_resid (int /*i*/, int /*j*/, int /*k*/, Array4<Real> const&,
                         Array4<Real const> const&, Array4<int const> const&,
                         Box const&, GpuArray<LinOpBCType,AMREX_SPACEDIM> const&,
                         GpuArray<LinOpBCType,AMREX_SPACEDIM> const&,
                         bool) noexcept
{}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_res_cf_contrib (int /*i*/, int /*j*/, int /*k*/, Array4<Real> const&,
                             Array4<Real const> const&, Array4<Real const> const&,
                             Array4<Real const> const&, Array4<int const> const&,
                             Array4<int const> const&, Array4<int const> const&,
                             Array4<Real const> const&,
                             GpuArray<Real,AMREX_SPACEDIM> const&, Box const&,
                             GpuArray<LinOpBCType, AMREX_SPACEDIM> const&,
                             GpuArray<LinOpBCType, AMREX_SPACEDIM> const&,
                             bool) noexcept
{}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_res_cf_contrib_cs (int /*i*/, int /*j*/, int /*k*/, Array4<Real> const&,
                                Array4<Real const> const&, Array4<Real const> const&,
                                Real, Array4<int const> const&,
                                Array4<int const> const&, Array4<int const> const&,
                                Array4<Real const> const&,
                                GpuArray<Real,AMREX_SPACEDIM> const&, Box const&,
                                GpuArray<LinOpBCType, AMREX_SPACEDIM> const&,
                                GpuArray<LinOpBCType, AMREX_SPACEDIM> const&,
                                bool) noexcept
{}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_set_stencil (Box const&, Array4<Real> const&,
                          Array4<Real const> const&,
                          GpuArray<Real,AMREX_SPACEDIM> const&) noexcept
{}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_set_stencil_s0 (int /*i*/, int /*j*/, int /*k*/, Array4<Real> const&) noexcept
{}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_stencil_rap (int /*i*/, int /*j*/, int /*k*/, Array4<Real> const&,
                          Array4<Real const> const&) noexcept
{}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real mlndlap_adotx_sten (int /*i*/, int /*j*/, int /*k*/, Array4<Real const> const&,
                         Array4<Real const> const&, Array4<int const> const&) noexcept
{ return Real(0.0); }

inline
void mlndlap_gauss_seidel_sten (Box const&, Array4<Real> const&,
                                Array4<Real const> const&,
                                Array4<Real const> const&,
                                Array4<int const> const&) noexcept
{}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_interpadd_rap (int /*i*/, int /*j*/, int /*k*/, Array4<Real> const&,
                            Array4<Real const> const&, Array4<Real const> const&,
                            Array4<int const> const&) noexcept
{}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_restriction_rap (int /*i*/, int /*j*/, int /*k*/, Array4<Real> const&,
                              Array4<Real const> const&, Array4<Real const> const&,
                              Array4<int const> const&) noexcept
{}

AMREX_GPU_DEVICE AMREX_FORCE_INLINE
int mlndlap_color (int i, int, int)
{
    return i%2;
}

AMREX_GPU_DEVICE AMREX_FORCE_INLINE
void mlndlap_gscolor_ha (int i, int j, int k, Array4<Real> const& sol,
                         Array4<Real const> const& rhs,
                         Array4<Real const> const& sx,
                         Array4<int const> const& msk,
                         GpuArray<Real,AMREX_SPACEDIM> const& dxinv, int color) noexcept
{
    if (mlndlap_color(i,j,k) == color) {
        if (msk(i,0,0)) {
            sol(i,0,0) = Real(0.0);
        } else {
            Real fac = dxinv[0]*dxinv[0];

            Real s0 = Real(-1.0) * fac * (sx(i-1,0,0)+sx(i,0,0));
            Real Ax = sol(i-1,0,0)*fac*sx(i-1,0,0)
                +     sol(i+1,0,0)*fac*sx(i  ,0,0)
                +     sol(i  ,0,0)*s0;
            sol(i,0,0) += (rhs(i,0,0) - Ax) / s0;
        }
    }
}

AMREX_GPU_DEVICE AMREX_FORCE_INLINE
void mlndlap_gscolor_aa (int i, int j, int k, Array4<Real> const& sol,
                         Array4<Real const> const& rhs,
                         Array4<Real const> const& sx,
                         Array4<int const> const& msk,
                         GpuArray<Real,AMREX_SPACEDIM> const& dxinv, int color) noexcept
{
    mlndlap_gscolor_ha(i,j,k,sol,rhs,sx,msk,dxinv,color);
}

AMREX_GPU_DEVICE AMREX_FORCE_INLINE
void mlndlap_gscolor_c (int i, int j, int k, Array4<Real> const& sol,
                        Array4<Real const> const& rhs, Real sig,
                        Array4<int const> const& msk,
                        GpuArray<Real,AMREX_SPACEDIM> const& dxinv, int color) noexcept
{
    if (mlndlap_color(i,j,k) == color) {
        if (msk(i,0,0)) {
            sol(i,0,0) = Real(0.0);
        } else {
            Real fac = dxinv[0]*dxinv[0];

            Real s0 = Real(-2.0) * fac * sig;
            Real Ax = sol(i-1,0,0)*fac*sig
                +     sol(i+1,0,0)*fac*sig
                +     sol(i  ,0,0)*s0;
            sol(i,0,0) += (rhs(i,0,0) - Ax) / s0;
        }
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlndlap_gscolor_sten (int, int, int, Array4<Real> const&,
                           Array4<Real const> const&,
                           Array4<Real const> const&,
                           Array4<int const> const&, int) noexcept
{}

}

#endif
