/*
Highly Optimized Object-oriented Many-particle Dynamics -- Blue Edition
(HOOMD-blue) Open Source Software License Copyright 2009-2015 The Regents of
the University of Michigan All rights reserved.

HOOMD-blue may contain modifications ("Contributions") provided, and to which
copyright is held, by various Contributors who have granted The Regents of the
University of Michigan the right to modify and/or distribute such Contributions.

You may redistribute, use, and create derivate works of HOOMD-blue, in source
and binary forms, provided you abide by the following conditions:

* Redistributions of source code must retain the above copyright notice, this
list of conditions, and the following disclaimer both in the code and
prominently in any materials provided with the distribution.

* Redistributions in binary form must reproduce the above copyright notice, this
list of conditions, and the following disclaimer in the documentation and/or
other materials provided with the distribution.

* All publications and presentations based on HOOMD-blue, including any reports
or published results obtained, in whole or in part, with HOOMD-blue, will
acknowledge its use according to the terms posted at the time of submission on:
http://codeblue.umich.edu/hoomd-blue/citations.html

* Any electronic documents citing HOOMD-Blue will link to the HOOMD-Blue website:
http://codeblue.umich.edu/hoomd-blue/

* Apart from the above required attributions, neither the name of the copyright
holder nor the names of HOOMD-blue's contributors may be used to endorse or
promote products derived from this software without specific prior written
permission.

Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND/OR ANY
WARRANTIES THAT THIS SOFTWARE IS FREE OF INFRINGEMENT ARE DISCLAIMED.

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Maintainer: joaander

/*! \file CellListGPU.cc
    \brief Defines CellListGPU
*/

#include "CellListGPU.h"
#include "CellListGPU.cuh"

#include <boost/python.hpp>

using namespace std;
using namespace boost::python;

/*! \param sysdef system to compute the cell list of
*/
CellListGPU::CellListGPU(boost::shared_ptr<SystemDefinition> sysdef)
    : CellList(sysdef)
    {
    if (!m_exec_conf->isCUDAEnabled())
        {
        m_exec_conf->msg->error() << "Creating a CellListGPU with no GPU in the execution configuration" << endl;
        throw std::runtime_error("Error initializing CellListGPU");
        }

    m_tuner.reset(new Autotuner(32, 1024, 32, 5, 100000, "cell_list", this->m_exec_conf));

    #ifdef ENABLE_CUDA
    if (m_exec_conf->isCUDAEnabled())
        {
        // create a ModernGPU context
        m_mgpu_context = mgpu::CreateCudaDeviceAttachStream(0);
        }
    #endif
    }

void CellListGPU::computeCellList()
    {
    if (m_prof)
        m_prof->push(m_exec_conf, "compute");

    // acquire the particle data
    ArrayHandle<Scalar4> d_pos(m_pdata->getPositions(), access_location::device, access_mode::read);
    ArrayHandle<Scalar4> d_orientation(m_pdata->getOrientationArray(), access_location::device, access_mode::read);
    ArrayHandle<Scalar> d_charge(m_pdata->getCharges(), access_location::device, access_mode::read);
    ArrayHandle<Scalar> d_diameter(m_pdata->getDiameters(), access_location::device, access_mode::read);
    ArrayHandle<unsigned int> d_body(m_pdata->getBodies(), access_location::device, access_mode::read);

    BoxDim box = m_pdata->getBox();

    // access the cell list data arrays
    ArrayHandle<unsigned int> d_cell_size(m_cell_size, access_location::device, access_mode::overwrite);
    ArrayHandle<Scalar4> d_xyzf(m_xyzf, access_location::device, access_mode::overwrite);
    ArrayHandle<Scalar4> d_tdb(m_tdb, access_location::device, access_mode::overwrite);
    ArrayHandle<Scalar4> d_cell_orientation(m_orientation, access_location::device, access_mode::overwrite);
    ArrayHandle<unsigned int> d_cell_idx(m_idx, access_location::device, access_mode::overwrite);


    // autotune block sizes
    m_tuner->begin();
    gpu_compute_cell_list(d_cell_size.data,
                          d_xyzf.data,
                          d_tdb.data,
                          d_cell_orientation.data,
                          d_cell_idx.data,
                          m_conditions.getDeviceFlags(),
                          d_pos.data,
                          d_orientation.data,
                          d_charge.data,
                          d_diameter.data,
                          d_body.data,
                          m_pdata->getN(),
                          m_pdata->getNGhosts(),
                          m_Nmax,
                          m_flag_charge,
                          m_flag_type,
                          box,
                          m_cell_indexer,
                          m_cell_list_indexer,
                          getGhostWidth(),
                          m_tuner->getParam());
    if(m_exec_conf->isCUDAErrorCheckingEnabled())
        CHECK_CUDA_ERROR();
    m_tuner->end();

    if(m_exec_conf->isCUDAErrorCheckingEnabled())
        CHECK_CUDA_ERROR();

    if (m_sort_cell_list)
        {
        ScopedAllocation<uint2> d_sort_idx(m_exec_conf->getCachedAllocator(), m_cell_list_indexer.getNumElements());
        ScopedAllocation<unsigned int> d_sort_permutation(m_exec_conf->getCachedAllocator(), m_cell_list_indexer.getNumElements());
        ScopedAllocation<unsigned int> d_cell_idx_new(m_exec_conf->getCachedAllocator(), m_idx.getNumElements());
        ScopedAllocation<Scalar4> d_xyzf_new(m_exec_conf->getCachedAllocator(), m_xyzf.getNumElements());
        ScopedAllocation<Scalar4> d_cell_orientation_new(m_exec_conf->getCachedAllocator(), m_orientation.getNumElements());
        ScopedAllocation<Scalar4> d_tdb_new(m_exec_conf->getCachedAllocator(), m_tdb.getNumElements());

        gpu_sort_cell_list(d_cell_size.data,
                           d_xyzf.data,
                           d_xyzf_new.data,
                           d_tdb.data,
                           d_tdb_new.data,
                           d_cell_orientation.data,
                           d_cell_orientation_new.data,
                           d_cell_idx.data,
                           d_cell_idx_new.data,
                           d_sort_idx.data,
                           d_sort_permutation.data,
                           m_cell_indexer,
                           m_cell_list_indexer,
                           m_mgpu_context);

        if(m_exec_conf->isCUDAErrorCheckingEnabled())
            CHECK_CUDA_ERROR();
        }

    if (m_prof)
        m_prof->pop(m_exec_conf);
    }

void export_CellListGPU()
    {
    class_<CellListGPU, boost::shared_ptr<CellListGPU>, bases<CellList>, boost::noncopyable >
        ("CellListGPU", init< boost::shared_ptr<SystemDefinition> >())
        ;
    }
