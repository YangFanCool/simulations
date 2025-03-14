#include <AMReX_ParticleCommunication.H>
#include <AMReX_ParallelDescriptor.H>

namespace amrex {

void ParticleCopyOp::clear ()
{
    m_boxes.resize(0);
    m_levels.resize(0);
    m_src_indices.resize(0);
    m_periodic_shift.resize(0);
}

void ParticleCopyOp::setNumLevels (int num_levels)
{
    m_boxes.resize(num_levels);
    m_levels.resize(num_levels);
    m_src_indices.resize(num_levels);
    m_periodic_shift.resize(num_levels);
}

void ParticleCopyOp::resize (int gid, int lev, int size)
{
    if (lev >= m_boxes.size())
    {
        setNumLevels(lev+1);
    }
    m_boxes[lev][gid].resize(size);
    m_levels[lev][gid].resize(size);
    m_src_indices[lev][gid].resize(size);
    m_periodic_shift[lev][gid].resize(size);
}

void ParticleCopyPlan::clear ()
{
    m_dst_indices.clear();
    m_box_counts_d.clear();
    m_box_counts_h.clear();
    m_box_offsets.clear();

    m_rcv_box_counts.clear();
    m_rcv_box_offsets.clear();
    m_rcv_box_ids.clear();
    m_rcv_box_pids.clear();
    m_rcv_box_levs.clear();
}

void ParticleCopyPlan::buildMPIStart (const ParticleBufferMap& map, Long psize) // NOLINT(readability-convert-member-functions-to-static)
{
    BL_PROFILE("ParticleCopyPlan::buildMPIStart");

#ifdef AMREX_USE_MPI
    const int NProcs = ParallelContext::NProcsSub();
    const int MyProc = ParallelContext::MyProcSub();
    const auto NNeighborProcs = static_cast<int>(m_neighbor_procs.size());

    if (NProcs == 1) { return; }

    m_Snds.resize(0);
    m_Snds.resize(NProcs, 0);

    m_Rcvs.resize(0);
    m_Rcvs.resize(NProcs, 0);

    m_snd_num_particles.resize(0);
    m_snd_num_particles.resize(NProcs, 0);

    m_rcv_num_particles.resize(0);
    m_rcv_num_particles.resize(NProcs, 0);

    std::map<int, Vector<int> > snd_data;

    m_NumSnds = 0;
    for (auto i : m_neighbor_procs)
    {
        auto box_buffer_indices = map.allBucketsOnProc(i);
        Long nbytes = 0;
        for (auto bucket : box_buffer_indices)
        {
            int dst = map.bucketToGrid(bucket);
            int lev = map.bucketToLevel(bucket);
            AMREX_ASSERT(m_box_counts_h[bucket] <= static_cast<unsigned int>(std::numeric_limits<int>::max()));
            int npart = static_cast<int>(m_box_counts_h[bucket]);
            if (npart == 0) { continue; }
            m_snd_num_particles[i] += npart;
            if (i == MyProc) { continue; }
            snd_data[i].push_back(npart);
            snd_data[i].push_back(dst);
            snd_data[i].push_back(lev);
            snd_data[i].push_back(MyProc);
            nbytes += 4*sizeof(int);
        }
        m_Snds[i] = nbytes;
        m_NumSnds += nbytes;
    }

    doHandShake(m_Snds, m_Rcvs);

    const int SeqNum = ParallelDescriptor::SeqNum();
    Long tot_snds_this_proc = 0;
    Long tot_rcvs_this_proc = 0;

    if (m_local)
    {
        for (int i = 0; i < NNeighborProcs; ++i)
        {
            tot_snds_this_proc += m_Snds[m_neighbor_procs[i]];
            tot_rcvs_this_proc += m_Rcvs[m_neighbor_procs[i]];
        }
    } else {
        for (int i = 0; i < NProcs; ++i)
        {
            tot_snds_this_proc += m_Snds[i];
            tot_rcvs_this_proc += m_Rcvs[i];
        }
    }

    if ( (tot_snds_this_proc == 0) && (tot_rcvs_this_proc == 0) )
    {
        m_nrcvs = 0;
        m_NumSnds = 0;
        return;
    }

    m_RcvProc.resize(0);
    m_rOffset.resize(0);
    std::size_t TotRcvBytes = 0;
    for (auto i : m_neighbor_procs)
    {
        if (m_Rcvs[i] > 0)
        {
            m_RcvProc.push_back(i);
            m_rOffset.push_back(TotRcvBytes/sizeof(int));
            TotRcvBytes += m_Rcvs[i];
        }
    }

    m_nrcvs = static_cast<int>(m_RcvProc.size());

    m_build_stats.resize(0);
    m_build_stats.resize(m_nrcvs);

    m_build_rreqs.resize(0);
    m_build_rreqs.resize(m_nrcvs);

    m_rcv_data.resize(TotRcvBytes/sizeof(int));

    for (int i = 0; i < m_nrcvs; ++i)
    {
        const auto Who    = m_RcvProc[i];
        const auto offset = m_rOffset[i];
        const auto Cnt    = m_Rcvs[Who];

        AMREX_ASSERT(Cnt > 0);
        AMREX_ASSERT(Cnt < std::numeric_limits<int>::max());
        AMREX_ASSERT(Who >= 0 && Who < NProcs);

        m_build_rreqs[i] = ParallelDescriptor::Arecv((char*) (m_rcv_data.dataPtr() + offset), Cnt, Who, SeqNum, ParallelContext::CommunicatorSub()).req();
    }

    Vector<MPI_Request> snd_reqs;
    Vector<MPI_Status>  snd_stats;
    for (auto i : m_neighbor_procs)
    {
        if (i == MyProc) { continue; }
        const auto Who = i;
        const auto Cnt = m_Snds[i];
        if (Cnt == 0) { continue; }

        AMREX_ASSERT(Cnt > 0);
        AMREX_ASSERT(Who >= 0 && Who < NProcs);
        AMREX_ASSERT(Cnt < std::numeric_limits<int>::max());

        snd_reqs.push_back(ParallelDescriptor::Asend((char*) snd_data[i].data(), Cnt, Who, SeqNum,
                                                      ParallelContext::CommunicatorSub()).req());
    }

    m_snd_counts.resize(0);
    m_snd_offsets.resize(0);
    m_snd_pad_correction_h.resize(0);

    m_snd_offsets.push_back(0);
    m_snd_pad_correction_h.push_back(0);
    for (int i = 0; i < NProcs; ++i)
    {
        Long nbytes = m_snd_num_particles[i]*psize;
        std::size_t acd = ParallelDescriptor::sizeof_selected_comm_data_type(nbytes);
        auto Cnt = static_cast<Long>(amrex::aligned_size(acd, nbytes));
        Long bytes_to_send = (i == MyProc) ? 0 : Cnt;
        m_snd_counts.push_back(bytes_to_send);
        m_snd_offsets.push_back(amrex::aligned_size(Arena::align_size,
                                                    m_snd_offsets.back() + Cnt));
        m_snd_pad_correction_h.push_back(m_snd_pad_correction_h.back() + nbytes);
    }

    for (int i = 0; i < NProcs; ++i)
    {
        m_snd_pad_correction_h[i] = m_snd_offsets[i] - m_snd_pad_correction_h[i];
    }

    m_snd_pad_correction_d.resize(m_snd_pad_correction_h.size());
    Gpu::copy(Gpu::hostToDevice, m_snd_pad_correction_h.begin(), m_snd_pad_correction_h.end(),
              m_snd_pad_correction_d.begin());

    snd_stats.resize(0);
    snd_stats.resize(snd_reqs.size());
    ParallelDescriptor::Waitall(snd_reqs, snd_stats);
#else
    amrex::ignore_unused(map,psize);
#endif
}

void ParticleCopyPlan::buildMPIFinish (const ParticleBufferMap& map) // NOLINT(readability-convert-member-functions-to-static)
{
    amrex::ignore_unused(map);

    BL_PROFILE("ParticleCopyPlan::buildMPIFinish");

#ifdef AMREX_USE_MPI

    const int NProcs = ParallelContext::NProcsSub();
    if (NProcs == 1) { return; }

    if (m_nrcvs > 0)
    {
        ParallelDescriptor::Waitall(m_build_rreqs, m_build_stats);

        m_rcv_box_offsets.resize(0);
        m_rcv_box_counts.resize(0);
        m_rcv_box_ids.resize(0);
        m_rcv_box_levs.resize(0);
        m_rcv_box_pids.resize(0);

        m_rcv_box_offsets.push_back(0);
        for (int i = 0, N = static_cast<int>(m_rcv_data.size()); i < N; i+=4)
        {
            m_rcv_box_counts.push_back(m_rcv_data[i]);
            AMREX_ASSERT(ParallelContext::MyProcSub() == map.procID(m_rcv_data[i+1], m_rcv_data[i+2]));
            m_rcv_box_ids.push_back(m_rcv_data[i+1]);
            m_rcv_box_levs.push_back(m_rcv_data[i+2]);
            m_rcv_box_pids.push_back(m_rcv_data[i+3]);
            m_rcv_box_offsets.push_back(m_rcv_box_offsets.back() + m_rcv_box_counts.back());
        }
    }

    for (int j = 0; j < m_nrcvs; ++j)
    {
        const auto Who    = m_RcvProc[j];
        const auto offset = m_rOffset[j];
        const auto Cnt    = m_Rcvs[Who]/sizeof(int);

        Long nparticles = 0;
        for (auto i = offset; i < offset + Cnt; i +=4)
        {
            nparticles += m_rcv_data[i];
        }
        m_rcv_num_particles[Who] = nparticles;
    }
#endif // MPI
}

void ParticleCopyPlan::doHandShake (const Vector<Long>& Snds, Vector<Long>& Rcvs) const // NOLINT(readability-convert-member-functions-to-static)
{
    BL_PROFILE("ParticleCopyPlan::doHandShake");
    if (m_local) { doHandShakeLocal(Snds, Rcvs); }
    else         { doHandShakeGlobal(Snds, Rcvs); }
}

void ParticleCopyPlan::doHandShakeLocal (const Vector<Long>& Snds, Vector<Long>& Rcvs) const // NOLINT(readability-convert-member-functions-to-static)
{
#ifdef AMREX_USE_MPI
    const int SeqNum = ParallelDescriptor::SeqNum();
    const auto num_rcvs = static_cast<int>(m_neighbor_procs.size());
    Vector<MPI_Status>  rstats(num_rcvs);
    Vector<MPI_Request> rreqs(num_rcvs);
    Vector<MPI_Status>  sstats(num_rcvs);
    Vector<MPI_Request> sreqs(num_rcvs);

    // Post receives
    for (int i = 0; i < num_rcvs; ++i)
    {
        const int Who = m_neighbor_procs[i];
        const Long Cnt = 1;

        AMREX_ASSERT(Who >= 0 && Who < ParallelContext::NProcsSub());

        rreqs[i] = ParallelDescriptor::Arecv(&Rcvs[Who], Cnt, Who, SeqNum,
                                             ParallelContext::CommunicatorSub()).req();
    }

    // Send.
    for (int i = 0; i < num_rcvs; ++i)
    {
        const int Who = m_neighbor_procs[i];
        const Long Cnt = 1;

        AMREX_ASSERT(Who >= 0 && Who < ParallelContext::NProcsSub());

        sreqs[i] = ParallelDescriptor::Asend(&Snds[Who], Cnt, Who, SeqNum,
                                             ParallelContext::CommunicatorSub()).req();
    }

    if (num_rcvs > 0)
    {
        ParallelDescriptor::Waitall(sreqs, sstats);
        ParallelDescriptor::Waitall(rreqs, rstats);
    }
#else
    amrex::ignore_unused(Snds,Rcvs);
#endif
}

void ParticleCopyPlan::doHandShakeAllToAll (const Vector<Long>& Snds, Vector<Long>& Rcvs)
{
#ifdef AMREX_USE_MPI
    BL_COMM_PROFILE(BLProfiler::Alltoall, sizeof(Long),
                    ParallelContext::MyProcSub(), BLProfiler::BeforeCall());

    BL_MPI_REQUIRE( MPI_Alltoall(Snds.dataPtr(),
                                 1,
                                 ParallelDescriptor::Mpi_typemap<Long>::type(),
                                 Rcvs.dataPtr(),
                                 1,
                                 ParallelDescriptor::Mpi_typemap<Long>::type(),
                                 ParallelContext::CommunicatorSub()) );

    AMREX_ASSERT(Rcvs[ParallelContext::MyProcSub()] == 0);

    BL_COMM_PROFILE(BLProfiler::Alltoall, sizeof(Long),
                    ParallelContext::MyProcSub(), BLProfiler::AfterCall());
#else
    amrex::ignore_unused(Snds,Rcvs);
#endif
}

void ParticleCopyPlan::doHandShakeGlobal (const Vector<Long>& Snds, Vector<Long>& Rcvs)
{
#ifdef AMREX_USE_MPI
    const int SeqNum = ParallelDescriptor::SeqNum();
    const int NProcs = ParallelContext::NProcsSub();

    Vector<Long> snd_connectivity(NProcs, 0);
    Vector<int > rcv_connectivity(NProcs, 1);
    for (int i = 0; i < NProcs; ++i) { if (Snds[i] > 0) { snd_connectivity[i] = 1; } }

    Long num_rcvs = 0;
    MPI_Reduce_scatter(snd_connectivity.data(), &num_rcvs, rcv_connectivity.data(),
                       ParallelDescriptor::Mpi_typemap<Long>::type(), MPI_SUM,
                       ParallelContext::CommunicatorSub());

    Vector<MPI_Status>  rstats(num_rcvs);
    Vector<MPI_Request> rreqs(num_rcvs);
    Vector<MPI_Status>  sstats;
    Vector<MPI_Request> sreqs;

    Vector<Long> num_bytes_rcv(num_rcvs);
    for (int i = 0; i < static_cast<int>(num_rcvs); ++i)
    {
        BL_MPI_REQUIRE(MPI_Irecv( &num_bytes_rcv[i], 1, ParallelDescriptor::Mpi_typemap<Long>::type(),
                                  MPI_ANY_SOURCE, SeqNum, ParallelContext::CommunicatorSub(), &rreqs[i] ));
    }
    for (int i = 0; i < NProcs; ++i)
    {
        if (Snds[i] == 0) { continue; }
        const Long Cnt = 1;
        sreqs.push_back(ParallelDescriptor::Asend( &Snds[i], Cnt, i, SeqNum, ParallelContext::CommunicatorSub()).req());
    }

    sstats.resize(0);
    sstats.resize(sreqs.size());
    ParallelDescriptor::Waitall(sreqs, sstats);
    ParallelDescriptor::Waitall(rreqs, rstats);

    for (int i = 0; i < num_rcvs; ++i)
    {
        const auto Who = rstats[i].MPI_SOURCE;
        Rcvs[Who] = num_bytes_rcv[i];
    }
#else
    amrex::ignore_unused(Snds,Rcvs);
#endif
}

void communicateParticlesFinish (const ParticleCopyPlan& plan)
{
    BL_PROFILE("amrex::communicateParticlesFinish");
#ifdef AMREX_USE_MPI
    if (plan.m_NumSnds > 0)
    {
        ParallelDescriptor::Waitall(plan.m_particle_sreqs, plan.m_particle_sstats);
    }
    if (plan.m_nrcvs > 0)
    {
        ParallelDescriptor::Waitall(plan.m_particle_rreqs, plan.m_particle_rstats);
    }
#else
    amrex::ignore_unused(plan);
#endif
}

}
