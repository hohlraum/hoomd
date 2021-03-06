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

// Maintainer: ndtrung

/*! \file TwoStepNPTRigidGPU.cuh
    \brief Declares GPU kernel code for NPT rigid body integration on the GPU. Used by TwoStepNPTRigidGPU.
*/

#include "ParticleData.cuh"
#include "RigidData.cuh"
#include "TwoStepNVERigidGPU.cuh"

#ifndef __TWO_STEP_NPT_RIGID_CUH__
#define __TWO_STEP_NPT_RIGID_CUH__

/*! Thermostat data structure
*/
struct gpu_npt_rigid_data
    {
    unsigned int n_bodies;  //!< Number of rigid bodies
    unsigned int nf_t;      //!< Translational degrees of freedom
    unsigned int nf_r;      //!< Rotational degrees of freedom
    unsigned int dimension; //!< System dimension
    Scalar4* new_box;        //!< New box size
    Scalar4  dilation;       //!< Box size change

    Scalar4  scale_t;        //!< Translational velocity scaling factor
    Scalar   scale_r;        //!< Conjugate momentum scaling factor
    Scalar4  scale_v;        //!< Translational velocity scaling factor due to barostatting
    Scalar4  epsilon_dot;    //!< Barostat velocity

    Scalar *partial_Ksum_t;  //!< NBlocks elements, each is a partial sum of m*v^2
    Scalar *partial_Ksum_r;  //!< NBlocks elements, each is a partial sum of L*w^2
    Scalar *Ksum_t;          //!< fully reduced Ksum_t on one GPU
    Scalar *Ksum_r;          //!< fully reduced Ksum_r on one GPU
    };

//! Kernel driver for the first part of the NPT update called by TwoStepNPTRigidGPU
cudaError_t gpu_npt_rigid_step_one(const gpu_rigid_data_arrays& rigid_data,
                                   unsigned int *d_group_members,
                                   unsigned int group_size,
                                   Scalar4 *d_net_force,
                                   const BoxDim& box,
                                   const gpu_npt_rigid_data &npt_rdata,
                                   Scalar deltaT);

//! Kernel driver for the second part of the NPT update called by TwoStepNPTRigidGPU
cudaError_t gpu_npt_rigid_step_two(const gpu_rigid_data_arrays& rigid_data,
                                   unsigned int *d_group_members,
                                   unsigned int group_size,
                                   Scalar4 *d_net_force,
                                   Scalar *d_net_virial,
                                   const BoxDim& box,
                                   const gpu_npt_rigid_data &npt_rdata,
                                   Scalar deltaT);

//! Kernel driver for the Ksum reduction final pass called by TwoStepNPTRigidGPU
cudaError_t gpu_npt_rigid_reduce_ksum(const gpu_npt_rigid_data &npt_rdata);

#endif // __TWO_STEP_NPT_RIGID_CUH__

