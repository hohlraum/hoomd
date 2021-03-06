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

#ifndef _COMPUTE_THERMO_TYPES_H_
#define _COMPUTE_THERMO_TYPES_H_

#include "HOOMDMath.h"
/*! \file ComputeThermoTypes.h
    \brief Data structures common to both CPU and GPU implementations of ComputeThermo
    */

//! Enum for indexing the GPUArray of computed values
struct thermo_index
    {
    //! The enum
    enum Enum
        {
        translational_kinetic_energy=0,      //!< Index for the kinetic energy in the GPUArray
        rotational_kinetic_energy,       //!< Rotational kinetic energy
        potential_energy,    //!< Index for the potential energy in the GPUArray
        pressure,            //!< Total pressure
        pressure_xx,         //!< Index for the xx component of the pressure tensor in the GPUArray
        pressure_xy,         //!< Index for the xy component of the pressure tensor in the GPUArray
        pressure_xz,         //!< Index for the xz component of the pressure tensor in the GPUArray
        pressure_yy,         //!< Index for the yy component of the pressure tensor in the GPUArray
        pressure_yz,         //!< Index for the yz component of the pressure tensor in the GPUArray
        pressure_zz,         //!< Index for the zz component of the pressure tensor in the GPUArray
        num_quantities       // final element to count number of quantities
        };
    };

//! structure for storing the components of the pressure tensor
struct PressureTensor
    {
    //! The six components of the upper triangular pressure tensor
    Scalar xx; //!< xx component
    Scalar xy; //!< xy component
    Scalar xz; //!< xz component
    Scalar yy; //!< yy component
    Scalar yz; //!< yz component
    Scalar zz; //!< zz component
    };
#endif
