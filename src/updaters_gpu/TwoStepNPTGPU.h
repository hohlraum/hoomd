/*
Highly Optimized Object-oriented Many-particle Dynamics -- Blue Edition
(HOOMD-blue) Open Source Software License Copyright 2008, 2009 Ames Laboratory
Iowa State University and The Regents of the University of Michigan All rights
reserved.

HOOMD-blue may contain modifications ("Contributions") provided, and to which
copyright is held, by various Contributors who have granted The Regents of the
University of Michigan the right to modify and/or distribute such Contributions.

Redistribution and use of HOOMD-blue, in source and binary forms, with or
without modification, are permitted, provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright notice, this
list of conditions, and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
list of conditions, and the following disclaimer in the documentation and/or
other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of HOOMD-blue's
contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND/OR
ANY WARRANTIES THAT THIS SOFTWARE IS FREE OF INFRINGEMENT ARE DISCLAIMED.

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// $Id$
// $URL$
// Maintainer: joaander

#include "TwoStepNPT.h"

#ifndef __TWO_STEP_NPT_GPU_H__
#define __TWO_STEP_NPT_GPU_H__

/*! \file TwoStepNPTGPU.h
    \brief Declares the TwoStepNPTGPU class
*/

//! Integrates part of the system forward in two steps in the NPT ensemble on the GPU
/*! Implements Nose-Hoover/Anderson NPT integration through the IntegrationMethodTwoStep interface, runs on the GPU
    
    \ingroup updaters
*/
class TwoStepNPTGPU : public TwoStepNPT
    {
    public:
        //! Constructs the integration method and associates it with the system
        TwoStepNPTGPU(boost::shared_ptr<SystemDefinition> sysdef,
                      boost::shared_ptr<ParticleGroup> group,
                      Scalar tau,
                      Scalar tauP,
                      boost::shared_ptr<Variant> T,
                      boost::shared_ptr<Variant> P);
        virtual ~TwoStepNPTGPU() {};
        
        //! Performs the first step of the integration
        virtual void integrateStepOne(unsigned int timestep);
        
        //! Performs the second step of the integration
        virtual void integrateStepTwo(unsigned int timestep);
    protected:
        unsigned int m_block_size;        //!< Block size to launch on the GPU (must be a power of two)
        unsigned int m_group_num_blocks;  //!< Number of blocks of \a block_size to launch when updating the group
        unsigned int m_full_num_blocks;   //!< Number of blocks to launch when updating all particles
        GPUArray<float> m_partial_sum2K;  //!< Partial sums from the first pass reduction
        GPUArray<float> m_sum2K;          //!< Total sum of 2K on the GPU
        GPUArray<float> m_partial_sumW;   //!< Partial sums for the first pass reduction of W
        GPUArray<float> m_sumW;           //!< Total sum of W on the GPU

        //! helper function to compute pressure
        virtual Scalar computePressure(unsigned int timestep);
        
        //! helper function to compute group temperature
        virtual Scalar computeGroupTemperature(unsigned int timestep);
    };

//! Exports the TwoStepNPTGPU class to python
void export_TwoStepNPTGPU();

#endif // #ifndef __TWO_STEP_NPT_GPU_H__
