#include <AMReX_TracerParticles.H>
#include <AMReX_TracerParticle_mod_K.H>
#include <AMReX_Print.H>
namespace amrex {

//
// Uses midpoint method to advance particles using umac.
//
void
TracerParticleContainer::AdvectWithUmac (MultiFab* umac, int lev, Real dt)
{
    BL_PROFILE("TracerParticleContainer::AdvectWithUmac()");
    AMREX_ASSERT(OK(lev, lev, umac[0].nGrow()-1));
    AMREX_ASSERT(lev >= 0 && lev < GetParticles().size());

    AMREX_ASSERT(AMREX_D_TERM(umac[0].nGrowVect().allGE(1),
                           && umac[1].nGrowVect().allGE(1),
                           && umac[2].nGrowVect().allGE(1)));

    AMREX_ASSERT(AMREX_D_TERM(!umac[0].contains_nan(),
                           && !umac[1].contains_nan(),
                           && !umac[2].contains_nan()));

    const auto      strttime = amrex::second();
    const Geometry& geom     = m_gdb->Geom(lev);
    const auto          plo      = geom.ProbLoArray();
    const auto          dxi      = geom.InvCellSizeArray();

    Vector<std::unique_ptr<MultiFab> > raii_umac(AMREX_SPACEDIM);
    Vector<MultiFab*> umac_pointer(AMREX_SPACEDIM);
    if (OnSameGrids(lev, umac[0]))
    {
        for (int i = 0; i < AMREX_SPACEDIM; i++) {
            umac_pointer[i] = &umac[i];
        }
    }
    else
    {
        for (int i = 0; i < AMREX_SPACEDIM; i++)
        {
            int ng = umac[i].nGrow();
            raii_umac[i] = std::make_unique<MultiFab>
                (amrex::convert(m_gdb->ParticleBoxArray(lev), IntVect::TheDimensionVector(i)),
                 m_gdb->ParticleDistributionMap(lev), umac[i].nComp(), ng);
            umac_pointer[i] = raii_umac[i].get();
            umac_pointer[i]->ParallelCopy(umac[i],0,0,umac[i].nComp(),ng,ng);
        }
    }

    for (int ipass = 0; ipass < 2; ipass++)
    {
#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        for (ParIterType pti(*this, lev); pti.isValid(); ++pti)
        {
            int grid    = pti.index();
            auto& ptile = ParticlesAt(lev, pti);
            auto& aos  = ptile.GetArrayOfStructs();
            const int n = aos.numParticles();
            auto *p_pbox = aos().data();
            const FArrayBox* fab[AMREX_SPACEDIM] = { AMREX_D_DECL(&((*umac_pointer[0])[grid]),
                                                                  &((*umac_pointer[1])[grid]),
                                                                  &((*umac_pointer[2])[grid])) };

            //array of these pointers to pass to the GPU
            amrex::GpuArray<amrex::Array4<const Real>, AMREX_SPACEDIM>
                const umacarr {{AMREX_D_DECL((*fab[0]).array(),
                                             (*fab[1]).array(),
                                             (*fab[2]).array() )}};

            amrex::ParallelFor(n,
                               [=] AMREX_GPU_DEVICE (int i)
            {
                ParticleType& p = p_pbox[i];
                if (p.id() <= 0) { return; }
                ParticleReal v[AMREX_SPACEDIM];
                mac_interpolate(p, plo, dxi, umacarr, v);
                if (ipass == 0)
                {
                    for (int dim=0; dim < AMREX_SPACEDIM; dim++)
                    {
                        p.rdata(dim) = p.pos(dim);
                        p.pos(dim) += static_cast<ParticleReal>(ParticleReal(0.5)*dt*v[dim]);
                    }
                }
                else
                {
                    for (int dim=0; dim < AMREX_SPACEDIM; dim++)
                    {
                        p.pos(dim) = p.rdata(dim) + static_cast<ParticleReal>(dt*v[dim]);
                        p.rdata(dim) = v[dim];
                    }
                }
            });
        }
    }

    if (m_verbose > 1)
    {
        auto stoptime = amrex::second() - strttime;

#ifdef AMREX_LAZY
        Lazy::QueueReduction( [=] () mutable {
#endif
                ParallelReduce::Max(stoptime, ParallelContext::IOProcessorNumberSub(),
                                    ParallelContext::CommunicatorSub());

                amrex::Print() << "TracerParticleContainer::AdvectWithUmac() time: " << stoptime << '\n';
#ifdef AMREX_LAZY
        });
#endif
    }
}

//
// Uses midpoint method to advance particles using cell-centered velocity
//
void
TracerParticleContainer::AdvectWithUcc (const MultiFab& Ucc, int lev, Real dt)
{
    BL_PROFILE("TracerParticleContainer::AdvectWithUcc()");
    AMREX_ASSERT(Ucc.nGrowVect().allGT(0));
    AMREX_ASSERT(OK(lev, lev, Ucc.nGrow()-1));
    AMREX_ASSERT(lev >= 0 && lev < GetParticles().size());
    AMREX_ASSERT(!Ucc.contains_nan());

    const auto          strttime = amrex::second();
    const Geometry&     geom     = m_gdb->Geom(lev);
    const auto          plo      = geom.ProbLoArray();
    const auto          dxi      = geom.InvCellSizeArray();

    AMREX_ASSERT(OnSameGrids(lev, Ucc));

    for (int ipass = 0; ipass < 2; ipass++)
    {
#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        for (ParIterType pti(*this, lev); pti.isValid(); ++pti)
        {
            int grid    = pti.index();
            auto& ptile = ParticlesAt(lev, pti);
            auto& aos  = ptile.GetArrayOfStructs();
            const int n          = aos.numParticles();
            const FArrayBox& fab = Ucc[grid];
            const auto uccarr = fab.array();
            auto *  p_pbox = aos().data();

            amrex::ParallelFor(n,
                               [=] AMREX_GPU_DEVICE (int i)
            {
                ParticleType& p  = p_pbox[i];
                if (p.id() <= 0) { return; }
                ParticleReal v[AMREX_SPACEDIM];

                cic_interpolate(p, plo, dxi, uccarr, v);

                if (ipass == 0)
                {
                    for (int dim=0; dim < AMREX_SPACEDIM; dim++)
                    {
                        p.rdata(dim) = p.pos(dim);
                        p.pos(dim) += static_cast<ParticleReal>(ParticleReal(0.5)*dt*v[dim]);
                    }
                }
                else
                {
                    for (int dim=0; dim < AMREX_SPACEDIM; dim++)
                    {
                        p.pos(dim) = p.rdata(dim) + static_cast<ParticleReal>(dt*v[dim]);
                        p.rdata(dim) = v[dim];
                    }
                }
            });
        }
    }

    if (m_verbose > 1)
    {
        auto stoptime = amrex::second() - strttime;

#ifdef AMREX_LAZY
        Lazy::QueueReduction( [=] () mutable {
#endif
                ParallelReduce::Max(stoptime, ParallelContext::IOProcessorNumberSub(),
                                    ParallelContext::CommunicatorSub());

                amrex::Print() << "TracerParticleContainer::AdvectWithUcc() time: " << stoptime << '\n';
#ifdef AMREX_LAZY
            });
#endif
    }
}

void
TracerParticleContainer::Timestamp (const std::string&      basename,
                                    const MultiFab&         mf,
                                    int                     lev,
                                    Real                    time,
                                    const std::vector<int>& indices)
{
    BL_PROFILE("TracerParticleContainer::Timestamp()");
    //
    // basename -> base filename for the output file
    // mf       -> the multifab
    // lev      -> level to check for particles
    // time     -> simulation time (will be recorded in Timestamp file)
    // indices  -> indices into mf that we output
    //
    AMREX_ASSERT(lev >= 0);
    AMREX_ASSERT(time >= 0);
    AMREX_ASSERT(!basename.empty());
    AMREX_ASSERT(lev <= m_gdb->finestLevel());

    const auto strttime = amrex::second();
    const Geometry& geom     = m_gdb->Geom(lev);
    const auto      plo      = geom.ProbLoArray();
    const auto      dxi      = geom.InvCellSizeArray();

    const int   MyProc    = ParallelDescriptor::MyProc();
    const int   NProcs    = ParallelContext::NProcsSub();
    // We'll spread the output over this many files.
    int nOutFiles(64);
    ParmParse pp("particles");
    pp.query("particles_nfiles",nOutFiles);
    if(nOutFiles == -1) {
      nOutFiles = NProcs;
    }
    nOutFiles = std::max(1, std::min(nOutFiles,NProcs));
    const int   nSets     = ((NProcs + (nOutFiles - 1)) / nOutFiles);
    const int   mySet     = (MyProc / nOutFiles);

    for (int iSet = 0; iSet < nSets; ++iSet)
      {
        if (mySet == iSet)
          {
            //
            // Do we have any particles at this level that need writing?
            //
            bool gotwork = NumberOfParticlesAtLevel(lev, true, true) > 0;

            if (gotwork)
              {
                std::string FileName = amrex::Concatenate(basename + '_', MyProc % nOutFiles, 2);

                VisMF::IO_Buffer io_buffer(VisMF::IO_Buffer_Size);

                std::ofstream TimeStampFile;

                TimeStampFile.rdbuf()->pubsetbuf(io_buffer.dataPtr(), io_buffer.size());

                TimeStampFile.open(FileName.c_str(), std::ios::out|std::ios::app|std::ios::binary);

                TimeStampFile.setf(std::ios_base::scientific,std::ios_base::floatfield);

                TimeStampFile.precision(10);

                TimeStampFile.seekp(0, std::ios::end);

                if (!TimeStampFile.good()) {
                    amrex::FileOpenFailed(FileName);
                }

                const auto M  = static_cast<int>(indices.size());
                const BoxArray& ba = mf.boxArray();

                std::vector<ParticleReal> vals(M);

                const auto& pmap = GetParticles(lev);
                for (const auto& kv : pmap) {
                  using PinnedTile = amrex::ParticleTile<Particle<AMREX_SPACEDIM, 0>, 0, 0,
                                                         amrex::PinnedArenaAllocator>;
                  PinnedTile pinned_tile;
                  pinned_tile.define(NumRuntimeRealComps(), NumRuntimeIntComps());
                  pinned_tile.resize(kv.second.numParticles());
                  amrex::copyParticles(pinned_tile, kv.second);

                  int grid = kv.first.first;
                  const auto& pbox = pinned_tile.GetArrayOfStructs();
                  const Box&       bx   = ba[grid];
                  const FArrayBox& fab  = mf[grid];
                  auto uccarr = fab.array();

#ifdef AMREX_USE_GPU
                  std::unique_ptr<FArrayBox> hostfab;
                  {
                      hostfab = std::make_unique<FArrayBox>(fab.box(), fab.nComp(),
                                                            The_Pinned_Arena());
                      Gpu::dtoh_memcpy_async(hostfab->dataPtr(), fab.dataPtr(),
                                             fab.size()*sizeof(Real));
                      Gpu::streamSynchronize();
                      uccarr = hostfab->const_array();
                  }
#endif

                  for (int k = 0; k < pbox.numParticles(); ++k)
                    {
                      const ParticleType& p = pbox[k];

                      if (p.id() <= 0) { continue; }

                      const IntVect& iv = Index(p,lev);

                      if (!bx.contains(iv) && !ba.contains(iv)) { continue; }

                      TimeStampFile << p.id()  << ' ' << p.cpu() << ' ';

                      AMREX_D_TERM(TimeStampFile << p.pos(0) << ' ';,
                                   TimeStampFile << p.pos(1) << ' ';,
                                   TimeStampFile << p.pos(2) << ' ';);

                      TimeStampFile << time;
                      //
                      // AdvectWithUmac stores the velocity in rdata ...
                      //
                      AMREX_D_TERM(TimeStampFile << ' ' << p.rdata(0);,
                                   TimeStampFile << ' ' << p.rdata(1);,
                                   TimeStampFile << ' ' << p.rdata(2););

                      if (M > 0)
                        {
                          cic_interpolate(p, plo, dxi, uccarr, vals.data(), M);

                          for (int i = 0; i < M; i++)
                            {
                              TimeStampFile << ' ' << vals[i];
                            }
                        }

                      TimeStampFile << '\n';
                    }
                }

                TimeStampFile.flush();
                TimeStampFile.close();
            }

            const int iBuff     = 0;
            const int wakeUpPID = (MyProc + nOutFiles);
            const int tag       = (MyProc % nOutFiles);

            if (wakeUpPID < NProcs) {
                ParallelDescriptor::Send(&iBuff, 1, wakeUpPID, tag);
            }
        }
        if (mySet == (iSet + 1))
        {
            //
            // Next set waits.
            //
            int       iBuff;
            const int waitForPID = (MyProc - nOutFiles);
            const int tag        = (MyProc % nOutFiles);

            ParallelDescriptor::Recv(&iBuff, 1, waitForPID, tag);
        }
    }

    if (m_verbose > 1)
    {
        auto stoptime = amrex::second() - strttime;

#ifdef AMREX_LAZY
        Lazy::QueueReduction( [=] () mutable {
#endif
        ParallelDescriptor::ReduceRealMax(stoptime,ParallelDescriptor::IOProcessorNumber());
        amrex::Print() << "TracerParticleContainer::Timestamp: lev: " << lev << " time: " << stoptime << '\n';
#ifdef AMREX_LAZY
        });
#endif
   }
}
}
