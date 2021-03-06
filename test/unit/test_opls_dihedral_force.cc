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


#include <iostream>

#include <boost/bind.hpp>
#include <boost/function.hpp>

#include "OPLSDihedralForceCompute.h"
#include "ConstForceCompute.h"
#ifdef ENABLE_CUDA
#include "OPLSDihedralForceComputeGPU.h"
#endif

#include <stdio.h>

#include "Initializers.h"
#include "SnapshotSystemData.h"

using namespace std;
using namespace boost;

//! Name the boost unit test module
#define BOOST_TEST_MODULE OPLSDihedralForceTests
#include "boost_utf_configure.h"

//! Typedef to make using the boost::function factory easier
typedef boost::function<boost::shared_ptr<OPLSDihedralForceCompute>  (boost::shared_ptr<SystemDefinition> sysdef)> dihedralforce_creator;

//! Perform some simple functionality tests of any BondForceCompute
void dihedral_force_basic_tests(dihedralforce_creator tf_creator, boost::shared_ptr<ExecutionConfiguration> exec_conf)
    {
    // start with the simplest possible test: 4 particles in a huge box with only one dihedral type - no dihedrals
    boost::shared_ptr<SystemDefinition> sysdef_4(new SystemDefinition(4, BoxDim(2.5), 1, 0, 0, 1, 0, exec_conf));
    boost::shared_ptr<ParticleData> pdata_4 = sysdef_4->getParticleData();

    pdata_4->setPosition(0,make_scalar3(1.0,0.0,0.0));
    pdata_4->setPosition(1,make_scalar3(1.0,0.5,0));
    pdata_4->setPosition(2,make_scalar3(0.7,0.3,-0.2));
    pdata_4->setPosition(3,make_scalar3(0,0.4,-0.6));

    // create the dihedral force compute to check
    boost::shared_ptr<OPLSDihedralForceCompute> fc_4 = tf_creator(sysdef_4);

    // k1 = 1.5, k2 = 6.2, k3 = 1.7, k4 = 3.0
    fc_4->setParams(0, 1.5, 6.2, 1.7, 3.0);

    // compute the force (should be 0) and check the results
    fc_4->compute(0);

    {
    GPUArray<Scalar4>& force_array_1 =  fc_4->getForceArray();
    GPUArray<Scalar>& virial_array_1 =  fc_4->getVirialArray();
    unsigned int pitch = 0;
    ArrayHandle<Scalar4> h_force_1(force_array_1,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_1(virial_array_1,access_location::host,access_mode::read);

    // check that the force is correct, it should be 0 since we haven't created any dihedrals yet
    MY_BOOST_CHECK_SMALL(h_force_1.data[0].x, tol);
    MY_BOOST_CHECK_SMALL(h_force_1.data[0].y, tol);
    MY_BOOST_CHECK_SMALL(h_force_1.data[0].z, tol);
    MY_BOOST_CHECK_SMALL(h_force_1.data[0].w, tol);
    MY_BOOST_CHECK_SMALL(h_virial_1.data[0*pitch], tol);
    MY_BOOST_CHECK_SMALL(h_virial_1.data[1*pitch], tol);
    MY_BOOST_CHECK_SMALL(h_virial_1.data[2*pitch], tol);
    MY_BOOST_CHECK_SMALL(h_virial_1.data[3*pitch], tol);
    MY_BOOST_CHECK_SMALL(h_virial_1.data[4*pitch], tol);
    MY_BOOST_CHECK_SMALL(h_virial_1.data[5*pitch], tol);
    }

    // add a dihedral and check the force again
    sysdef_4->getDihedralData()->addBondedGroup(Dihedral(0,0,1,2,3)); // add type 0 dihedral bewtween atoms 0-1-2-3
    fc_4->compute(1);

    {
    // this time there should be a force
    GPUArray<Scalar4>& force_array_2 =  fc_4->getForceArray();
    GPUArray<Scalar>& virial_array_2 =  fc_4->getVirialArray();
    unsigned int pitch = virial_array_2.getPitch();
    ArrayHandle<Scalar4> h_force_2(force_array_2,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_2(virial_array_2,access_location::host,access_mode::read);

    MY_BOOST_CHECK_CLOSE(h_force_2.data[0].x, 6.40868096, tol);
    MY_BOOST_CHECK_SMALL(h_force_2.data[0].y, tol);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[0].z, -9.61302145, tol);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[0].w, 0.07393705, tol);
    MY_BOOST_CHECK_SMALL(h_virial_2.data[0*pitch+0]
                        +h_virial_2.data[3*pitch+0]
                        +h_virial_2.data[5*pitch+0], tol);

    MY_BOOST_CHECK_CLOSE(h_force_2.data[1].x, 5.77846043, tol);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[1].y, 1.68346581, tol);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[1].z, -10.35115646, tol);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[1].w, 0.07393705, tol);
    MY_BOOST_CHECK_SMALL(h_virial_2.data[0*pitch+1]
                        +h_virial_2.data[3*pitch+1]
                        +h_virial_2.data[5*pitch+1], tol);

    MY_BOOST_CHECK_CLOSE(h_force_2.data[2].x, -17.48694118, tol);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[2].y, -2.74342577, tol);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[2].z, 28.97383755, tol);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[2].w, 0.07393705, tol);
    MY_BOOST_CHECK_SMALL(h_virial_2.data[0*pitch+2]
                        +h_virial_2.data[3*pitch+2]
                        +h_virial_2.data[5*pitch+2], tol);

    MY_BOOST_CHECK_CLOSE(h_force_2.data[3].x, 5.29979978, tol);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[3].y, 1.05995995, loose_tol);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[3].z, -9.00965963, tol);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[3].w, 0.07393705, tol);
    MY_BOOST_CHECK_SMALL(h_virial_2.data[0*pitch+3]
                        +h_virial_2.data[3*pitch+3]
                        +h_virial_2.data[5*pitch+3], tol);
    }

    // rearrange the two particles in memory and see if they are properly updated
    {
    ArrayHandle<Scalar4> h_pos(pdata_4->getPositions(), access_location::host, access_mode::readwrite);
    ArrayHandle<unsigned int> h_tag(pdata_4->getTags(), access_location::host, access_mode::readwrite);
    ArrayHandle<unsigned int> h_rtag(pdata_4->getRTags(), access_location::host, access_mode::readwrite);

    std::swap(h_pos.data[0],h_pos.data[1]);
    std::swap(h_tag.data[0],h_tag.data[1]);
    std::swap(h_rtag.data[0], h_rtag.data[1]);
    }

    // notify that we made the sort
    pdata_4->notifyParticleSort();
    // recompute at the same timestep, the forces should still be updated
    fc_4->compute(1);

    {
    GPUArray<Scalar4>& force_array_3 =  fc_4->getForceArray();
    GPUArray<Scalar>& virial_array_3 =  fc_4->getVirialArray();
    unsigned int pitch = virial_array_3.getPitch();
    ArrayHandle<Scalar4> h_force_3(force_array_3,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_3(virial_array_3,access_location::host,access_mode::read);

    MY_BOOST_CHECK_CLOSE(h_force_3.data[1].x, 6.40868096, tol);
    MY_BOOST_CHECK_SMALL(h_force_3.data[1].y, tol);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[1].z, -9.61302145, tol);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[1].w, 0.07393705, tol);
    MY_BOOST_CHECK_SMALL(h_virial_3.data[0*pitch+0]
                        +h_virial_3.data[3*pitch+0]
                        +h_virial_3.data[5*pitch+0], tol);

    MY_BOOST_CHECK_CLOSE(h_force_3.data[0].x, 5.77846043, tol);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[0].y, 1.68346581, tol);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[0].z, -10.35115646, tol);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[0].w, 0.07393705, tol);
    MY_BOOST_CHECK_SMALL(h_virial_3.data[0*pitch+1]
                        +h_virial_3.data[3*pitch+1]
                        +h_virial_3.data[5*pitch+1], tol);

    MY_BOOST_CHECK_CLOSE(h_force_3.data[2].x, -17.48694118, tol);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[2].y, -2.74342577, tol);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[2].z, 28.97383755, tol);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[2].w, 0.07393705, tol);
    MY_BOOST_CHECK_SMALL(h_virial_3.data[0*pitch+2]
                        +h_virial_3.data[3*pitch+2]
                        +h_virial_3.data[5*pitch+2], tol);

    MY_BOOST_CHECK_CLOSE(h_force_3.data[3].x, 5.29979978, tol);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[3].y, 1.05995995, loose_tol);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[3].z, -9.00965963, tol);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[3].w, 0.07393705, tol);
    MY_BOOST_CHECK_SMALL(h_virial_3.data[0*pitch+3]
                        +h_virial_3.data[3*pitch+3]
                        +h_virial_3.data[5*pitch+3], tol);
    }

    {
    ArrayHandle<Scalar4> h_pos(pdata_4->getPositions(), access_location::host, access_mode::readwrite);

    // translate all particles and wrap them back into the box
    Scalar3 shift = make_scalar3(.5,0,1);
    int3 img = make_int3(0,0,0);
    const BoxDim& box = pdata_4->getBox();
    h_pos.data[0] = make_scalar4(h_pos.data[0].x+shift.x, h_pos.data[0].y+shift.y,h_pos.data[0].z + shift.z,h_pos.data[0].w);
    box.wrap(h_pos.data[0], img);
    h_pos.data[1] = make_scalar4(h_pos.data[1].x+shift.x, h_pos.data[1].y+shift.y,h_pos.data[1].z + shift.z,h_pos.data[1].w);
    box.wrap(h_pos.data[1], img);
    h_pos.data[2] = make_scalar4(h_pos.data[2].x+shift.x, h_pos.data[2].y+shift.y,h_pos.data[2].z + shift.z,h_pos.data[2].w);
    box.wrap(h_pos.data[2], img);
    h_pos.data[3] = make_scalar4(h_pos.data[3].x+shift.x, h_pos.data[3].y+shift.y,h_pos.data[3].z + shift.z,h_pos.data[3].w);
    box.wrap(h_pos.data[3], img);
    }

    fc_4->compute(2);
    {
    GPUArray<Scalar4>& force_array_4 =  fc_4->getForceArray();
    GPUArray<Scalar>& virial_array_4 =  fc_4->getVirialArray();
    unsigned int pitch = virial_array_4.getPitch();
    ArrayHandle<Scalar4> h_force_4(force_array_4,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_4(virial_array_4,access_location::host,access_mode::read);

    MY_BOOST_CHECK_CLOSE(h_force_4.data[1].x, 6.40868096, tol);
    MY_BOOST_CHECK_SMALL(h_force_4.data[1].y, tol);
    MY_BOOST_CHECK_CLOSE(h_force_4.data[1].z, -9.61302145, tol);
    MY_BOOST_CHECK_CLOSE(h_force_4.data[1].w, 0.07393705, tol);
    MY_BOOST_CHECK_SMALL(h_virial_4.data[0*pitch+0]
                        +h_virial_4.data[3*pitch+0]
                        +h_virial_4.data[5*pitch+0], tol);

    MY_BOOST_CHECK_CLOSE(h_force_4.data[0].x, 5.77846043, tol);
    MY_BOOST_CHECK_CLOSE(h_force_4.data[0].y, 1.68346581, tol);
    MY_BOOST_CHECK_CLOSE(h_force_4.data[0].z, -10.35115646, tol);
    MY_BOOST_CHECK_CLOSE(h_force_4.data[0].w, 0.07393705, tol);
    MY_BOOST_CHECK_SMALL(h_virial_4.data[0*pitch+1]
                        +h_virial_4.data[3*pitch+1]
                        +h_virial_4.data[5*pitch+1], tol);

    MY_BOOST_CHECK_CLOSE(h_force_4.data[2].x, -17.48694118, tol);
    MY_BOOST_CHECK_CLOSE(h_force_4.data[2].y, -2.74342577, tol);
    MY_BOOST_CHECK_CLOSE(h_force_4.data[2].z, 28.97383755, tol);
    MY_BOOST_CHECK_CLOSE(h_force_4.data[2].w, 0.07393705, tol);
    MY_BOOST_CHECK_SMALL(h_virial_4.data[0*pitch+2]
                        +h_virial_4.data[3*pitch+2]
                        +h_virial_4.data[5*pitch+2], tol);

    MY_BOOST_CHECK_CLOSE(h_force_4.data[3].x, 5.29979978, tol);
    MY_BOOST_CHECK_CLOSE(h_force_4.data[3].y, 1.05995995, loose_tol);
    MY_BOOST_CHECK_CLOSE(h_force_4.data[3].z, -9.00965963, tol);
    MY_BOOST_CHECK_CLOSE(h_force_4.data[3].w, 0.07393705, tol);
    MY_BOOST_CHECK_SMALL(h_virial_4.data[0*pitch+3]
                        +h_virial_4.data[3*pitch+3]
                        +h_virial_4.data[5*pitch+3], tol);
    }

    // now test a position with a negative dihedral angle
    pdata_4->setPosition(0,make_scalar3(1.0,0.0,0.0));
    pdata_4->setPosition(1,make_scalar3(1.0,0.5,0));
    pdata_4->setPosition(2,make_scalar3(0.7,0.3,0.3));
    pdata_4->setPosition(3,make_scalar3(0,0.4,0.6));
    fc_4->compute(3);

    {
    GPUArray<Scalar4>& force_array_5 =  fc_4->getForceArray();
    GPUArray<Scalar>& virial_array_5 =  fc_4->getVirialArray();
    unsigned int pitch = virial_array_5.getPitch();
    ArrayHandle<Scalar4> h_force_5(force_array_5,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_5(virial_array_5,access_location::host,access_mode::read);

    MY_BOOST_CHECK_CLOSE(h_force_5.data[1].x, 19.30099804, tol);
    MY_BOOST_CHECK_SMALL(h_force_5.data[1].y, tol);
    MY_BOOST_CHECK_CLOSE(h_force_5.data[1].z, 19.30099804, tol);
    MY_BOOST_CHECK_CLOSE(h_force_5.data[1].w, 1.51788878, tol);
    MY_BOOST_CHECK_SMALL(h_virial_5.data[0*pitch+1]
                        +h_virial_5.data[3*pitch+1]
                        +h_virial_5.data[5*pitch+1], tol);

    MY_BOOST_CHECK_CLOSE(h_force_5.data[0].x, 2.37592759, tol);
    MY_BOOST_CHECK_CLOSE(h_force_5.data[0].y, 17.20499296, tol);
    MY_BOOST_CHECK_CLOSE(h_force_5.data[0].z, 13.84592290, tol);
    MY_BOOST_CHECK_CLOSE(h_force_5.data[0].w, 1.51788878, tol);
    MY_BOOST_CHECK_SMALL(h_virial_5.data[0*pitch+0]
                        +h_virial_5.data[3*pitch+0]
                        +h_virial_5.data[5*pitch+0], tol);

    MY_BOOST_CHECK_CLOSE(h_force_5.data[2].x, -31.81558221, tol);
    MY_BOOST_CHECK_CLOSE(h_force_5.data[2].y, -30.72320175, tol);
    MY_BOOST_CHECK_CLOSE(h_force_5.data[2].z, -52.29771667, tol);
    MY_BOOST_CHECK_CLOSE(h_force_5.data[2].w, 1.51788878, tol);
    MY_BOOST_CHECK_SMALL(h_virial_5.data[0*pitch+2]
                        +h_virial_5.data[3*pitch+2]
                        +h_virial_5.data[5*pitch+2], tol);

    MY_BOOST_CHECK_CLOSE(h_force_5.data[3].x, 10.13865656, tol);
    MY_BOOST_CHECK_CLOSE(h_force_5.data[3].y, 13.51820875, tol);
    MY_BOOST_CHECK_CLOSE(h_force_5.data[3].z, 19.15079572, tol);
    MY_BOOST_CHECK_CLOSE(h_force_5.data[3].w, 1.51788878, tol);
    MY_BOOST_CHECK_SMALL(h_virial_5.data[0*pitch+3]
                        +h_virial_5.data[3*pitch+3]
                        +h_virial_5.data[5*pitch+3], tol);
    }

    // test an 8-particle system with two non-overlapping dihedrals
    boost::shared_ptr<SystemDefinition> sysdef_8(new SystemDefinition(8, BoxDim(50.0), 1, 0, 0, 2, 0, exec_conf));
    boost::shared_ptr<ParticleData> pdata_8 = sysdef_8->getParticleData();

    pdata_8->setPosition(0, make_scalar3(1.0,0.0,0.0));
    pdata_8->setPosition(1, make_scalar3(3.0,1.2,2.1));
    pdata_8->setPosition(2, make_scalar3(0.0,0.7,3.2));
    pdata_8->setPosition(3, make_scalar3(4.7,-0.5,-0.3));
    pdata_8->setPosition(4, make_scalar3(4.8,1.1,0.0));
    pdata_8->setPosition(5, make_scalar3(3.8,0.0,-2.0));
    pdata_8->setPosition(6, make_scalar3(0.0,2.9,-1.7));
    pdata_8->setPosition(7, make_scalar3(-2.0,0.3,0.7));

    boost::shared_ptr<OPLSDihedralForceCompute> fc_8 = tf_creator(sysdef_8);
    fc_8->setParams(0, 2.0, 3.0, 4.0, 5.0);
    fc_8->setParams(1, 5.2, 4.2, 3.2, 1.2);

    sysdef_8->getDihedralData()->addBondedGroup(Dihedral(0, 0,1,2,3));
    sysdef_8->getDihedralData()->addBondedGroup(Dihedral(1, 4,5,6,7));

    fc_8->compute(0);

    {
    // check that the forces are correctly computed
    GPUArray<Scalar4>& force_array_6 =  fc_8->getForceArray();
    GPUArray<Scalar>& virial_array_6 =  fc_8->getVirialArray();
    unsigned int pitch = virial_array_6.getPitch();
    ArrayHandle<Scalar4> h_force_6(force_array_6,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_6(virial_array_6,access_location::host,access_mode::read);

    MY_BOOST_CHECK_CLOSE(h_force_6.data[0].x, 0.42570372, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[0].y, -1.52678552, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[0].z, 0.46701674, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[0].w, 2.09533111, tol);
    MY_BOOST_CHECK_SMALL(h_virial_6.data[0*pitch+0]
                        +h_virial_6.data[3*pitch+0]
                        +h_virial_6.data[5*pitch+0], tol);

    MY_BOOST_CHECK_CLOSE(h_force_6.data[1].x, 0.80582443, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[1].y, -0.93440123, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[1].z, 1.77297516, tol); // 5.650116
    MY_BOOST_CHECK_CLOSE(h_force_6.data[1].w, 2.09533111, tol);
    MY_BOOST_CHECK_SMALL(h_virial_6.data[0*pitch+1]
                        +h_virial_6.data[3*pitch+1]
                        +h_virial_6.data[5*pitch+1], tol);

    MY_BOOST_CHECK_CLOSE(h_force_6.data[2].x, -0.59432265, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[2].y, 1.35489836, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[2].z, -1.00501706, tol); //
    MY_BOOST_CHECK_CLOSE(h_force_6.data[2].w, 2.09533111, tol);
    MY_BOOST_CHECK_SMALL(h_virial_6.data[0*pitch+2]
                        +h_virial_6.data[3*pitch+2]
                        +h_virial_6.data[5*pitch+2], tol);

    MY_BOOST_CHECK_CLOSE(h_force_6.data[3].x, -0.63720550, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[3].y, 1.10628839, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[3].z, -1.23497484, tol); //
    MY_BOOST_CHECK_CLOSE(h_force_6.data[3].w, 2.09533111, tol);
    MY_BOOST_CHECK_SMALL(h_virial_6.data[0*pitch+3]
                        +h_virial_6.data[3*pitch+3]
                        +h_virial_6.data[5*pitch+3], tol);

    MY_BOOST_CHECK_CLOSE(h_force_6.data[4].x, -0.40576686, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[4].y, -0.58602527, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[4].z, 0.52519733, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[4].w, 2.10324711, tol);
    MY_BOOST_CHECK_SMALL(h_virial_6.data[0*pitch+4]
                        +h_virial_6.data[3*pitch+4]
                        +h_virial_6.data[5*pitch+4], tol);

    MY_BOOST_CHECK_CLOSE(h_force_6.data[5].x, 0.41329413, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[5].y, 0.59437186, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[5].z, -0.51053556, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[5].w, 2.10324711, tol);
    MY_BOOST_CHECK_SMALL(h_virial_6.data[0*pitch+5]
                        +h_virial_6.data[3*pitch+5]
                        +h_virial_6.data[5*pitch+5], tol);

    MY_BOOST_CHECK_CLOSE(h_force_6.data[6].x, -0.22370415,tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[6].y, -0.24630873, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[6].z, -0.45260150, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[6].w, 2.10324711, tol);
    MY_BOOST_CHECK_SMALL(h_virial_6.data[0*pitch+6]
                        +h_virial_6.data[3*pitch+6]
                        +h_virial_6.data[5*pitch+6], tol);

    MY_BOOST_CHECK_CLOSE(h_force_6.data[7].x, 0.21617688, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[7].y, 0.23796214, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[7].z, 0.43793972, tol);
    MY_BOOST_CHECK_CLOSE(h_force_6.data[7].w, 2.10324711, tol);
    MY_BOOST_CHECK_SMALL(h_virial_6.data[0*pitch+7]
                        +h_virial_6.data[3*pitch+7]
                        +h_virial_6.data[5*pitch+7], tol);
    }

    // test a 5-particle system with one dihedral type on two overlapping sets of particles
    boost::shared_ptr<SystemDefinition> sysdef_5(new SystemDefinition(5, BoxDim(50.0), 1, 0, 0, 1, 0, exec_conf));
    boost::shared_ptr<ParticleData> pdata_5 = sysdef_5->getParticleData();

    pdata_5->setPosition(0, make_scalar3(1.0,0.0,0.0));
    pdata_5->setPosition(1, make_scalar3(3.0,1.2,2.1));
    pdata_5->setPosition(2, make_scalar3(0.0,0.7,3.2));
    pdata_5->setPosition(3, make_scalar3(4.7,-0.5,-0.3));
    pdata_5->setPosition(4, make_scalar3(4.8,1.1,0.0));

    // build the dihedral force compute and try it out
    boost::shared_ptr<OPLSDihedralForceCompute> fc_5 = tf_creator(sysdef_5);
    fc_5->setParams(0, 1.2, 3.3, 4.2, 6.4);

    sysdef_5->getDihedralData()->addBondedGroup(Dihedral(0, 0,1,2,3));
    sysdef_5->getDihedralData()->addBondedGroup(Dihedral(0, 1,2,3,4));

    fc_5->compute(0);

    {
    GPUArray<Scalar4>& force_array_7 =  fc_5->getForceArray();
    GPUArray<Scalar>& virial_array_7 =  fc_5->getVirialArray();
    unsigned int pitch = virial_array_7.getPitch();
    ArrayHandle<Scalar4> h_force_7(force_array_7,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_7(virial_array_7,access_location::host,access_mode::read);

    MY_BOOST_CHECK_CLOSE(h_force_7.data[0].x, 0.65834052, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[0].y, -2.36113691, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[0].z, 0.72223011, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[0].w, 2.21706239, tol);
    MY_BOOST_CHECK_SMALL(h_virial_7.data[0*pitch+0]
                        +h_virial_7.data[3*pitch+0]
                        +h_virial_7.data[5*pitch+0], tol);

    MY_BOOST_CHECK_CLOSE(h_force_7.data[1].x, -0.73383345, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[1].y, 1.99259791, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[1].z, -1.09563763, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[1].w, 4.37805164, tol);
    MY_BOOST_CHECK_SMALL(h_virial_7.data[0*pitch+1]
                        +h_virial_7.data[3*pitch+1]
                        +h_virial_7.data[5*pitch+1], tol);

    MY_BOOST_CHECK_CLOSE(h_force_7.data[2].x, -0.09368793, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[2].y, 0.38994288, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[2].z, 0.13888332, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[2].w, 4.37805164, tol);
    MY_BOOST_CHECK_SMALL(h_virial_7.data[0*pitch+2]
                        +h_virial_7.data[3*pitch+2]
                        +h_virial_7.data[5*pitch+2], tol);

    MY_BOOST_CHECK_CLOSE(h_force_7.data[3].x, -2.61415944, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[3].y, 0.91345850, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[3].z, -3.82362845, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[3].w, 4.37805164, tol);
    MY_BOOST_CHECK_SMALL(h_virial_7.data[0*pitch+3]
                        +h_virial_7.data[3*pitch+3]
                        +h_virial_7.data[5*pitch+3], tol);

    MY_BOOST_CHECK_CLOSE(h_force_7.data[4].x, 2.78334029, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[4].y, -0.93486239, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[4].z, 4.05815265, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[4].w, 2.16098925, tol);
    MY_BOOST_CHECK_SMALL(h_virial_7.data[0*pitch+4]
                        +h_virial_7.data[3*pitch+4]
                        +h_virial_7.data[5*pitch+4], tol);
    }
    }

//! Compares the output of two OPLSDihedralForceComputes
void dihedral_force_comparison_tests(dihedralforce_creator tf_creator1,
                                     dihedralforce_creator tf_creator2,
                                     boost::shared_ptr<ExecutionConfiguration> exec_conf)
    {
    const unsigned int N = 1000;

    // create a particle system to sum forces on
    // just randomly place particles. We don't really care how huge the bond forces get: this is just a unit test
    RandomInitializer rand_init(N, Scalar(0.2), Scalar(0.9), "A");
    boost::shared_ptr< SnapshotSystemData<Scalar> > snap = rand_init.getSnapshot();
    snap->dihedral_data.type_mapping.push_back("A");
    boost::shared_ptr<SystemDefinition> sysdef(new SystemDefinition(snap, exec_conf));

    boost::shared_ptr<OPLSDihedralForceCompute> fc1 = tf_creator1(sysdef);
    boost::shared_ptr<OPLSDihedralForceCompute> fc2 = tf_creator2(sysdef);
    fc1->setParams(0, 1.1, 2.2, 4.5, 3.6);
    fc2->setParams(0, 1.1, 2.2, 4.5, 3.6);

    // add dihedrals
    for (unsigned int i = 0; i < N-3; i++)
        {
        sysdef->getDihedralData()->addBondedGroup(Dihedral(0, i, i+1,i+2, i+3));
        }

    // compute the forces
    fc1->compute(0);
    fc2->compute(0);

    // verify that the forces are identical (within roundoff errors)
    {
    GPUArray<Scalar4>& force_array_7 =  fc1->getForceArray();
    GPUArray<Scalar>& virial_array_7 =  fc1->getVirialArray();
    unsigned int pitch = virial_array_7.getPitch();
    ArrayHandle<Scalar4> h_force_7(force_array_7,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_7(virial_array_7,access_location::host,access_mode::read);
    GPUArray<Scalar4>& force_array_8 =  fc2->getForceArray();
    GPUArray<Scalar>& virial_array_8 =  fc2->getVirialArray();
    ArrayHandle<Scalar4> h_force_8(force_array_8,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_8(virial_array_8,access_location::host,access_mode::read);

    // compare average deviation between the two computes
    double deltaf2 = 0.0;
    double deltape2 = 0.0;
    double deltav2[6];
    for (unsigned int i = 0; i < 6; i++)
        deltav2[i] = 0.0;

    for (unsigned int i = 0; i < N; i++)
        {
        deltaf2 += double(h_force_8.data[i].x - h_force_7.data[i].x) * double(h_force_8.data[i].x - h_force_7.data[i].x);
        deltaf2 += double(h_force_8.data[i].y - h_force_7.data[i].y) * double(h_force_8.data[i].y - h_force_7.data[i].y);
        deltaf2 += double(h_force_8.data[i].z - h_force_7.data[i].z) * double(h_force_8.data[i].z - h_force_7.data[i].z);
        deltape2 += double(h_force_8.data[i].w - h_force_7.data[i].w) * double(h_force_8.data[i].w - h_force_7.data[i].w);
        for (unsigned int j = 0; j < 6; j++)
            deltav2[j] += double(h_virial_8.data[j*pitch+i] - h_virial_7.data[j*pitch+i]) * double(h_virial_8.data[j*pitch+i] - h_virial_7.data[j*pitch+i]);

        // also check that each individual calculation is somewhat close
        }
    deltaf2 /= double(N);
    deltape2 /= double(N);
    for (unsigned int j = 0; j < 6; j++)
        deltav2[j] /= double(N);

    BOOST_CHECK_SMALL(deltaf2, double(tol_small));
    BOOST_CHECK_SMALL(deltape2, double(tol_small));
    BOOST_CHECK_SMALL(deltav2[0], double(tol_small));
    BOOST_CHECK_SMALL(deltav2[1], double(tol_small));
    BOOST_CHECK_SMALL(deltav2[2], double(tol_small));
    BOOST_CHECK_SMALL(deltav2[3], double(tol_small));
    BOOST_CHECK_SMALL(deltav2[4], double(tol_small));
    BOOST_CHECK_SMALL(deltav2[5], double(tol_small));
    }
    }

//! OPLSDihedralForceCompute creator for dihedral_force_basic_tests()
boost::shared_ptr<OPLSDihedralForceCompute> base_class_tf_creator(boost::shared_ptr<SystemDefinition> sysdef)
    {
    return boost::shared_ptr<OPLSDihedralForceCompute>(new OPLSDihedralForceCompute(sysdef));
    }

#ifdef ENABLE_CUDA
//! DihedralForceCompute creator for bond_force_basic_tests()
boost::shared_ptr<OPLSDihedralForceCompute> gpu_tf_creator(boost::shared_ptr<SystemDefinition> sysdef)
    {
    return boost::shared_ptr<OPLSDihedralForceCompute>(new OPLSDihedralForceComputeGPU(sysdef));
    }
#endif

//! boost test case for dihedral forces on the CPU
BOOST_AUTO_TEST_CASE( OPLSDihedralForceCompute_basic )
    {
    printf(" IN BOOST_AUTO_TEST_CASE: CPU \n");
    dihedralforce_creator tf_creator = bind(base_class_tf_creator, _1);
    dihedral_force_basic_tests(tf_creator, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)));
    }

#ifdef ENABLE_CUDA
//! boost test case for dihedral forces on the GPU
BOOST_AUTO_TEST_CASE( OPLSDihedralForceComputeGPU_basic )
    {
    printf(" IN BOOST_AUTO_TEST_CASE: GPU \n");
    dihedralforce_creator tf_creator = bind(gpu_tf_creator, _1);
    dihedral_force_basic_tests(tf_creator, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)));
    }

//! boost test case for comparing bond GPU and CPU BondForceComputes
BOOST_AUTO_TEST_CASE( OPLSDihedralForceComputeGPU_compare )
    {
    dihedralforce_creator tf_creator_gpu = bind(gpu_tf_creator, _1);
    dihedralforce_creator tf_creator = bind(base_class_tf_creator, _1);
    dihedral_force_comparison_tests(tf_creator, tf_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)));
    }

//! boost test case for comparing calculation on the CPU to multi-gpu ones
BOOST_AUTO_TEST_CASE( OPLSDihedralForce_MultiGPU_compare)
    {
    boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));

    dihedralforce_creator tf_creator_gpu = bind(gpu_tf_creator, _1);
    dihedralforce_creator tf_creator = bind(base_class_tf_creator, _1);
    dihedral_force_comparison_tests(tf_creator, tf_creator_gpu, exec_conf);
    }
#endif
