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

#include "Compute.h"
#include "GPUArray.h"
#include "ComputeThermoTypes.h"
#include "ParticleGroup.h"

#include <boost/shared_ptr.hpp>
#include <limits>

/*! \file ComputeThermo.h
    \brief Declares a class for computing thermodynamic quantities
*/

#ifdef NVCC
#error This header cannot be compiled by nvcc
#endif

#ifndef __COMPUTE_THERMO_H__
#define __COMPUTE_THERMO_H__

//! Computes thermodynamic properties of a group of particles
/*! ComputeThermo calculates instantaneous thermodynamic properties and provides them for the logger.
    All computed values are stored in a GPUArray so that they can be accessed on the GPU without intermediate copies.
    Use the enum values in thermo_index to index the array and extract the properties of interest. Convenience functions
    are provided for accessing the values on the CPU. Certain properties, loke ndof and num_particles are always known
    and there is no need for them to be accessible via the GPUArray.

    Computed quantities available in the GPUArray:
     - temperature of the group from translational degrees of freedom
     - temperature of the group from rotational degrees of freedom
     - pressure (valid for the all group)
     - translational kinetic energy
     - rotational kinetic energy
     - potential energy

    Values available all the time
     - number of degrees of freedom (ndof)
     - number of particles in the group

    ndof is utilized in calculating the temperature from the kinetic energy. setNDOF() changes it to any value
    the user desires (the default is one!). In standard usage, the python interface queries the number of degrees
    of freedom from the integrators and sets that value for each ComputeThermo so that it is always correct.

    All quantities are made available for the logger. ComputerThermo can be given a suffix which it will append
    to each quantity provided to the logger. Typical usage is to provide _groupname as the suffix so that properties
    of different groups can be logged seperately (e.g. temperature_group1 and temperature_group2).

    \ingroup computes
*/
class ComputeThermo : public Compute
    {
    public:
        //! Constructs the compute
        ComputeThermo(boost::shared_ptr<SystemDefinition> sysdef,
                      boost::shared_ptr<ParticleGroup> group,
                      const std::string& suffix = std::string(""));

        //! Destructor
        virtual ~ComputeThermo();

        //! Compute the temperature
        virtual void compute(unsigned int timestep);

        //! Change the number of degrees of freedom
        void setNDOF(unsigned int ndof);

        //! Get the number of degrees of freedom
        unsigned int getNDOF()
            {
            return m_ndof;
            }

        //! Change the number of degrees of freedom
        void setRotationalNDOF(unsigned int ndof)
            {
            m_ndof_rot = ndof;
            }

        //! Get the number of degrees of freedom
        unsigned int getRotationalNDOF()
            {
            return m_ndof_rot;
            }

        //! Returns the overall temperature last computed by compute()
        /*! \returns Instantaneous overall temperature of the system
         */
        Scalar getTemperature()
        {
            #ifdef ENABLE_MPI
            if (!m_properties_reduced) reduceProperties();
            #endif
            ArrayHandle<Scalar> h_properties(m_properties, access_location::host, access_mode::read);
            Scalar prefactor = Scalar(2.0)/(m_ndof + m_ndof_rot);
            return prefactor*(h_properties.data[thermo_index::translational_kinetic_energy] +
                              h_properties.data[thermo_index::rotational_kinetic_energy]);
        }

        //! Returns the translational temperature last computed by compute()
        /*! \returns Instantaneous translational temperature of the system
        */
        Scalar getTranslationalTemperature()
            {
            #ifdef ENABLE_MPI
            if (!m_properties_reduced) reduceProperties();
            #endif
            ArrayHandle<Scalar> h_properties(m_properties, access_location::host, access_mode::read);
            return Scalar(2.0)/m_ndof*h_properties.data[thermo_index::translational_kinetic_energy];
            }

        //! Returns the rotational temperature last computed by compute()
        /*! \returns Instantaneous rotational temperature of the system
        */
        Scalar getRotationalTemperature()
            {
            #ifdef ENABLE_MPI
            if (!m_properties_reduced) reduceProperties();
            #endif
            // return NaN if the flags are not valid or we have no rotational DOF
            PDataFlags flags = m_pdata->getFlags();
            if (flags[pdata_flag::rotational_kinetic_energy] && m_ndof_rot)
                {
                ArrayHandle<Scalar> h_properties(m_properties, access_location::host, access_mode::read);
                return Scalar(2.0)/m_ndof_rot*h_properties.data[thermo_index::rotational_kinetic_energy];
                }
            else
                {
                return std::numeric_limits<Scalar>::quiet_NaN();
                }
            }

        //! Returns the pressure last computed by compute()
        /*! \returns Instantaneous pressure of the system
        */
        Scalar getPressure()
            {
            // return NaN if the flags are not valid
            PDataFlags flags = m_pdata->getFlags();
            if (flags[pdata_flag::isotropic_virial])
                {
                // return the pressure
                #ifdef ENABLE_MPI
                if (!m_properties_reduced) reduceProperties();
                #endif

                ArrayHandle<Scalar> h_properties(m_properties, access_location::host, access_mode::read);
                return h_properties.data[thermo_index::pressure];
                }
            else
                {
                return std::numeric_limits<Scalar>::quiet_NaN();
                }
            }

        //! Returns the translational kinetic energy last computed by compute()
        /*! \returns Instantaneous translational kinetic energy of the system
        */
        Scalar getTranslationalKineticEnergy()
            {
            #ifdef ENABLE_MPI
            if (!m_properties_reduced) reduceProperties();
            #endif

            ArrayHandle<Scalar> h_properties(m_properties, access_location::host, access_mode::read);
            return h_properties.data[thermo_index::translational_kinetic_energy];
            }

        //! Returns the rotational kinetic energy last computed by compute()
        /*! \returns Instantaneous rotational kinetic energy of the system
        */
        Scalar getRotationalKineticEnergy()
            {
            // return NaN if the flags are not valid
            PDataFlags flags = m_pdata->getFlags();
            if (flags[pdata_flag::rotational_kinetic_energy])
                {
                ArrayHandle<Scalar> h_properties(m_properties, access_location::host, access_mode::read);
                return h_properties.data[thermo_index::rotational_kinetic_energy];
                }
            else
                {
                return std::numeric_limits<Scalar>::quiet_NaN();
                }
            }

        //! Returns the total kinetic energy last computed by compute()
        /*! \returns Instantaneous total kinetic energy of the system
        */
        Scalar getKineticEnergy()
            {
            // return only translational component if the flags are not valid
            PDataFlags flags = m_pdata->getFlags();
            ArrayHandle<Scalar> h_properties(m_properties, access_location::host, access_mode::read);
            if (flags[pdata_flag::rotational_kinetic_energy])
                {
                return (h_properties.data[thermo_index::translational_kinetic_energy] +
                        h_properties.data[thermo_index::rotational_kinetic_energy]);
                }
            else
                {
                return h_properties.data[thermo_index::translational_kinetic_energy];
                }
            }

        //! Returns the potential energy last computed by compute()
        /*! \returns Instantaneous potential energy of the system, or NaN if the energy is not valid
        */
        Scalar getPotentialEnergy()
            {
            #ifdef ENABLE_MPI
            if (!m_properties_reduced) reduceProperties();
            #endif

            // return NaN if the flags are not valid
            PDataFlags flags = m_pdata->getFlags();
            if (flags[pdata_flag::potential_energy])
                {
                ArrayHandle<Scalar> h_properties(m_properties, access_location::host, access_mode::read);
                return h_properties.data[thermo_index::potential_energy];
                }
            else
                {
                return std::numeric_limits<Scalar>::quiet_NaN();
                }
            }

        //! Returns the upper triangular virial tensor last computed by compute()
        /*! \returns Instantaneous virial tensor, or virial tensor containing NaN entries if it is
            not available
        */
        PressureTensor getPressureTensor()
            {
            // return tensor of NaN's if flags are not valid
            PDataFlags flags = m_pdata->getFlags();
            PressureTensor p;
            if (flags[pdata_flag::pressure_tensor])
                {
                #ifdef ENABLE_MPI
                if (!m_properties_reduced) reduceProperties();
                #endif

                ArrayHandle<Scalar> h_properties(m_properties, access_location::host, access_mode::read);

                p.xx = h_properties.data[thermo_index::pressure_xx];
                p.xy = h_properties.data[thermo_index::pressure_xy];
                p.xz = h_properties.data[thermo_index::pressure_xz];
                p.yy = h_properties.data[thermo_index::pressure_yy];
                p.yz = h_properties.data[thermo_index::pressure_yz];
                p.zz = h_properties.data[thermo_index::pressure_zz];
                }
            else
                {
                p.xx = std::numeric_limits<Scalar>::quiet_NaN();
                p.xy = std::numeric_limits<Scalar>::quiet_NaN();
                p.xz = std::numeric_limits<Scalar>::quiet_NaN();
                p.yy = std::numeric_limits<Scalar>::quiet_NaN();
                p.yz = std::numeric_limits<Scalar>::quiet_NaN();
                p.zz = std::numeric_limits<Scalar>::quiet_NaN();
                }
            return p;
            }

        //! Get the gpu array of properties
        const GPUArray<Scalar>& getProperties()
            {
            #ifdef ENABLE_MPI
            if (!m_properties_reduced) reduceProperties();
            #endif

            return m_properties;
            }

        //! Returns a list of log quantities this compute calculates
        virtual std::vector< std::string > getProvidedLogQuantities();

        //! Calculates the requested log value and returns it
        virtual Scalar getLogValue(const std::string& quantity, unsigned int timestep);

    protected:
        boost::shared_ptr<ParticleGroup> m_group;     //!< Group to compute properties for
        GPUArray<Scalar> m_properties;  //!< Stores the computed properties
        unsigned int m_ndof;            //!< Stores the number of translational degrees of freedom in the system
        unsigned int m_ndof_rot;        //!< Stores the number of rotational degrees of freedom in the system
        std::vector<std::string> m_logname_list;  //!< Cache all generated logged quantities names

        //! Does the actual computation
        virtual void computeProperties();

        #ifdef ENABLE_MPI
        bool m_properties_reduced;      //!< True if properties have been reduced across MPI

        //! Reduce properties over MPI
        virtual void reduceProperties();
        #endif
    };

//! Exports the ComputeThermo class to python
void export_ComputeThermo();

#endif
