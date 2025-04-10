#ifndef _NyxParticleContainer_H_
#define _NyxParticleContainer_H_

#include <AMReX_Amr.H>
#include <AMReX_AmrLevel.H>
#include <AMReX_NeighborParticles.H>
#include <AMReX_AmrParticles.H>

class NyxParticleContainerBase
{
public:

    virtual ~NyxParticleContainerBase() {}

    virtual void moveKickDrift (amrex::MultiFab& acceleration, int level, amrex::Real timestep,
                        amrex::Real a_old = 1.0, amrex::Real a_half = 1.0, int where_width = 0) = 0;
    virtual void moveKick      (amrex::MultiFab& acceleration, int level, amrex::Real timestep,
                                amrex::Real a_new = 1.0, amrex::Real a_half = 1.0) = 0;
    virtual void Redistribute (int lev_min              = 0,
                               int lev_max              =-1,
                               int nGrow                = 0) = 0;

    virtual void RedistributeLocal (int lev_min              = 0,
                                    int lev_max              =-1,
                                    int nGrow                = 0) = 0;

    virtual void RedistributeOK    (int lev_min              = 0,
                                    int lev_max              =-1,
                                    int nGrow                = 0) = 0;

    virtual void RedistributeGPU   (int lev_min              = 0,
                                    int lev_max              =-1,
                                    int nGrow                = 0) = 0;

    virtual int finestLevel() const = 0;
    virtual void ShrinkToFit() = 0;
    virtual void RemoveParticlesAtLevel (int level) = 0;
    virtual amrex::Real sumParticleMass (int level) const = 0;
    virtual void Regrid  (const amrex::DistributionMapping& dmap, const amrex::BoxArray& ba, const int lev) = 0;
  //  void RegridHat (const amrex::DistributionMapping& dmap, const amrex::BoxArray& ba, const int lev);
  virtual amrex::Vector<long> NumberOfParticlesInGrid (int lev, bool only_valid, bool only_local) = 0;

    virtual void AssignDensitySingleLevel (amrex::MultiFab& mf, int level, int ncomp=1,
                                           int particle_lvl_offset = 0) const = 0;
    virtual void AssignDensity (amrex::Vector<std::unique_ptr<amrex::MultiFab> >& mf, int lev_min = 0, int ncomp = 1,
                                int finest_level = -1, int ngrow = 1) const = 0;
};

template <int NSR, int NSI=0, int NAR=0, int NAI=0>
class NyxParticleContainer
    : public amrex::NeighborParticleContainer<NSR,NSI>,
      public NyxParticleContainerBase
{
public:

    using ParticleTileType = amrex::ParticleTile<amrex::Particle<NSR,NSI>,NAR,NAI>;
    using MyParIter = amrex::ParIter<NSR,NSI,NAR,NAI>;
    using MyConstParIter = amrex::ParConstIter<NSR,NSI,NAR,NAI>;

    NyxParticleContainer (amrex::Amr* amr, int nghost=0)
        : amrex::NeighborParticleContainer<NSR,NSI>((amrex::ParGDBBase*) amr->GetParGDB(), nghost),
          sub_cycle(amr->subCycle())
    {}

    virtual ~NyxParticleContainer () {}

    void GetParticleVelocities (amrex::Vector<amrex::Real>& part_vels);
    void SetParticleVelocities (amrex::Vector<amrex::Real>& part_data);

    virtual amrex::Real sumParticleMass (int level) const override {
        using PType = typename amrex::ParticleContainer<NSR, NSI>::SuperParticleType;
        auto msum = amrex::ReduceSum(*this, level,
                                     [=] AMREX_GPU_HOST_DEVICE (const PType& p) -> amrex::Real
                                     {
                                         return (p.id() > 0) ? p.rdata(0) : 0.0;
                                     });
        amrex::ParallelAllReduce::Sum(msum, amrex::ParallelContext::CommunicatorSub());
        return msum;
    }

    virtual void Regrid (const amrex::DistributionMapping& dmap, const amrex::BoxArray& ba, const int lev) override {
          amrex::NeighborParticleContainer<NSR,NSI>::Regrid(dmap, ba, lev);
    }

  virtual amrex::Vector<long> NumberOfParticlesInGrid (int lev, bool only_valid, bool only_local) override {
          return amrex::NeighborParticleContainer<NSR,NSI>::NumberOfParticlesInGrid(lev,only_valid,only_local);
  }
    void sumParticleMomentum (int lev, amrex::Real* mom) const;

    virtual void AssignDensitySingleLevel (amrex::MultiFab& mf, int level, int ncomp=1, int particle_lvl_offset = 0) const override
    {
        amrex::NeighborParticleContainer<NSR,NSI>::AssignCellDensitySingleLevel(0, mf, level, ncomp, particle_lvl_offset);
    }
    virtual void AssignDensity (amrex::Vector<std::unique_ptr<amrex::MultiFab> >& mf, int lev_min = 0, int ncomp = 1, int finest_level = -1, int ngrow = 1) const override
    {
        amrex::NeighborParticleContainer<NSR,NSI>::AssignDensity(0, mf, lev_min, ncomp, finest_level, ngrow);
    }

    void MultiplyParticleMass (int lev, amrex::Real mult);

    amrex::Real estTimestep (amrex::MultiFab& acceleration,                int level, amrex::Real cfl) const;
    amrex::Real estTimestep (amrex::MultiFab& acceleration, amrex::Real a, int level, amrex::Real cfl) const;

    virtual int finestLevel() const override
    {
        return amrex::NeighborParticleContainer<NSR,NSI>::finestLevel();
    }

    virtual void ShrinkToFit() override
    {
        amrex::NeighborParticleContainer<NSR,NSI>::ShrinkToFit();
    }

    virtual void Redistribute (int lev_min              = 0,
                               int lev_max              =-1,
                               int nGrow                = 0) override
    {
      //        amrex::Gpu::synchronize();
        amrex::NeighborParticleContainer<NSR,NSI>::Redistribute(lev_min, lev_max, nGrow);
        //        amrex::Gpu::synchronize();
    }

    virtual void RedistributeLocal (int /* lev_min              = 0*/,
                                    int /* lev_max              =-1*/,
                                    int /* nGrow                = 0*/) override
    {
        AMREX_ASSERT(this->finestLevel() == 0);

        const int local = true;
        const int lev_minal = 0;
        const int lev_maxal = 0;
        const int nGrowal = 0;

        amrex::NeighborParticleContainer<NSR,NSI>
            ::Redistribute(lev_minal, lev_maxal, nGrowal, local);
    }

    virtual void RedistributeOK    (int lev_min              = 0,
                                    int lev_max              =-1,
                                    int nGrow                = 0) override
    {

        const int local =         amrex::NeighborParticleContainer<NSR,NSI>
          ::OK(lev_min, lev_max);
        //      amrex::Print()<<"local flag is: "<<local<<std::endl;
        amrex::NeighborParticleContainer<NSR,NSI>
            ::Redistribute(lev_min, lev_max, nGrow, local);

    }

    virtual void RedistributeGPU   (int /*lev_min              = 0*/,
                                    int /*lev_max              =-1*/,
                                    int /*nGrow                = 0*/) override
    {
        AMREX_ASSERT(this->finestLevel() == 0);

        int local = false;
        const int lev_minal = 0;
        const int lev_maxal = 0;
        const int nGrowal = 0;

        amrex::Gpu::synchronize();
        local =         amrex::NeighborParticleContainer<NSR,NSI>
          ::OK(lev_minal, lev_maxal);
        //      amrex::Print()<<"local flag is: "<<local<<std::endl;
        if(local)
        amrex::NeighborParticleContainer<NSR,NSI>
            ::RedistributeGPU(lev_minal, lev_maxal, nGrowal, local);
        else
        amrex::NeighborParticleContainer<NSR,NSI>
            ::RedistributeCPU(lev_minal, lev_maxal, nGrowal, local);
    }

    virtual void RemoveParticlesAtLevel (int level) override
    {
        amrex::NeighborParticleContainer<NSR,NSI>::RemoveParticlesAtLevel(level);
    }

    virtual void WriteNyxPlotFile (const std::string& dir,
                                   const std::string& name) const;

    virtual void NyxCheckpoint (const std::string& dir,
                                const std::string& name) const;

    typedef amrex::Particle<NSR,NSI> ParticleType;
    using AoS = typename amrex::ParticleContainer<NSR,NSI,NAR,NAI>::AoS;
    using ParticleLevel = typename amrex::ParticleContainer<NSR,NSI,NAR,NAI>::ParticleLevel;

protected:
    bool sub_cycle;
  amrex::Vector<std::string> real_comp_names;
};

template <int NSR,int NSI,int NAR,int NAI>
void
NyxParticleContainer<NSR,NSI,NAR,NAI>::GetParticleVelocities (amrex::Vector<amrex::Real>& part_data)
{
    BL_PROFILE("NyxParticleContainer<NSR,NSI,NAR,NAI>::GetParticleVelocities()");
    // This assumes that the mass/charge is stored in the first position
    //      in the particle data, followed by the velocity components
    int start_comp = 1;
    int   num_comp = AMREX_SPACEDIM;
    this->GetParticleData(part_data,1,AMREX_SPACEDIM);
}

template <int NSR,int NSI,int NAR,int NAI>
void
NyxParticleContainer<NSR,NSI,NAR,NAI>::SetParticleVelocities (amrex::Vector<amrex::Real>& part_data)
{
   BL_PROFILE("NyxParticleContainer<NSR,NSI,NAR,NSI>::SetParticleVelocities()");
   // This gives us the starting point into the part_data array
   // If only one processor (or no MPI), then that's all we need
   int cnt = 0;

#ifdef BL_USE_MPI
   amrex::Vector<long> cnts(amrex::ParallelDescriptor::NProcs());

   // This returns the number of particles on this processor
   long lcnt = this->TotalNumberOfParticles(true,true);

   // This accumulates the "lcnt" values into "cnts"
   MPI_Gather(&lcnt,1,
              amrex::ParallelDescriptor::Mpi_typemap<long>::type(),
              cnts.dataPtr(),
              1,
              amrex::ParallelDescriptor::Mpi_typemap<long>::type(),
              amrex::ParallelDescriptor::IOProcessorNumber(),
              amrex::ParallelDescriptor::Communicator());

   amrex::ParallelDescriptor::Bcast(cnts.dataPtr(), cnts.size(), amrex::ParallelDescriptor::IOProcessorNumber());

   for (int iproc = 0; iproc < amrex::ParallelDescriptor::MyProc(); iproc++)
       cnt += cnts[iproc];

   // Each particle takes up (AMREX_SPACEDIM) Reals
   cnt*= (AMREX_SPACEDIM);
#endif

   // This is the total number of particles on *all* processors
   long npart = this->TotalNumberOfParticles(true,false);

   // Velocities
   if (part_data.size() != npart*(AMREX_SPACEDIM))
       amrex::Abort("Sending in wrong size part_data to SetParticleVelocities");

   for (int lev = 0; lev <= this->m_gdb->finestLevel(); lev++)
   {
       ParticleLevel& pmap = this->GetParticles(lev);

       for (typename ParticleLevel::iterator pmap_it = pmap.begin(), pmapEnd = pmap.end(); pmap_it != pmapEnd; ++pmap_it)
       {
           AoS&     pbx = pmap_it->second.GetArrayOfStructs();
           const int n    = pbx.size();

           for (int i = 0; i < n; i++)
           {
              ParticleType& p = pbx[i];
              if (p.id() > 0)
              {
                  // Load velocities
                  for (int d=0; d < AMREX_SPACEDIM; d++)
                      p.rdata(d+1) = part_data[cnt+d];

                  // Update counter
                  cnt += AMREX_SPACEDIM;
              }
           }
       }
    }
}

//
// Assumes mass is in rdata(0), vx in rdata(1), ...!
// dim defines the cartesian direction in which the momentum is summed, x is 0, y is 1, ...
//

template <int NSR,int NSI,int NAR,int NAI>
void
NyxParticleContainer<NSR,NSI,NAR,NAI>::sumParticleMomentum (int          lev,
                                                            amrex::Real* mom) const
{
    BL_PROFILE("NyxParticleContainer<NSR,NSI,NAR,NAI>::sumParticleMomentum()");
    BL_ASSERT(NSR >= AMREX_SPACEDIM+1);
    BL_ASSERT(lev >= 0 && lev < this->GetParticles().size());

    const ParticleLevel& pmap = this->GetParticles(lev);

    AMREX_D_TERM(mom[0] = 0;, mom[1] = 0;, mom[2] = 0;);

    for (typename ParticleLevel::const_iterator pmap_it = pmap.begin(), pmapEnd = pmap.end(); pmap_it != pmapEnd; ++pmap_it)
    {
        const AoS& pbox = pmap_it->second.GetArrayOfStructs();
        const int   n    = pbox.size();

        amrex::Real mom_0 = 0, mom_1 = 0, mom_2 = 0;

#ifdef _OPENMP
#pragma omp parallel for reduction(+:mom_0,mom_1,mom_2)
#endif
        for (int i = 0; i < n; i++)
        {
            const ParticleType& p = pbox[i];

            if (p.id() > 0)
            {
                AMREX_D_TERM(mom_0 += p.rdata(0) * p.rdata(1);,
                             mom_1 += p.rdata(0) * p.rdata(2);,
                             mom_2 += p.rdata(0) * p.rdata(3););
            }
        }

        AMREX_D_TERM(mom[0] += mom_0;, mom[1] += mom_1;, mom[2] += mom_2;);
    }

    amrex::ParallelDescriptor::ReduceRealSum(mom,AMREX_SPACEDIM);
}

template <int NSR,int NSI,int NAR,int NAI>
amrex::Real
NyxParticleContainer<NSR,NSI,NAR,NAI>::estTimestep (amrex::MultiFab&       acceleration,
                                                    int             lev,
                                                    amrex::Real            cfl) const
{
    return estTimestep(acceleration,1.0,lev,cfl);
}

template <int NSR,int NSI,int NAR,int NAI>
amrex::Real
NyxParticleContainer<NSR,NSI,NAR,NAI>::estTimestep (amrex::MultiFab&       acceleration,
                                                    amrex::Real            a,
                                                    int                    lev,
                                                    amrex::Real            cfl) const
{
    BL_PROFILE("NyxParticleContainer<NSR,NSI,NAR,NAI>::estTimestep(lev)");
    amrex::Real            dt               = 1e50;
    BL_ASSERT(NSR >= AMREX_SPACEDIM+1);
    BL_ASSERT(lev >= 0);

    if (this->GetParticles().size() == 0)
        return dt;

    const amrex::Real      strttime         = amrex::ParallelDescriptor::second();

    const amrex::Geometry& geom             = this->m_gdb->Geom(lev);
    const amrex::IntVect   base             = geom.Domain().smallEnd();
    const auto             dxi              = geom.InvCellSizeArray();
    amrex::GpuArray<amrex::Real,AMREX_SPACEDIM> adxi{dxi[0]/a,dxi[1]/a,dxi[2]/a};
    const auto             plo              = geom.ProbLoArray();

    long num_particles_at_level = 0;

    std::unique_ptr<amrex::MultiFab> ac_pointer;
    if (!this->OnSameGrids(lev, acceleration))
    {
        ac_pointer.reset(new amrex::MultiFab(this->m_gdb->ParticleBoxArray(lev),
                                             this->m_gdb->ParticleDistributionMap(lev),
                                             acceleration.nComp(), acceleration.nGrow()));
        if(acceleration.boxArray() == ac_pointer->boxArray())//this->finestLevel() == 0)
        {
            ac_pointer->Redistribute(acceleration,0,0,AMREX_SPACEDIM,acceleration.nGrowVect());
            ac_pointer->FillBoundary(geom.periodicity()); // DO WE NEED GHOST CELLS FILLED ???
        }
        else
        {
            ac_pointer->ParallelCopy(acceleration,0,0,AMREX_SPACEDIM,acceleration.nGrowVect(),acceleration.nGrowVect());
            ac_pointer->FillBoundary(geom.periodicity()); // DO WE NEED GHOST CELLS FILLED ???
        }
    }

#ifdef _OPENMP
#pragma omp parallel if (amrex::Gpu::notInLaunchRegion()) reduction(min:dt)
#endif
    {
      amrex::ReduceOps<amrex::ReduceOpMin> reduce_op;
      amrex::ReduceData<amrex::Real> reduce_data(reduce_op);
      using ReduceTuple = typename decltype(reduce_data)::Type;

        for (MyConstParIter pti(*this, lev); pti.isValid(); ++pti) {
            const int grid = pti.index();
            const AoS&       pbox = pti.GetArrayOfStructs();
            const ParticleType* pstruct = pbox().data();
            const int        n    = pbox.size();

            const auto gfab = (ac_pointer) ? (*ac_pointer).array(grid) : acceleration.array(grid);

            num_particles_at_level += n;

                reduce_op.eval(n, reduce_data,
                [=] AMREX_GPU_DEVICE (const int i) -> ReduceTuple {
                const ParticleType& p = pstruct[i];

                if (p.id() > 0)
                {

                    amrex::IntVect cell;
                    cell[0]= static_cast<int>(amrex::Math::floor((p.pos(0)-plo[0])*dxi[0]));
                    cell[1]= static_cast<int>(amrex::Math::floor((p.pos(1)-plo[1])*dxi[1]));
                    cell[2]= static_cast<int>(amrex::Math::floor((p.pos(2)-plo[2])*dxi[2]));
                    cell += base;

                    const amrex::Real mag_vel_over_dx = amrex::Math::abs(p.rdata(1))*adxi[0];
                    const amrex::Real mag_vel_over_dy = amrex::Math::abs(p.rdata(2))*adxi[1];
                    const amrex::Real mag_vel_over_dz = amrex::Math::abs(p.rdata(3))*adxi[2];

                    amrex::Real max_mag_vel_over_dx = amrex::max(mag_vel_over_dx,mag_vel_over_dy,mag_vel_over_dz);
                    amrex::Real dt_part = (max_mag_vel_over_dx > 0) ? (cfl / max_mag_vel_over_dx) : 1e50;

                    const amrex::Real aval_x = gfab(cell[0],cell[1],cell[2],0);
                    const amrex::Real aval_y = gfab(cell[0],cell[1],cell[2],1);
                    const amrex::Real aval_z = gfab(cell[0],cell[1],cell[2],2);
                    const amrex::Real mag_accel = std::sqrt(aval_x*aval_x+aval_y*aval_y+aval_z*aval_z);

                    if (mag_accel > 0) {
                        dt_part = amrex::min( dt_part, 1/std::sqrt(mag_accel*dxi[0]) );
                    }

                    return dt_part;
                }

                return 1e50;
            });
        }

        ReduceTuple hv = reduce_data.value();
        amrex::Real ldt_cpu = amrex::get<0>(hv);

        dt = std::min(dt,ldt_cpu);
    }

    amrex::ParallelDescriptor::ReduceRealMin(dt);

    if (this->m_verbose > 1)
    {
      long min_local_num_particles_at_level = num_particles_at_level;
      long max_local_num_particles_at_level = num_particles_at_level;

      amrex::ParallelDescriptor::ReduceLongMin(min_local_num_particles_at_level);
      amrex::ParallelDescriptor::ReduceLongMax(max_local_num_particles_at_level);

      amrex::Print()<<"Min particles      : "<<min_local_num_particles_at_level<<std::endl;
      amrex::Print()<<"Max particles      : "<<max_local_num_particles_at_level<<std::endl;
      amrex::Arena::PrintUsage();
    }

    //
    // Set dt negative if there are no particles at this level.
    //
    amrex::ParallelDescriptor::ReduceLongSum(num_particles_at_level);

    if (num_particles_at_level == 0) dt = -1.e50;

    if (this->m_verbose > 1)
    {
        amrex::Real stoptime = amrex::ParallelDescriptor::second() - strttime;

        amrex::ParallelDescriptor::ReduceRealMax(stoptime,amrex::ParallelDescriptor::IOProcessorNumber());

        if (amrex::ParallelDescriptor::IOProcessor())
        {
            std::cout << "NyxParticleContainer<NSR,NSI,NAR,NAI>::estTimestep() time: " << stoptime << '\n';
        }
    }

    return dt;
}

template <int NSR,int NSI,int NAR,int NAI>
void
NyxParticleContainer<NSR,NSI,NAR,NAI>::MultiplyParticleMass (int lev, amrex::Real mult)
{
   BL_PROFILE("NyxParticleContainer<NSR,NSI,NAR,NAI>::MultiplyParticleMass()");
   BL_ASSERT(lev == 0);

   ParticleLevel& pmap = this->GetParticles(lev);

   for (typename ParticleLevel::iterator pmap_it = pmap.begin(), pmapEnd = pmap.end(); pmap_it != pmapEnd; ++pmap_it)
   {
        AoS&       pbx = pmap_it->second.GetArrayOfStructs();
        ParticleType* pstruct = pbx().data();
        const long np    = pbx.size();

        amrex::ParallelFor(np,
                           [=] AMREX_GPU_HOST_DEVICE ( long i)
                           {
                               if (pstruct[i].id() > 0)
                               {
                                   //
                                   // Note: rdata(0) is mass, ...
                                   //
                                   pstruct[i].rdata(0) *= mult;
                               }
                           });
   }
   amrex::Gpu::streamSynchronize();
}

template <int NSR,int NSI,int NAR,int NAI>
void
NyxParticleContainer<NSR,NSI,NAR,NAI>::WriteNyxPlotFile (const std::string& dir,
                                                         const std::string& name) const
{
  BL_PROFILE("NyxParticleContainer<NSR,NSI,NAR,NAI>::WriteNyxPlotFile()");

  amrex::NeighborParticleContainer<NSR,NSI>::WritePlotFile(dir, name, real_comp_names);

}

template <int NSR,int NSI,int NAR,int NAI>
void
NyxParticleContainer<NSR,NSI,NAR,NAI>::NyxCheckpoint (const std::string& dir,
                                                      const std::string& name) const
{
  BL_PROFILE("NyxParticleContainer<NSR,NSI,NAR,NAI>::NyxCheckpoint()");

  bool is_checkpoint = true;
  amrex::NeighborParticleContainer<NSR,NSI>::Checkpoint(dir, name, is_checkpoint, real_comp_names);
}

#endif /*_NyxParticleContainer_H_*/
