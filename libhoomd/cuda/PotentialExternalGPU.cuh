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

// Maintainer: jglaser

#include "HOOMDMath.h"
#include "ParticleData.cuh"

#include <assert.h>

/*! \file PotentialExternalGPU.cuh
    \brief Defines templated GPU kernel code for calculating the external forces.
*/

#ifndef __POTENTIAL_EXTERNAL_GPU_CUH__
#define __POTENTIAL_EXTERNAL_GPU_CUH__

//! Wraps arguments to gpu_cpef
struct external_potential_args_t
    {
    //! Construct a external_potential_args_t
    external_potential_args_t(Scalar4 *_d_force,
              Scalar *_d_virial,
              const unsigned int _virial_pitch,
              const unsigned int _N,
              const Scalar4 *_d_pos,
              const Scalar *_d_diameter,
              const Scalar *_d_charge,
              const BoxDim& _box,
              const unsigned int _block_size)
                : d_force(_d_force),
                  d_virial(_d_virial),
                  virial_pitch(_virial_pitch),
                  box(_box),
                  N(_N),
                  d_pos(_d_pos),
                  d_diameter(_d_diameter),
                  d_charge(_d_charge),
                  block_size(_block_size)
        {
        };

    Scalar4 *d_force;                //!< Force to write out
    Scalar *d_virial;                //!< Virial to write out
    const unsigned int virial_pitch; //!< The pitch of the 2D array of virial matrix elements
    const BoxDim& box;         //!< Simulation box in GPU format
    const unsigned int N;           //!< Number of particles
    const Scalar4 *d_pos;           //!< Device array of particle positions
    const Scalar *d_diameter;       //!< particle diameters
    const Scalar *d_charge;         //!< particle charges
    const unsigned int block_size;  //!< Block size to execute
    };

#ifdef NVCC
//! Kernel for calculating external forces
/*! This kernel is called to calculate the external forces on all N particles. Actual evaluation of the potentials and
    forces for each particle is handled via the template class \a evaluator.

    \param d_force Device memory to write computed forces
    \param d_virial Device memory to write computed virials
    \param virial_pitch pitch of 2D virial array
    \param N number of particles
    \param d_pos device array of particle positions
    \param box Box dimensions used to implement periodic boundary conditions
    \param params per-type array of parameters for the potential

*/
template< class evaluator >
__global__ void gpu_compute_external_forces_kernel(Scalar4 *d_force,
                                               Scalar *d_virial,
                                               const unsigned int virial_pitch,
                                               const unsigned int N,
                                               const Scalar4 *d_pos,
                                               const Scalar *d_diameter,
                                               const Scalar *d_charge,
                                               const BoxDim box,
                                               const typename evaluator::param_type *params,
                                               const typename evaluator::field_type *d_field)
    {
    // start by identifying which particle we are to handle
    unsigned int idx = blockIdx.x * blockDim.x + threadIdx.x;

    // read in field data cooperatively
    extern __shared__ char s_data[];
    typename evaluator::field_type *s_field = (typename evaluator::field_type *)(&s_data[0]);
        {
        unsigned int tidx = threadIdx.x;
        unsigned int block_size = blockDim.x;
        unsigned int field_size = sizeof(typename evaluator::field_type) / sizeof(int);

        for (unsigned int cur_offset = 0; cur_offset < field_size; cur_offset += block_size)
            {
            if (cur_offset + tidx < field_size)
                {
                ((int *)s_field)[cur_offset + tidx] = ((int *)d_field)[cur_offset + tidx];
                }
            }
        }
    const typename evaluator::field_type& field = *s_field;

    if (idx >= N)
        return;

    // read in the position of our particle.
    // (MEM TRANSFER: 16 bytes)
    Scalar4 posi = d_pos[idx];
    Scalar di;
    Scalar qi;
    if (evaluator::needsDiameter())
        di = d_diameter[idx];
    else
        di += Scalar(1.0); // shutup compiler warning

    if (evaluator::needsCharge())
        qi = d_charge[idx];
    else
        qi = Scalar(0.0); // shutup compiler warning


    // initialize the force to 0
    Scalar3 force = make_scalar3(Scalar(0.0), Scalar(0.0), Scalar(0.0));
    Scalar virial[6];
    for (unsigned int k = 0; k < 6; k++)
        virial[k] = Scalar(0.0);
    Scalar energy = Scalar(0.0);

    unsigned int typei = __scalar_as_int(posi.w);
    Scalar3 Xi = make_scalar3(posi.x, posi.y, posi.z);
    evaluator eval(Xi, box, params[typei], field);

    if (evaluator::needsDiameter())
        eval.setDiameter(di);
    if (evaluator::needsCharge())
        eval.setCharge(qi);

    eval.evalForceEnergyAndVirial(force, energy, virial);

    // now that the force calculation is complete, write out the result)
    d_force[idx].x = force.x;
    d_force[idx].y = force.y;
    d_force[idx].z = force.z;
    d_force[idx].w = energy;

    for (unsigned int k = 0; k < 6; k++)
        d_virial[k*virial_pitch+idx] = virial[k];
    }

#endif

template< class evaluator >
cudaError_t gpu_cpef(const external_potential_args_t& external_potential_args,
                     const typename evaluator::param_type *d_params,
                     const typename evaluator::field_type *d_field);
#endif // __POTENTIAL_PAIR_GPU_CUH__
