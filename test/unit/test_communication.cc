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

#ifdef ENABLE_MPI

//! name the boost unit test module
#define BOOST_TEST_MODULE CommunicationTests

// this has to be included after naming the test module
#include "boost_utf_configure.h"

#include "System.h"

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

#include "ExecutionConfiguration.h"
#include "Communicator.h"

#include "ConstForceCompute.h"
#include "TwoStepNVE.h"
#include "IntegratorTwoStep.h"

#ifdef ENABLE_CUDA
#include "CommunicatorGPU.h"
#endif

#include <algorithm>

#define TO_TRICLINIC(v) dest_box.makeCoordinates(ref_box.makeFraction(make_scalar3(v.x,v.y,v.z)))
#define TO_POS4(v) make_scalar4(v.x,v.y,v.z,h_pos.data[rtag].w)
#define FROM_TRICLINIC(v) ref_box.makeCoordinates(dest_box.makeFraction(make_scalar3(v.x,v.y,v.z)))

using namespace std;
using namespace boost;

//! Typedef for function that creates the Communnicator on the CPU or GPU
typedef boost::function<boost::shared_ptr<Communicator> (boost::shared_ptr<SystemDefinition> sysdef,
                                                  boost::shared_ptr<DomainDecomposition> decomposition)> communicator_creator;

boost::shared_ptr<Communicator> base_class_communicator_creator(boost::shared_ptr<SystemDefinition> sysdef,
                                                         boost::shared_ptr<DomainDecomposition> decomposition);

#ifdef ENABLE_CUDA
boost::shared_ptr<Communicator> gpu_communicator_creator(boost::shared_ptr<SystemDefinition> sysdef,
                                                  boost::shared_ptr<DomainDecomposition> decomposition);
#endif

void test_domain_decomposition(boost::shared_ptr<ExecutionConfiguration> exec_conf,
                               const BoxDim& box,
                               boost::shared_ptr<DomainDecomposition> decomposition)
{
    // this test needs to be run on eight processors
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    BOOST_REQUIRE_EQUAL(size,8);

    // create a system with eight particles
    boost::shared_ptr<SystemDefinition> sysdef(new SystemDefinition(8,           // number of particles
                                                             box,         // box dimensions
                                                             1,           // number of particle types
                                                             0,           // number of bond types
                                                             0,           // number of angle types
                                                             0,           // number of dihedral types
                                                             0,           // number of dihedral types
                                                             exec_conf));



    boost::shared_ptr<ParticleData> pdata(sysdef->getParticleData());
        {
        ArrayHandle<Scalar4> h_pos(pdata->getPositions(), access_location::host, access_mode::readwrite);

        // set up eight particles, one in every domain
        h_pos.data[0].x = -0.5;
        h_pos.data[0].y = -0.5;
        h_pos.data[0].z = -0.5;

        h_pos.data[1].x = 0.5;
        h_pos.data[1].y = -0.5;
        h_pos.data[1].z = -0.5;

        h_pos.data[2].x = -0.5;
        h_pos.data[2].y = 0.5;
        h_pos.data[2].z = -0.5;

        h_pos.data[3].x = 0.5;
        h_pos.data[3].y = 0.5;
        h_pos.data[3].z = -0.5;

        h_pos.data[4].x = -0.5;
        h_pos.data[4].y = -0.5;
        h_pos.data[4].z = 0.5;

        h_pos.data[5].x = 0.5;
        h_pos.data[5].y = -0.5;
        h_pos.data[5].z = 0.5;

        h_pos.data[6].x = -0.5;
        h_pos.data[6].y = 0.5;
        h_pos.data[6].z = 0.5;

        h_pos.data[7].x = 0.5;
        h_pos.data[7].y = 0.5;
        h_pos.data[7].z = 0.5;
        }

    SnapshotParticleData<Scalar> snap(8);
    pdata->takeSnapshot(snap);

    pdata->setDomainDecomposition(decomposition);

    // check that periodic flags are correctly set on the box
    BOOST_CHECK_EQUAL(pdata->getBox().getPeriodic().x, 0);
    BOOST_CHECK_EQUAL(pdata->getBox().getPeriodic().y, 0);
    BOOST_CHECK_EQUAL(pdata->getBox().getPeriodic().z, 0);

    pdata->initializeFromSnapshot(snap);

    // check that every domain has exactly one particle
    BOOST_CHECK_EQUAL(pdata->getN(), 1);

    // check that every particle ended up in the domain to where it belongs
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(0), 0);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(1), 1);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(2), 2);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(3), 3);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(4), 4);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(5), 5);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(6), 6);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(7), 7);

    // check that the positions have been transferred correctly
    Scalar3 pos = pdata->getPosition(0);
    BOOST_CHECK_CLOSE(pos.x, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.5, tol);

    pos = pdata->getPosition(1);
    BOOST_CHECK_CLOSE(pos.x, 0.5, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.5, tol);

    pos = pdata->getPosition(2);
    BOOST_CHECK_CLOSE(pos.x, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.y,  0.5, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.5, tol);

    pos = pdata->getPosition(3);
    BOOST_CHECK_CLOSE(pos.x,  0.5, tol);
    BOOST_CHECK_CLOSE(pos.y,  0.5, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.5, tol);

    pos = pdata->getPosition(4);
    BOOST_CHECK_CLOSE(pos.x, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.5, tol);

    pos = pdata->getPosition(5);
    BOOST_CHECK_CLOSE(pos.x,  0.5, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.5, tol);

    pos = pdata->getPosition(6);
    BOOST_CHECK_CLOSE(pos.x, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.y,  0.5, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.5, tol);

    pos = pdata->getPosition(7);
    BOOST_CHECK_CLOSE(pos.x,  0.5, tol);
    BOOST_CHECK_CLOSE(pos.y,  0.5, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.5, tol);
    }

void test_balanced_domain_decomposition(boost::shared_ptr<ExecutionConfiguration> exec_conf)
{
    // this test needs to be run on eight processors
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    BOOST_REQUIRE_EQUAL(size,8);

    // create a system with eight particles
    boost::shared_ptr<SystemDefinition> sysdef(new SystemDefinition(8,           // number of particles
                                                             BoxDim(2.0), // box dimensions
                                                             1,           // number of particle types
                                                             0,           // number of bond types
                                                             0,           // number of angle types
                                                             0,           // number of dihedral types
                                                             0,           // number of dihedral types
                                                             exec_conf));



    boost::shared_ptr<ParticleData> pdata(sysdef->getParticleData());
        {
        ArrayHandle<Scalar4> h_pos(pdata->getPositions(), access_location::host, access_mode::readwrite);

        // set up eight particles, one in every domain
        h_pos.data[0].x = -0.5;
        h_pos.data[0].y = -0.75;
        h_pos.data[0].z = -0.9;

        h_pos.data[1].x = 0.5;
        h_pos.data[1].y = -0.75;
        h_pos.data[1].z = -0.9;

        h_pos.data[2].x = -0.5;
        h_pos.data[2].y = -0.25;
        h_pos.data[2].z = -0.9;

        h_pos.data[3].x = 0.5;
        h_pos.data[3].y = -0.25;
        h_pos.data[3].z = -0.9;

        h_pos.data[4].x = -0.5;
        h_pos.data[4].y = -0.75;
        h_pos.data[4].z = 0.9;

        h_pos.data[5].x = 0.5;
        h_pos.data[5].y = -0.75;
        h_pos.data[5].z = 0.9;

        h_pos.data[6].x = -0.5;
        h_pos.data[6].y = -0.25;
        h_pos.data[6].z = 0.9;

        h_pos.data[7].x = 0.5;
        h_pos.data[7].y = -0.25;
        h_pos.data[7].z = 0.9;
        }

    // initialize a 2x2x2 domain decomposition on processor with rank 0
    std::vector<Scalar> fxs(1), fys(1), fzs(1);
    fxs[0] = Scalar(0.5);
    fys[0] = Scalar(0.35);
    fzs[0] = Scalar(0.8);

    SnapshotParticleData<Scalar> snap(8);
    pdata->takeSnapshot(snap);

    boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, pdata->getBox().getL(), fxs, fys, fzs));
    std::vector<Scalar> cum_frac_x = decomposition->getCumulativeFractions(0);
    MY_BOOST_CHECK_SMALL(cum_frac_x[0], tol);
    MY_BOOST_CHECK_CLOSE(cum_frac_x[1], 0.5, tol);
    MY_BOOST_CHECK_CLOSE(cum_frac_x[2], 1.0, tol);

    std::vector<Scalar> cum_frac_y = decomposition->getCumulativeFractions(1);
    MY_BOOST_CHECK_SMALL(cum_frac_y[0], tol);
    MY_BOOST_CHECK_CLOSE(cum_frac_y[1], 0.35, tol);
    MY_BOOST_CHECK_CLOSE(cum_frac_y[2], 1.0, tol);
    
    std::vector<Scalar> cum_frac_z = decomposition->getCumulativeFractions(2);
    MY_BOOST_CHECK_SMALL(cum_frac_z[0], tol);
    MY_BOOST_CHECK_CLOSE(cum_frac_z[1], 0.8, tol);
    MY_BOOST_CHECK_CLOSE(cum_frac_z[2], 1.0, tol);

    pdata->setDomainDecomposition(decomposition);

    // check that periodic flags are correctly set on the box
    BOOST_CHECK_EQUAL(pdata->getBox().getPeriodic().x, 0);
    BOOST_CHECK_EQUAL(pdata->getBox().getPeriodic().y, 0);
    BOOST_CHECK_EQUAL(pdata->getBox().getPeriodic().z, 0);

    pdata->initializeFromSnapshot(snap);

    // check that every domain has exactly one particle
    BOOST_CHECK_EQUAL(pdata->getN(), 1);

    // check that every particle ended up in the domain to where it belongs
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(0), 0);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(1), 1);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(2), 2);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(3), 3);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(4), 4);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(5), 5);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(6), 6);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(7), 7);

    // check that the positions have been transferred correctly
    Scalar3 pos = pdata->getPosition(0);
    BOOST_CHECK_CLOSE(pos.x, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.75, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.9, tol);

    pos = pdata->getPosition(1);
    BOOST_CHECK_CLOSE(pos.x, 0.5, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.75, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.9, tol);

    pos = pdata->getPosition(2);
    BOOST_CHECK_CLOSE(pos.x, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.y,  -0.25, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.9, tol);

    pos = pdata->getPosition(3);
    BOOST_CHECK_CLOSE(pos.x,  0.5, tol);
    BOOST_CHECK_CLOSE(pos.y,  -0.25, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.9, tol);

    pos = pdata->getPosition(4);
    BOOST_CHECK_CLOSE(pos.x, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.75, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.9, tol);

    pos = pdata->getPosition(5);
    BOOST_CHECK_CLOSE(pos.x,  0.5, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.75, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.9, tol);

    pos = pdata->getPosition(6);
    BOOST_CHECK_CLOSE(pos.x, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.y,  -0.25, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.9, tol);

    pos = pdata->getPosition(7);
    BOOST_CHECK_CLOSE(pos.x,  0.5, tol);
    BOOST_CHECK_CLOSE(pos.y,  -0.25, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.9, tol);


    // test that the simulation boxes are correct for each rank
    const BoxDim& box = pdata->getBox();
    const Scalar3 L = box.getL();
    const BoxDim& global_box = pdata->getGlobalBox();
    const Scalar3 global_L = global_box.getL();
    const uint3 my_pos = decomposition->getGridPos();
    // box size should be fractional width of global box
    if (my_pos.x == 0)
        {
        BOOST_CHECK_CLOSE(L.x, global_L.x * fxs[0], tol);
        }
    else
        {
        BOOST_CHECK_CLOSE(L.x, global_L.x * (Scalar(1.0) - fxs[0]), tol);
        }
    if (my_pos.y == 0)
        {
        BOOST_CHECK_CLOSE(L.y, global_L.y * fys[0], tol);
        }
    else
        {
        BOOST_CHECK_CLOSE(L.y, global_L.y * (Scalar(1.0) - fys[0]), tol);
        }
    if (my_pos.z == 0)
        {
        BOOST_CHECK_CLOSE(L.z, global_L.z * fzs[0], tol);
        }
    else
        {
        BOOST_CHECK_CLOSE(L.z, global_L.z * (Scalar(1.0) - fzs[0]), tol);
        }

    // box lower bound should be shifted if rank isn't the first slice along the dim
    const Scalar3 lo = box.getLo();
    Scalar3 check_lo = global_box.getLo();
    if (my_pos.x > 0)
        check_lo.x += fxs[0] * global_L.x;
    if (my_pos.y > 0)
        check_lo.y += fys[0] * global_L.y;
    if (my_pos.z > 0)
        check_lo.z += fzs[0] * global_L.z;
    BOOST_CHECK_CLOSE(lo.x, check_lo.x, tol);
    BOOST_CHECK_CLOSE(lo.y, check_lo.y, tol);
    BOOST_CHECK_CLOSE(lo.z, check_lo.z, tol);
    }

//! Test particle migration of Communicator
void test_communicator_migrate(communicator_creator comm_creator, boost::shared_ptr<ExecutionConfiguration> exec_conf,
    BoxDim dest_box)
    {
    // this test needs to be run on eight processors
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    BOOST_REQUIRE_EQUAL(size,8);

    BoxDim ref_box = BoxDim(2.0);
    // create a system with eight particles
    boost::shared_ptr<SystemDefinition> sysdef(new SystemDefinition(8,           // number of particles
                                                             dest_box, // box dimensions
                                                             1,           // number of particle types
                                                             0,           // number of bond types
                                                             0,           // number of angle types
                                                             0,           // number of dihedral types
                                                             0,           // number of dihedral types
                                                             exec_conf));



    boost::shared_ptr<ParticleData> pdata(sysdef->getParticleData());

    pdata->setPosition(0, TO_TRICLINIC(make_scalar3(-0.5,-0.5,-0.5)),false);
    pdata->setPosition(1, TO_TRICLINIC(make_scalar3( 0.5,-0.5,-0.5)),false);
    pdata->setPosition(2, TO_TRICLINIC(make_scalar3(-0.5, 0.5,-0.5)),false);
    pdata->setPosition(3, TO_TRICLINIC(make_scalar3( 0.5, 0.5,-0.5)),false);
    pdata->setPosition(4, TO_TRICLINIC(make_scalar3(-0.5,-0.5, 0.5)),false);
    pdata->setPosition(5, TO_TRICLINIC(make_scalar3( 0.5,-0.5, 0.5)),false);
    pdata->setPosition(6, TO_TRICLINIC(make_scalar3(-0.5, 0.5, 0.5)),false);
    pdata->setPosition(7, TO_TRICLINIC(make_scalar3( 0.5, 0.5, 0.5)),false);

    SnapshotParticleData<Scalar> snap(8);

    pdata->takeSnapshot(snap);

    // initialize a 2x2x2 domain decomposition on processor with rank 0
    boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, pdata->getBox().getL(),2,2,2));

    boost::shared_ptr<Communicator> comm = comm_creator(sysdef, decomposition);

    pdata->setDomainDecomposition(decomposition);

    pdata->initializeFromSnapshot(snap);

    // migrate atoms
    comm->migrateParticles();

    // check that every domain has exactly one particle
    BOOST_CHECK_EQUAL(pdata->getN(), 1);

    // check that every particle stayed where it was
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(0), 0);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(1), 1);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(2), 2);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(3), 3);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(4), 4);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(5), 5);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(6), 6);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(7), 7);

    // Now move particle 0 into domain 1
    pdata->setPosition(0, TO_TRICLINIC(make_scalar3(0.1,-0.5,-0.5)),false);
    // move particle 1 into domain 2
    pdata->setPosition(1, TO_TRICLINIC(make_scalar3(-0.2, 0.5, -0.5)),false);
    // move particle 2 into domain 3
    pdata->setPosition(2, TO_TRICLINIC(make_scalar3(0.2, 0.3, -0.5)),false);
    // move particle 3 into domain 4
    pdata->setPosition(3, TO_TRICLINIC(make_scalar3(-0.5, -0.3, 0.2)),false);
    // move particle 4 into domain 5
    pdata->setPosition(4, TO_TRICLINIC(make_scalar3(0.1, -0.3, 0.2)),false);
    // move particle 5 into domain 6
    pdata->setPosition(5, TO_TRICLINIC(make_scalar3(-0.2, 0.4, 0.2)),false);
    // move particle 6 into domain 7
    pdata->setPosition(6, TO_TRICLINIC(make_scalar3(0.6, 0.1, 0.2)),false);
    // move particle 7 into domain 0
    pdata->setPosition(7, TO_TRICLINIC(make_scalar3(-0.6, -0.1,- 0.2)),false);

    // migrate atoms
    comm->migrateParticles();

    // check that every particle has ended up in the right domain
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(0), 1);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(1), 2);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(2), 3);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(3), 4);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(4), 5);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(5), 6);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(6), 7);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(7), 0);

    // check positions
    Scalar3 pos = pdata->getPosition(0);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  0.1, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.5, tol);

    pos = pdata->getPosition(1);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, -0.2, tol);
    BOOST_CHECK_CLOSE(pos.y,  0.5, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.5, tol);

    pos = pdata->getPosition(2);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  0.2, tol);
    BOOST_CHECK_CLOSE(pos.y,  0.3, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.5, tol);

    pos = pdata->getPosition(3);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  -0.5, tol);
    BOOST_CHECK_CLOSE(pos.y,  -0.3, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.2, tol);

    pos = pdata->getPosition(4);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  0.1, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.3, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.2, tol);

    pos = pdata->getPosition(5);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, -0.2, tol);
    BOOST_CHECK_CLOSE(pos.y,  0.4, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.2, tol);

    pos = pdata->getPosition(6);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  0.6, tol);
    BOOST_CHECK_CLOSE(pos.y,  0.1, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.2, tol);

    pos = pdata->getPosition(7);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, -0.6, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.1, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.2, tol);

    //
    // check that that particles are correctly wrapped across the boundary
    //

    // particle 0 crosses the global boundary in +x direction
    pdata->setPosition(0, TO_TRICLINIC(make_scalar3(1.1,-0.5,-0.5)),false);
    //  particle 1 crosses the global bounadry in the -x direction
    pdata->setPosition(1, TO_TRICLINIC(make_scalar3(-1.1, 0.5, -0.5)),false);
    // particle 2 crosses the global boundary in the + y direction
    pdata->setPosition(2, TO_TRICLINIC(make_scalar3(0.2, 1.3, -0.5)),false);
    // particle 3 crosses the global boundary in the - y direction
    pdata->setPosition(3, TO_TRICLINIC(make_scalar3(-0.5, -1.5, 0.2)),false);
    // particle 4 crosses the global boundary in the + z direction
    pdata->setPosition(4, TO_TRICLINIC(make_scalar3(0.1, -0.3, 1.6)),false);
    // particle 5 crosses the global boundary in the + z direction and in the -x direction
    pdata->setPosition(5, TO_TRICLINIC(make_scalar3(-1.1, 0.4, 1.25)),false);
    // particle 6 crosses the global boundary in the + z direction and in the +x direction
    pdata->setPosition(6, TO_TRICLINIC(make_scalar3(1.3, 0.1, 1.05)),false);
    // particle 7 crosses the global boundary in the - z direction
    pdata->setPosition(7, TO_TRICLINIC(make_scalar3(-0.6, -0.1,- 1.5)),false);

    // migrate particles
    comm->migrateParticles();

    // check number of particles
    switch (exec_conf->getRank())
        {
        case 0:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 1:
            BOOST_CHECK_EQUAL(pdata->getN(), 2);
            break;
        case 2:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 3:
            BOOST_CHECK_EQUAL(pdata->getN(), 2);
            break;
        case 4:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 5:
            BOOST_CHECK_EQUAL(pdata->getN(), 0);
            break;
        case 6:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 7:
            BOOST_CHECK_EQUAL(pdata->getN(), 0);
            break;
        }

    // check that every particle has ended up in the right domain
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(0), 0);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(1), 3);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(2), 1);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(3), 6);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(4), 1);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(5), 3);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(6), 2);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(7), 4);

    // check positions (taking into account that particles should have been wrapped)
    pos = pdata->getPosition(0);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  -0.9, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.5, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.5, tol);

    pos = pdata->getPosition(1);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, 0.9, tol);
    BOOST_CHECK_CLOSE(pos.y,  0.5, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.5, tol);

    pos = pdata->getPosition(2);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  0.2, tol);
    BOOST_CHECK_CLOSE(pos.y,  -0.7, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.5, tol);

    pos = pdata->getPosition(3);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  -0.5, tol);
    BOOST_CHECK_CLOSE(pos.y,   0.5, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.2, tol);

    pos = pdata->getPosition(4);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  0.1, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.3, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.4, tol);

    pos = pdata->getPosition(5);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  0.9, tol);
    BOOST_CHECK_CLOSE(pos.y,  0.4, tol);
    BOOST_CHECK_CLOSE(pos.z,-0.75, tol);

    pos = pdata->getPosition(6);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, -0.7, tol);
    BOOST_CHECK_CLOSE(pos.y,  0.1, tol);
    BOOST_CHECK_CLOSE(pos.z,-0.95, tol);

    pos = pdata->getPosition(7);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, -0.6, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.1, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.5, tol);
    }

//! Test particle migration of Communicator
void test_communicator_balanced_migrate(communicator_creator comm_creator, boost::shared_ptr<ExecutionConfiguration> exec_conf,
    BoxDim dest_box)
    {
    // this test needs to be run on eight processors
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    BOOST_REQUIRE_EQUAL(size,8);

    BoxDim ref_box = BoxDim(2.0);
    // create a system with eight particles
    boost::shared_ptr<SystemDefinition> sysdef(new SystemDefinition(8,           // number of particles
                                                             dest_box, // box dimensions
                                                             1,           // number of particle types
                                                             0,           // number of bond types
                                                             0,           // number of angle types
                                                             0,           // number of dihedral types
                                                             0,           // number of dihedral types
                                                             exec_conf));



    boost::shared_ptr<ParticleData> pdata(sysdef->getParticleData());

    pdata->setPosition(0, TO_TRICLINIC(make_scalar3(-0.5,-0.75,0.25)),false);
    pdata->setPosition(1, TO_TRICLINIC(make_scalar3( 0.5,-0.75,0.25)),false);
    pdata->setPosition(2, TO_TRICLINIC(make_scalar3(-0.5, -0.25,0.25)),false);
    pdata->setPosition(3, TO_TRICLINIC(make_scalar3( 0.5, -0.25,0.25)),false);
    pdata->setPosition(4, TO_TRICLINIC(make_scalar3(-0.5,-0.75, 0.75)),false);
    pdata->setPosition(5, TO_TRICLINIC(make_scalar3( 0.5,-0.75, 0.75)),false);
    pdata->setPosition(6, TO_TRICLINIC(make_scalar3(-0.5, -0.25, 0.75)),false);
    pdata->setPosition(7, TO_TRICLINIC(make_scalar3( 0.5, -0.25, 0.75)),false);

    SnapshotParticleData<Scalar> snap(8);

    pdata->takeSnapshot(snap);

    // initialize a 2x2x2 domain decomposition on processor with rank 0
    // initialize a 2x2x2 domain decomposition on processor with rank 0
    std::vector<Scalar> fxs(1), fys(1), fzs(1);
    fxs[0] = Scalar(0.5);
    fys[0] = Scalar(0.25);
    fzs[0] = Scalar(0.75);

    boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, pdata->getBox().getL(), fxs, fys, fzs));

    boost::shared_ptr<Communicator> comm = comm_creator(sysdef, decomposition);

    pdata->setDomainDecomposition(decomposition);

    pdata->initializeFromSnapshot(snap);

    // migrate atoms
    comm->migrateParticles();

    // check that every domain has exactly one particle
    BOOST_CHECK_EQUAL(pdata->getN(), 1);

    // check that every particle stayed where it was
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(0), 0);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(1), 1);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(2), 2);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(3), 3);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(4), 4);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(5), 5);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(6), 6);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(7), 7);

    // Now move particle 0 into domain 1
    pdata->setPosition(0, TO_TRICLINIC(make_scalar3( 0.51,-0.751,0.251)),false);
    // move particle 1 into domain 2
    pdata->setPosition(1, TO_TRICLINIC(make_scalar3(-0.51, -0.251,0.251)),false);
    // move particle 2 into domain 3
    pdata->setPosition(2, TO_TRICLINIC(make_scalar3( 0.51, -0.251,0.251)),false);
    // move particle 3 into domain 4
    pdata->setPosition(3, TO_TRICLINIC(make_scalar3(-0.51,-0.751, 0.751)),false);
    // move particle 4 into domain 5
    pdata->setPosition(4, TO_TRICLINIC(make_scalar3( 0.51,-0.751, 0.751)),false);
    // move particle 5 into domain 6
    pdata->setPosition(5, TO_TRICLINIC(make_scalar3(-0.51, -0.251, 0.751)),false);
    // move particle 6 into domain 7
    pdata->setPosition(6, TO_TRICLINIC(make_scalar3( 0.51, -0.251, 0.751)),false);
    // move particle 7 into domain 0
    pdata->setPosition(7, TO_TRICLINIC(make_scalar3(-0.51,-0.751,0.251)),false);

    // validate that placing the particle would send it to the ranks that we expect
    BOOST_CHECK_EQUAL(decomposition->placeParticle(pdata->getGlobalBox(), pdata->getPosition(0)), 1);
    BOOST_CHECK_EQUAL(decomposition->placeParticle(pdata->getGlobalBox(), pdata->getPosition(1)), 2);
    BOOST_CHECK_EQUAL(decomposition->placeParticle(pdata->getGlobalBox(), pdata->getPosition(2)), 3);
    BOOST_CHECK_EQUAL(decomposition->placeParticle(pdata->getGlobalBox(), pdata->getPosition(3)), 4);
    BOOST_CHECK_EQUAL(decomposition->placeParticle(pdata->getGlobalBox(), pdata->getPosition(4)), 5);
    BOOST_CHECK_EQUAL(decomposition->placeParticle(pdata->getGlobalBox(), pdata->getPosition(5)), 6);
    BOOST_CHECK_EQUAL(decomposition->placeParticle(pdata->getGlobalBox(), pdata->getPosition(6)), 7);
    BOOST_CHECK_EQUAL(decomposition->placeParticle(pdata->getGlobalBox(), pdata->getPosition(7)), 0);
    
    // migrate atoms
    comm->migrateParticles();

    // check that every particle has ended up in the right domain
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(0), 1);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(1), 2);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(2), 3);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(3), 4);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(4), 5);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(5), 6);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(6), 7);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(7), 0);

    // check positions
    Scalar3 pos = pdata->getPosition(0);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  0.51, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.751, tol);
    BOOST_CHECK_CLOSE(pos.z, 0.251, tol);

    pos = pdata->getPosition(1);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, -0.51, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.251, tol);
    BOOST_CHECK_CLOSE(pos.z, 0.251, tol);

    pos = pdata->getPosition(2);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  0.51, tol);
    BOOST_CHECK_CLOSE(pos.y,  -0.251, tol);
    BOOST_CHECK_CLOSE(pos.z, 0.251, tol);

    pos = pdata->getPosition(3);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  -0.51, tol);
    BOOST_CHECK_CLOSE(pos.y,  -0.751, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.751, tol);

    pos = pdata->getPosition(4);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  0.51, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.751, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.751, tol);

    pos = pdata->getPosition(5);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, -0.51, tol);
    BOOST_CHECK_CLOSE(pos.y,  -0.251, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.751, tol);

    pos = pdata->getPosition(6);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  0.51, tol);
    BOOST_CHECK_CLOSE(pos.y,  -0.251, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.751, tol);

    pos = pdata->getPosition(7);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, -0.51, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.751, tol);
    BOOST_CHECK_CLOSE(pos.z, 0.251, tol);

    //
    // check that that particles are correctly wrapped across the boundary
    //

    // particle 0 crosses the global boundary in +x direction
    pdata->setPosition(0, TO_TRICLINIC(make_scalar3(1.1,-0.751,0.251)),false);
    //  particle 1 crosses the global bounadry in the -x direction
    pdata->setPosition(1, TO_TRICLINIC(make_scalar3(-1.1, -0.251, 0.251)),false);
    // particle 2 crosses the global boundary in the + y direction
    pdata->setPosition(2, TO_TRICLINIC(make_scalar3(0.51, 1.3, 0.251)),false);
    // particle 3 crosses the global boundary in the - y direction
    pdata->setPosition(3, TO_TRICLINIC(make_scalar3(-0.51, -1.5, 0.751)),false);
    // particle 4 crosses the global boundary in the + z direction
    pdata->setPosition(4, TO_TRICLINIC(make_scalar3(0.51, -0.751, 1.6)),false);
    // particle 5 crosses the global boundary in the + z direction and in the -x direction
    pdata->setPosition(5, TO_TRICLINIC(make_scalar3(-1.1, -0.251, 1.25)),false);
    // particle 6 crosses the global boundary in the + z direction and in the +x direction
    pdata->setPosition(6, TO_TRICLINIC(make_scalar3(1.3, -0.251, 1.05)),false);
    // particle 7 crosses the global boundary in the - z direction
    pdata->setPosition(7, TO_TRICLINIC(make_scalar3(-0.51, -0.751, -1.3)),false);

    // migrate particles
    comm->migrateParticles();

    // check number of particles
    switch (exec_conf->getRank())
        {
        case 0:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 1:
            BOOST_CHECK_EQUAL(pdata->getN(), 2);
            break;
        case 2:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 3:
            BOOST_CHECK_EQUAL(pdata->getN(), 2);
            break;
        case 4:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 5:
            BOOST_CHECK_EQUAL(pdata->getN(), 0);
            break;
        case 6:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 7:
            BOOST_CHECK_EQUAL(pdata->getN(), 0);
            break;
        }

    // check that every particle has ended up in the right domain
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(0), 0);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(1), 3);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(2), 1);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(3), 6);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(4), 1);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(5), 3);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(6), 2);
    BOOST_CHECK_EQUAL(pdata->getOwnerRank(7), 4);

    // check positions (taking into account that particles should have been wrapped)
    pos = pdata->getPosition(0);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  -0.9, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.751, tol);
    BOOST_CHECK_CLOSE(pos.z, 0.251, tol);

    pos = pdata->getPosition(1);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, 0.9, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.251, tol);
    BOOST_CHECK_CLOSE(pos.z, 0.251, tol);

    pos = pdata->getPosition(2);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, 0.51, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.7, tol);
    BOOST_CHECK_CLOSE(pos.z, 0.251, tol);

    pos = pdata->getPosition(3);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, -0.51, tol);
    BOOST_CHECK_CLOSE(pos.y, 0.5, tol);
    BOOST_CHECK_CLOSE(pos.z, 0.751, tol);

    pos = pdata->getPosition(4);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, 0.51, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.751, tol);
    BOOST_CHECK_CLOSE(pos.z, -0.4, tol);

    pos = pdata->getPosition(5);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x,  0.9, tol);
    BOOST_CHECK_CLOSE(pos.y,-0.251, tol);
    BOOST_CHECK_CLOSE(pos.z,-0.75, tol);

    pos = pdata->getPosition(6);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, -0.7, tol);
    BOOST_CHECK_CLOSE(pos.y,-0.251, tol);
    BOOST_CHECK_CLOSE(pos.z,-0.95, tol);

    pos = pdata->getPosition(7);
    pos = FROM_TRICLINIC(pos);
    BOOST_CHECK_CLOSE(pos.x, -0.51, tol);
    BOOST_CHECK_CLOSE(pos.y, -0.751, tol);
    BOOST_CHECK_CLOSE(pos.z,  0.7, tol);
    }


struct ghost_layer_width
    {
    ghost_layer_width(Scalar width)
        {
        w = width;
        }
    Scalar get(unsigned int type)
        {
        return w;
        }
    Scalar w;
    };

//! Test ghost particle communication
void test_communicator_ghosts(communicator_creator comm_creator,
                              boost::shared_ptr<ExecutionConfiguration> exec_conf,
                              const BoxDim& dest_box,
                              boost::shared_ptr<DomainDecomposition> decomposition,
                              Scalar3 origin)
    {
    // this test needs to be run on eight processors
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    BOOST_REQUIRE_EQUAL(size,8);

    // create a system with eight particles
    boost::shared_ptr<SystemDefinition> sysdef(new SystemDefinition(16,          // number of particles
                                                             dest_box, // box dimensions
                                                             1,           // number of particle types
                                                             0,           // number of bond types
                                                             0,           // number of angle types
                                                             0,           // number of dihedral types
                                                             0,           // number of dihedral types
                                                             exec_conf));



    boost::shared_ptr<ParticleData> pdata(sysdef->getParticleData());
    BoxDim ref_box = BoxDim(2.0);

    // Set initial atom positions
    // place one particle in the middle of every box (outside the ghost layer)
    pdata->setPosition(0, TO_TRICLINIC(make_scalar3(-0.5,-0.5,-0.5)),false);
    pdata->setPosition(1, TO_TRICLINIC(make_scalar3( 0.5,-0.5,-0.5)),false);
    pdata->setPosition(2, TO_TRICLINIC(make_scalar3(-0.5, 0.5,-0.5)),false);
    pdata->setPosition(3, TO_TRICLINIC(make_scalar3( 0.5, 0.5,-0.5)),false);
    pdata->setPosition(4, TO_TRICLINIC(make_scalar3(-0.5,-0.5, 0.5)),false);
    pdata->setPosition(5, TO_TRICLINIC(make_scalar3( 0.5,-0.5, 0.5)),false);
    pdata->setPosition(6, TO_TRICLINIC(make_scalar3(-0.5, 0.5, 0.5)),false);
    pdata->setPosition(7, TO_TRICLINIC(make_scalar3( 0.5, 0.5, 0.5)),false);

    // place particle 8 in the same box as particle 0 and in the ghost layer of its +x neighbor
    pdata->setPosition(8, TO_TRICLINIC(make_scalar3(-0.02+origin.x,-0.5,-0.5)),false);
    // place particle 9 in the same box as particle 0 and in the ghost layer of its +y neighbor
    pdata->setPosition(9, TO_TRICLINIC(make_scalar3(-0.5,-0.05+origin.y,-0.5)),false);
    // place particle 10 in the same box as particle 0 and in the ghost layer of its +y and +z neighbor
    pdata->setPosition(10, TO_TRICLINIC(make_scalar3(-0.5, -0.01+origin.y,-0.05+origin.z)),false);
    // place particle 11 in the same box as particle 0 and in the ghost layer of its +x and +y neighbor
    pdata->setPosition(11, TO_TRICLINIC(make_scalar3(-0.05+origin.x, -0.03+origin.y,-0.5)),false);
    // place particle 12 in the same box as particle 0 and in the ghost layer of its +x , +y and +z neighbor
    pdata->setPosition(12, TO_TRICLINIC(make_scalar3(-0.05+origin.x, -0.03+origin.y,-0.001+origin.z)),false);
    // place particle 13 in the same box as particle 1 and in the ghost layer of its -x neighbor
    pdata->setPosition(13, TO_TRICLINIC(make_scalar3( 0.05+origin.x, -0.5, -0.5)),false);
    // place particle 14 in the same box as particle 1 and in the ghost layer of its -x neighbor and its +y neighbor
    pdata->setPosition(14, TO_TRICLINIC(make_scalar3( 0.01+origin.x, -0.0123+origin.y, -0.5)),false);
    // place particle 15 in the same box as particle 1 and in the ghost layer of its -x neighbor, of its +y neighbor, and of its +z neighbor
    pdata->setPosition(15, TO_TRICLINIC(make_scalar3( 0.01+origin.x, -0.0123+origin.y, -0.09+origin.z)),false);

    // distribute particle data on processors
    SnapshotParticleData<Scalar> snap(16);
    pdata->takeSnapshot(snap);

    // initialize a 2x2x2 domain decomposition on processor with rank 0
//     boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf,  pdata->getBox().getL()));
    boost::shared_ptr<Communicator> comm = comm_creator(sysdef, decomposition);

    pdata->setDomainDecomposition(decomposition);

    pdata->initializeFromSnapshot(snap);

    // width of ghost layer
    ghost_layer_width g(Scalar(0.05)*ref_box.getL().x);
    comm->addGhostLayerWidthRequest(bind(&ghost_layer_width::get,g, _1));

    // Check number of particles
    switch (exec_conf->getRank())
        {
        case 0:
            BOOST_CHECK_EQUAL(pdata->getN(), 6);
            break;
        case 1:
            BOOST_CHECK_EQUAL(pdata->getN(), 4);
            break;
        case 2:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 3:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 4:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 5:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 6:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 7:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        }

    // we should have zero ghosts before the exchange
    BOOST_CHECK_EQUAL(pdata->getNGhosts(),0);

    // set ghost exchange flags for position
    CommFlags flags(0);
    flags[comm_flag::position] = 1;
    flags[comm_flag::tag] = 1;
    comm->setFlags(flags);

    // exchange ghosts
    comm->exchangeGhosts();

    Scalar3 cmp;
   // check ghost atom numbers and positions
        {
        ArrayHandle<Scalar4> h_pos(pdata->getPositions(), access_location::host, access_mode::read);
        ArrayHandle<unsigned int> h_global_rtag(pdata->getRTags(), access_location::host, access_mode::read);
        unsigned int rtag;
        switch (exec_conf->getRank())
            {
            case 0:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 3);

                rtag = h_global_rtag.data[13];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.05 + origin.x,tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.5,tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5,tol);

                rtag = h_global_rtag.data[14];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.01 + origin.x,tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.0123 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5,tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.01 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.0123 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.09 + origin.z, tol);
                break;
            case 1:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 3);

                rtag = h_global_rtag.data[8];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.02 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.5, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[11];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.05 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.03 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.05 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.03 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.001 + origin.z, tol);

                break;
            case 2:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 6);

                rtag = h_global_rtag.data[9];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.5, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.05 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[10];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.5, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.01 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.05 + origin.z, tol);

                rtag = h_global_rtag.data[11];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.05 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.03 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.05 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.03 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.001 + origin.z, tol);

                rtag = h_global_rtag.data[14];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,  0.01 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.0123 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.01 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.0123 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.09 + origin.z, tol);

                break;
            case 3:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 4);

                rtag = h_global_rtag.data[11];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.05 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.03 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.05 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.03 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.001 + origin.z, tol);

                rtag = h_global_rtag.data[14];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.01 + origin.x,tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.0123 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5,tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.01 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.0123 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.09 + origin.z, tol);

                break;
            case 4:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 3);

                rtag = h_global_rtag.data[10];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.5, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.01 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.05 + origin.z, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.05 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.03 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.001 + origin.z, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.01 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.0123 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.09 + origin.z, tol);
                break;

            case 5:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 2);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.05 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.03 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.001 + origin.z, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.01 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.0123 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.09 + origin.z, tol);
                break;

            case 6:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 3);

                rtag = h_global_rtag.data[10];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.5, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.01 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.05 + origin.z, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.05 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.03 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.001 + origin.z, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.01 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.0123 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.09 + origin.z, tol);
                break;

            case 7:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 2);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.05 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.03 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.001 + origin.z, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.01 + origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.0123 + origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.09 + origin.z, tol);
                break;
            }
        }

    // migrate atoms
    // this should reset the number of ghost particles
    comm->migrateParticles();

    BOOST_CHECK_EQUAL(pdata->getNGhosts(),0);

    //
    // check handling of periodic boundary conditions
    //

    // place some atoms in a ghost layer at a global boundary

    // place particle 8 in the same box as particle 0 and in the ghost layer of its -x neighbor and -y neighbor
    pdata->setPosition(8, TO_TRICLINIC(make_scalar3(-0.02+origin.x,-0.95,-0.5)),false);
    // place particle 9 in the same box as particle 0 and in the ghost layer of its -y neighbor
    pdata->setPosition(9, TO_TRICLINIC(make_scalar3(-0.5,-0.96,-0.5)),false);
    // place particle 10 in the same box as particle 0 and in the ghost layer of its +y neighbor and -z neighbor
    pdata->setPosition(10, TO_TRICLINIC(make_scalar3(-0.5, -0.01+origin.y,-0.97)),false);
    // place particle 11 in the same box as particle 0 and in the ghost layer of its -x and -y neighbor
    pdata->setPosition(11, TO_TRICLINIC(make_scalar3(-0.97, -0.99,-0.5)),false);
    // place particle 12 in the same box as particle 0 and in the ghost layer of its -x , -y and -z neighbor
    pdata->setPosition(12, TO_TRICLINIC(make_scalar3(-0.997, -0.998,-0.999)),false);
    // place particle 13 in the same box as particle 0 and in the ghost layer of its -x neighbor and +y neighbor
    pdata->setPosition(13, TO_TRICLINIC(make_scalar3( -0.96, -0.005+origin.y, -0.50)),false);
    // place particle 14 in the same box as particle 7 and in the ghost layer of its +x neighbor and its +y neighbor
    pdata->setPosition(14, TO_TRICLINIC(make_scalar3( 0.901, .98, 0.50)),false);
    // place particle 15 in the same box as particle 7 and in the ghost layer of its +x neighbor, of its +y neighbor, and of its +z neighbor
    pdata->setPosition(15, TO_TRICLINIC(make_scalar3( 0.99, 0.999, 0.9999)),false);

    // migrate atoms in their respective boxes
    comm->migrateParticles();

    // Check number of particles
    switch (exec_conf->getRank())
        {
        case 0:
            BOOST_CHECK_EQUAL(pdata->getN(), 7);
            break;
        case 1:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 2:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 3:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 4:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 5:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 6:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 7:
            BOOST_CHECK_EQUAL(pdata->getN(), 3);
            break;
        }

   // exchange ghosts
   comm->exchangeGhosts();

   // check ghost atom numbers and positions, taking into account that the particles should have been
   // wrapped across the boundaries
        {
        ArrayHandle<Scalar4> h_pos(pdata->getPositions(), access_location::host, access_mode::read);
        ArrayHandle<unsigned int> h_global_rtag(pdata->getRTags(), access_location::host, access_mode::read);
        unsigned int rtag;
        switch (exec_conf->getRank())
            {
            case 0:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 1);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -1.01, tol);
                BOOST_CHECK_CLOSE(cmp.y, -1.001, tol);
                BOOST_CHECK_CLOSE(cmp.z, -1.0001, tol);
                break;

            case 1:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 5);

                rtag = h_global_rtag.data[8];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.02+origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.95, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[11];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,  1.03, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.99, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 1.003, tol);
                BOOST_CHECK_CLOSE(cmp.y,-0.998, tol);
                BOOST_CHECK_CLOSE(cmp.z,-0.999, tol);

                rtag = h_global_rtag.data[13];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 1.04, tol);
                BOOST_CHECK_CLOSE(cmp.y,-0.005+origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z,-0.50, tol);


                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.99, tol);
                BOOST_CHECK_CLOSE(cmp.y,-1.001, tol);
                BOOST_CHECK_CLOSE(cmp.z,-1.0001, tol);
                break;

            case 2:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 7);
                rtag = h_global_rtag.data[8];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.02+origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y,  1.05, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[9];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.5, tol);
                BOOST_CHECK_CLOSE(cmp.y,  1.04, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[10];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.5, tol);
                BOOST_CHECK_CLOSE(cmp.y,  -0.01+origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z,-0.97, tol);

                rtag = h_global_rtag.data[11];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.97, tol);
                BOOST_CHECK_CLOSE(cmp.y,  1.01, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.997, tol);
                BOOST_CHECK_CLOSE(cmp.y,  1.002, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.999, tol);

                rtag = h_global_rtag.data[13];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.96, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.005+origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.50, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -1.01, tol);
                BOOST_CHECK_CLOSE(cmp.y, 0.999, tol);
                BOOST_CHECK_CLOSE(cmp.z,-1.0001, tol);
               break;

            case 3:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 5);

                rtag = h_global_rtag.data[8];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.02+origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y,  1.05, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[11];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,  1.03, tol);
                BOOST_CHECK_CLOSE(cmp.y,  1.01, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 1.003, tol);
                BOOST_CHECK_CLOSE(cmp.y, 1.002, tol);
                BOOST_CHECK_CLOSE(cmp.z,-0.999, tol);

                rtag = h_global_rtag.data[13];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 1.04, tol);
                BOOST_CHECK_CLOSE(cmp.y,-0.005+origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z,-0.50, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.99, tol);
                BOOST_CHECK_CLOSE(cmp.y, 0.999, tol);
                BOOST_CHECK_CLOSE(cmp.z,-1.0001, tol);
                break;

            case 4:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 4);

                rtag = h_global_rtag.data[10];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.5, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.01+origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z,  1.03, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-0.997, tol);
                BOOST_CHECK_CLOSE(cmp.y,-0.998, tol);
                BOOST_CHECK_CLOSE(cmp.z, 1.001, tol);

                rtag = h_global_rtag.data[14];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-1.099, tol);
                BOOST_CHECK_CLOSE(cmp.y, -1.02, tol);
                BOOST_CHECK_CLOSE(cmp.z,  0.50, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-1.01, tol);
                BOOST_CHECK_CLOSE(cmp.y,-1.001, tol);
                BOOST_CHECK_CLOSE(cmp.z,0.9999, tol);
                break;

            case 5:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 3);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,1.003, tol);
                BOOST_CHECK_CLOSE(cmp.y,-0.998, tol);
                BOOST_CHECK_CLOSE(cmp.z,1.001, tol);

                rtag = h_global_rtag.data[14];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,0.901, tol);
                BOOST_CHECK_CLOSE(cmp.y,-1.02, tol);
                BOOST_CHECK_CLOSE(cmp.z,0.50, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,0.99, tol);
                BOOST_CHECK_CLOSE(cmp.y,-1.001, tol);
                BOOST_CHECK_CLOSE(cmp.z,0.9999, tol);
                break;

            case 6:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 4);

                rtag = h_global_rtag.data[10];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-0.5, tol);
                BOOST_CHECK_CLOSE(cmp.y,-0.01+origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z,1.03, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-0.997, tol);
                BOOST_CHECK_CLOSE(cmp.y,1.002, tol);
                BOOST_CHECK_CLOSE(cmp.z,1.001, tol);

                rtag = h_global_rtag.data[14];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-1.099, tol);
                BOOST_CHECK_CLOSE(cmp.y,0.98, tol);
                BOOST_CHECK_CLOSE(cmp.z,0.50, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-1.01, tol);
                BOOST_CHECK_CLOSE(cmp.y,0.999, tol);
                BOOST_CHECK_CLOSE(cmp.z,0.9999, tol);
                break;

            case 7:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 1);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,1.003, tol);
                BOOST_CHECK_CLOSE(cmp.y,1.002, tol);
                BOOST_CHECK_CLOSE(cmp.z,1.001, tol);
                break;
            }
        }

   //
   // Test ghost updating
   //

   // set some new positions for the ghost particles
   // the ghost particles could have moved anywhere
   // even outside the ghost layers or boxes they were in originally
   //(but they should not move further than half the skin length),

        {
        unsigned int rtag;
        ArrayHandle<unsigned int> h_rtag(pdata->getRTags(), access_location::host, access_mode::read);
        ArrayHandle<Scalar4> h_pos(pdata->getPositions(), access_location::host, access_mode::readwrite);

        switch (exec_conf->getRank())
            {
            case 0:
                rtag = h_rtag.data[8];
                h_pos.data[rtag].x = -0.12;
                h_pos.data[rtag].y = -1.05;
                h_pos.data[rtag].z = -0.6;
                h_pos.data[rtag] = TO_POS4(TO_TRICLINIC(h_pos.data[rtag]));

                rtag = h_rtag.data[9];
                h_pos.data[rtag].x = -0.03+origin.x;
                h_pos.data[rtag].y = -1.09;
                h_pos.data[rtag].z = -0.3;
                h_pos.data[rtag] = TO_POS4(TO_TRICLINIC(h_pos.data[rtag]));

                rtag = h_rtag.data[10];
                h_pos.data[rtag].x = -0.11;
                h_pos.data[rtag].y = 0.01+origin.y;
                h_pos.data[rtag].z = -1.02;
                h_pos.data[rtag] = TO_POS4(TO_TRICLINIC(h_pos.data[rtag]));

                rtag = h_rtag.data[11];
                h_pos.data[rtag].x = -0.81;
                h_pos.data[rtag].y = -0.92;
                h_pos.data[rtag].z = -0.2;
                h_pos.data[rtag] = TO_POS4(TO_TRICLINIC(h_pos.data[rtag]));

                rtag = h_rtag.data[12];
                h_pos.data[rtag].x = -1.02;
                h_pos.data[rtag].y = -1.05;
                h_pos.data[rtag].z = -1.100;
                h_pos.data[rtag] = TO_POS4(TO_TRICLINIC(h_pos.data[rtag]));

                rtag = h_rtag.data[13];
                h_pos.data[rtag].x = -0.89;
                h_pos.data[rtag].y = 0.005+origin.y;
                h_pos.data[rtag].z = -0.99;
                h_pos.data[rtag] = TO_POS4(TO_TRICLINIC(h_pos.data[rtag]));

                break;
            case 7:
                rtag = h_rtag.data[14];
                h_pos.data[rtag].x = 1.123;
                h_pos.data[rtag].y = 1.121;
                h_pos.data[rtag].z = 0.9;
                h_pos.data[rtag] = TO_POS4(TO_TRICLINIC(h_pos.data[rtag]));

                rtag = h_rtag.data[15];
                h_pos.data[rtag].x = 0.85;
                h_pos.data[rtag].y = 1.001;
                h_pos.data[rtag].z = 1.012;
                h_pos.data[rtag] = TO_POS4(TO_TRICLINIC(h_pos.data[rtag]));

                break;
            default:
                break;
            }
        }

    // update ghosts
    comm->beginUpdateGhosts(0);
    comm->finishUpdateGhosts(0);

    // check ghost positions, taking into account that the particles should have been wrapped across the boundaries
        {
        ArrayHandle<Scalar4> h_pos(pdata->getPositions(), access_location::host, access_mode::read);
        ArrayHandle<unsigned int> h_global_rtag(pdata->getRTags(), access_location::host, access_mode::read);
        unsigned int rtag;
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        switch (rank)
            {
            case 0:
                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -1.15, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.999, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.988, tol);
                break;

            case 1:
                rtag = h_global_rtag.data[8];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.12, tol);
                BOOST_CHECK_CLOSE(cmp.y, -1.05, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.6, tol);

                rtag = h_global_rtag.data[11];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,  1.19, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.92, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.2, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.98,  tol);
                BOOST_CHECK_CLOSE(cmp.y,-1.05, tol);
                BOOST_CHECK_CLOSE(cmp.z,-1.100, tol);

                rtag = h_global_rtag.data[13];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 1.11, tol);
                BOOST_CHECK_CLOSE(cmp.y, 0.005+origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z,-0.99, tol);


                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.85, tol);
                BOOST_CHECK_CLOSE(cmp.y,-0.999, tol);
                BOOST_CHECK_CLOSE(cmp.z,-0.988, tol);
                break;

            case 2:
                rtag = h_global_rtag.data[8];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.12, tol);
                BOOST_CHECK_CLOSE(cmp.y,  0.95, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.6, tol);

                rtag = h_global_rtag.data[9];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.03+origin.x, tol);
                BOOST_CHECK_CLOSE(cmp.y,  0.91, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.3, tol);

                rtag = h_global_rtag.data[10];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.11, tol);
                BOOST_CHECK_CLOSE(cmp.y,  0.01+origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z,-1.02, tol);

                rtag = h_global_rtag.data[11];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.81, tol);
                BOOST_CHECK_CLOSE(cmp.y,  1.08, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.2, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -1.02, tol);
                BOOST_CHECK_CLOSE(cmp.y,  0.95, tol);
                BOOST_CHECK_CLOSE(cmp.z, -1.100, tol);

                rtag = h_global_rtag.data[13];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.89, tol);
                BOOST_CHECK_CLOSE(cmp.y,  0.005+origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.99, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -1.15, tol);
                BOOST_CHECK_CLOSE(cmp.y, 1.001, tol);
                BOOST_CHECK_CLOSE(cmp.z,-0.988, tol);
               break;

            case 3:
                rtag = h_global_rtag.data[8];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -.12, tol);
                BOOST_CHECK_CLOSE(cmp.y, 0.95, tol);
                BOOST_CHECK_CLOSE(cmp.z,-0.6, tol);

                rtag = h_global_rtag.data[11];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,  1.19, tol);
                BOOST_CHECK_CLOSE(cmp.y,  1.08, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.2, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.98, tol);
                BOOST_CHECK_CLOSE(cmp.y, 0.95, tol);
                BOOST_CHECK_CLOSE(cmp.z,-1.100, tol);

                rtag = h_global_rtag.data[13];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 1.11, tol);
                BOOST_CHECK_CLOSE(cmp.y, 0.005+origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z,-0.99, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.85, tol);
                BOOST_CHECK_CLOSE(cmp.y, 1.001, tol);
                BOOST_CHECK_CLOSE(cmp.z,-0.988, tol);
                break;

            case 4:
                rtag = h_global_rtag.data[10];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.11, tol);
                BOOST_CHECK_CLOSE(cmp.y,  0.01+origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z,  0.98, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-1.02, tol);
                BOOST_CHECK_CLOSE(cmp.y,-1.05, tol);
                BOOST_CHECK_CLOSE(cmp.z, 0.90, tol);

                rtag = h_global_rtag.data[14];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-0.877, tol);
                BOOST_CHECK_CLOSE(cmp.y,-0.879, tol);
                BOOST_CHECK_CLOSE(cmp.z,  0.90, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-1.15, tol);
                BOOST_CHECK_CLOSE(cmp.y,-0.999, tol);
                BOOST_CHECK_CLOSE(cmp.z, 1.012, tol);
                break;

            case 5:
                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,0.98, tol);
                BOOST_CHECK_CLOSE(cmp.y,-1.05, tol);
                BOOST_CHECK_CLOSE(cmp.z,0.900, tol);

                rtag = h_global_rtag.data[14];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,1.123, tol);
                BOOST_CHECK_CLOSE(cmp.y,-0.879, tol);
                BOOST_CHECK_CLOSE(cmp.z,0.90, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,0.85, tol);
                BOOST_CHECK_CLOSE(cmp.y,-0.999, tol);
                BOOST_CHECK_CLOSE(cmp.z,1.012, tol);
                break;

            case 6:
                rtag = h_global_rtag.data[10];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-0.11, tol);
                BOOST_CHECK_CLOSE(cmp.y, 0.01+origin.y, tol);
                BOOST_CHECK_CLOSE(cmp.z,0.98, tol);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-1.02, tol);
                BOOST_CHECK_CLOSE(cmp.y, 0.95, tol);
                BOOST_CHECK_CLOSE(cmp.z, 0.90, tol);

                rtag = h_global_rtag.data[14];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-0.877, tol);
                BOOST_CHECK_CLOSE(cmp.y,1.121, tol);
                BOOST_CHECK_CLOSE(cmp.z,0.90, tol);

                rtag = h_global_rtag.data[15];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,-1.15, tol);
                BOOST_CHECK_CLOSE(cmp.y,1.001, tol);
                BOOST_CHECK_CLOSE(cmp.z,1.012, tol);
                break;

            case 7:
                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x,0.980, tol);
                BOOST_CHECK_CLOSE(cmp.y,0.950, tol);
                BOOST_CHECK_CLOSE(cmp.z,0.900, tol);
                break;
            }
        }
   }

//! Test particle communication for covalently bonded ghosts
void test_communicator_bond_exchange(communicator_creator comm_creator,
                                     boost::shared_ptr<ExecutionConfiguration> exec_conf,
                                     const BoxDim& box,
                                     boost::shared_ptr<DomainDecomposition> decomposition)
    {
    // this test needs to be run on eight processors
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    BOOST_REQUIRE_EQUAL(size,8);

    // create a system with eight particles
    boost::shared_ptr<SystemDefinition> sysdef(new SystemDefinition(8,           // number of particles
                                                             box,         // box dimensions
                                                             1,           // number of particle types
                                                             1,           // number of bond types
                                                             0,           // number of angle types
                                                             0,           // number of dihedral types
                                                             0,           // number of dihedral types
                                                             exec_conf));



    boost::shared_ptr<ParticleData> pdata(sysdef->getParticleData());

    // Set initial atom positions
    // place one particle slightly away from the middle of every box (in direction towards
    // the center of the global box - bonds cannot extend over more than half the box length)
    pdata->setPosition(0, make_scalar3(-0.4,-0.4,-0.4),false);
    pdata->setPosition(1, make_scalar3( 0.4,-0.4,-0.4),false);
    pdata->setPosition(2, make_scalar3(-0.4, 0.4,-0.4),false);
    pdata->setPosition(3, make_scalar3( 0.4, 0.4,-0.4),false);
    pdata->setPosition(4, make_scalar3(-0.4,-0.4, 0.4),false);
    pdata->setPosition(5, make_scalar3( 0.4,-0.4, 0.4),false);
    pdata->setPosition(6, make_scalar3(-0.4, 0.4, 0.4),false);
    pdata->setPosition(7, make_scalar3( 0.4, 0.4, 0.4),false);

    // now bond these particles together, forming a cube

    boost::shared_ptr<BondData> bdata(sysdef->getBondData());

    bdata->addBondedGroup(Bond(0,0,1));  // bond 0
    bdata->addBondedGroup(Bond(0,0,2));  // bond 1
    bdata->addBondedGroup(Bond(0,0,4));  // bond 2
    bdata->addBondedGroup(Bond(0,1,3));  // bond 3
    bdata->addBondedGroup(Bond(0,1,5));  // bond 4
    bdata->addBondedGroup(Bond(0,2,3));  // bond 5
    bdata->addBondedGroup(Bond(0,2,6));  // bond 6
    bdata->addBondedGroup(Bond(0,3,7));  // bond 7
    bdata->addBondedGroup(Bond(0,4,5));  // bond 8
    bdata->addBondedGroup(Bond(0,4,6));  // bond 9
    bdata->addBondedGroup(Bond(0,5,7));  // bond 10
    bdata->addBondedGroup(Bond(0,6,7));  // bond 11

    SnapshotParticleData<Scalar> snap(8);
    pdata->takeSnapshot(snap);

    BondData::Snapshot bdata_snap(12);
    bdata->takeSnapshot(bdata_snap);

    // initialize a 2x2x2 domain decomposition on processor with rank 0
    boost::shared_ptr<Communicator> comm = comm_creator(sysdef, decomposition);

    // width of ghost layer
    ghost_layer_width g(0.1);
    comm->addGhostLayerWidthRequest(bind(&ghost_layer_width::get,g,_1));

    pdata->setDomainDecomposition(decomposition);

    // distribute particle data on processors
    pdata->initializeFromSnapshot(snap);

    // distribute bonds on processors
    bdata->initializeFromSnapshot(bdata_snap);

    // we should have one particle
    BOOST_CHECK_EQUAL(pdata->getN(), 1);

    // and zero ghost particles
    BOOST_CHECK_EQUAL(pdata->getNGhosts(),  0);

    // check global number of bonds
    BOOST_CHECK_EQUAL(bdata->getNGlobal(), 12);

    // every domain should have three bonds
    BOOST_CHECK_EQUAL(bdata->getN(), 3);

    // exchange ghost particles
    comm->migrateParticles();

    // check that nothing has changed
    BOOST_CHECK_EQUAL(pdata->getN(), 1);
    BOOST_CHECK_EQUAL(pdata->getNGhosts(),  0);
    BOOST_CHECK_EQUAL(bdata->getN(), 3);

    // now move particle 0 to box 1
    pdata->setPosition(0, make_scalar3(.3, -0.4, -0.4),false);

    // migrate particles
    comm->migrateParticles();

    switch(exec_conf->getRank())
        {
        case 0:
            // box 0 should have zero particles and 0 bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 0);
            BOOST_CHECK_EQUAL(bdata->getN(), 0);

                {
                // we should not own any bonds
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_CHECK(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[11] == GROUP_NOT_LOCAL);
                }

            break;
        case 1:
            // box 1 should have two particles and 5 bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 2);
            BOOST_CHECK_EQUAL(bdata->getN(), 5);

                {
                // we should own bonds 0-4
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_REQUIRE(h_rtag.data[0] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[1] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[2] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[3] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[4] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[11] == GROUP_NOT_LOCAL);

                ArrayHandle<BondData::members_t> h_bonds(bdata->getMembersArray(), access_location::host, access_mode::read);
                ArrayHandle<unsigned int> h_tag(bdata->getTags(), access_location::host, access_mode::read);
                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[0]],0);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[0]].tag[0],0);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[0]].tag[1],1);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[1]],1);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[1]].tag[0],0);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[1]].tag[1],2);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[2]],2);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[2]].tag[0],0);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[2]].tag[1],4);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[3]],3);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[3]].tag[0],1);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[3]].tag[1],3);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[4]],4);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[4]].tag[0],1);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[4]].tag[1],5);
                }
            break;
        case 2:
            // box 2 should have three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 1,5,6
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_REQUIRE(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[1] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[5] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[6] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[11] == GROUP_NOT_LOCAL);

                ArrayHandle<BondData::members_t> h_bonds(bdata->getMembersArray(), access_location::host, access_mode::read);
                ArrayHandle<unsigned int> h_tag(bdata->getTags(), access_location::host, access_mode::read);
                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[1]],1);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[1]].tag[0],0);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[1]].tag[1],2);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[5]],5);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[5]].tag[0],2);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[5]].tag[1],3);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[6]],6);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[6]].tag[0],2);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[6]].tag[1],6);
                }
            break;
        case 3:
            // box 3 should have three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 3,5,7
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_REQUIRE(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[3] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[5] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[7] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[11] == GROUP_NOT_LOCAL);

                ArrayHandle<BondData::members_t> h_bonds(bdata->getMembersArray(), access_location::host, access_mode::read);
                ArrayHandle<unsigned int> h_tag(bdata->getTags(), access_location::host, access_mode::read);
                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[3]],3);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[3]].tag[0],1);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[3]].tag[1],3);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[5]],5);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[5]].tag[0],2);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[5]].tag[1],3);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[7]],7);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[7]].tag[0],3);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[7]].tag[1],7);
                }
            break;
         case 4:
            // box 4 should have three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 2,8,9
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_REQUIRE(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[2] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[8] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[9] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[11] == GROUP_NOT_LOCAL);

                ArrayHandle<BondData::members_t> h_bonds(bdata->getMembersArray(), access_location::host, access_mode::read);
                ArrayHandle<unsigned int> h_tag(bdata->getTags(), access_location::host, access_mode::read);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[2]],2);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[2]].tag[0],0);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[2]].tag[1],4);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[8]],8);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[8]].tag[0],4);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[8]].tag[1],5);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[9]],9);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[9]].tag[0],4);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[9]].tag[1],6);
                }
            break;
         case 5:
            // box 5 should have three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 4,8,10
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_REQUIRE(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[4] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[8] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[10] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[11] == GROUP_NOT_LOCAL);

                ArrayHandle<BondData::members_t> h_bonds(bdata->getMembersArray(), access_location::host, access_mode::read);
                ArrayHandle<unsigned int> h_tag(bdata->getTags(), access_location::host, access_mode::read);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[4]],4);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[4]].tag[0],1);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[4]].tag[1],5);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[8]],8);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[8]].tag[0],4);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[8]].tag[1],5);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[10]],10);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[10]].tag[0],5);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[10]].tag[1],7);
                }
            break;
        case 6:
            // box 6 should have three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 6,9,11
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_REQUIRE(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[6] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[9] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[11] < bdata->getN());

                ArrayHandle<BondData::members_t> h_bonds(bdata->getMembersArray(), access_location::host, access_mode::read);
                ArrayHandle<unsigned int> h_tag(bdata->getTags(), access_location::host, access_mode::read);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[6]],6);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[6]].tag[0],2);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[6]].tag[1],6);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[9]],9);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[9]].tag[0],4);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[9]].tag[1],6);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[11]],11);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[11]].tag[0],6);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[11]].tag[1],7);
                }
            break;
        case 7:
            // box 7 should have three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 7,10,11
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_REQUIRE(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[7] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_REQUIRE(h_rtag.data[10] < bdata->getN());
                BOOST_REQUIRE(h_rtag.data[11] < bdata->getN());

                ArrayHandle<BondData::members_t> h_bonds(bdata->getMembersArray(), access_location::host, access_mode::read);
                ArrayHandle<unsigned int> h_tag(bdata->getTags(), access_location::host, access_mode::read);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[7]],7);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[7]].tag[0],3);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[7]].tag[1],7);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[10]],10);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[10]].tag[0],5);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[10]].tag[1],7);

                BOOST_CHECK_EQUAL(h_tag.data[h_rtag.data[11]],11);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[11]].tag[0],6);
                BOOST_CHECK_EQUAL(h_bonds.data[h_rtag.data[11]].tag[1],7);
                }
            break;
        }

    // move particle back
    pdata->setPosition(0, make_scalar3(-.4, -0.4, -0.4),false);

    comm->migrateParticles();

    // check that old state has been restored
    BOOST_CHECK_EQUAL(pdata->getN(), 1);
    BOOST_CHECK_EQUAL(bdata->getN(), 3);

    // swap ptl 0 and 1
    pdata->setPosition(0, make_scalar3(.4, -0.4, -0.4),false);
    pdata->setPosition(1, make_scalar3(-.4, -0.4, -0.4),false);

    comm->migrateParticles();

    switch(exec_conf->getRank())
        {
        case 0:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own three bonds
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_CHECK(h_rtag.data[0] < bdata->getN());
                BOOST_CHECK(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[3] < bdata->getN());
                BOOST_CHECK(h_rtag.data[4] < bdata->getN());
                BOOST_CHECK(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[11] == GROUP_NOT_LOCAL);
                }

            break;
        case 1:
            // box 1 should own three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 0-2
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_CHECK(h_rtag.data[0] < bdata->getN());
                BOOST_CHECK(h_rtag.data[1] < bdata->getN());
                BOOST_CHECK(h_rtag.data[2] < bdata->getN());
                BOOST_CHECK(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[11] == GROUP_NOT_LOCAL);

                }
            break;

        default:
            break;
        }

    // swap ptl 0 and 6
    pdata->setPosition(0, make_scalar3(-.4, 0.4, 0.4),false);
    pdata->setPosition(6, make_scalar3(.4, -0.4, -0.4),false);

    comm->migrateParticles();

    switch(exec_conf->getRank())
        {
        case 0:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should have three bonds
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_CHECK(h_rtag.data[0] < bdata->getN());
                BOOST_CHECK(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[3] < bdata->getN());
                BOOST_CHECK(h_rtag.data[4] < bdata->getN());
                BOOST_CHECK(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[11] == GROUP_NOT_LOCAL);
                }
            break;

        case 1:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 6,9,11
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_CHECK(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[6] < bdata->getN());
                BOOST_CHECK(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[9] < bdata->getN());
                BOOST_CHECK(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[11] < bdata->getN());
                }
            break;
        case 2:
            // box 2 should have three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 1,5,6
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_CHECK(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[1] < bdata->getN());
                BOOST_CHECK(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[5] < bdata->getN());
                BOOST_CHECK(h_rtag.data[6] < bdata->getN());
                BOOST_CHECK(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[11] == GROUP_NOT_LOCAL);
                }
            break;
        case 3:
            // box 3 should have three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 3,5,7
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_CHECK(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[3] < bdata->getN());
                BOOST_CHECK(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[5] < bdata->getN());
                BOOST_CHECK(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[7] < bdata->getN());
                BOOST_CHECK(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[11] == GROUP_NOT_LOCAL);
                }
            break;
         case 4:
            // box 4 should have three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 2,8,9
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_CHECK(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[2] < bdata->getN());
                BOOST_CHECK(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[8] < bdata->getN());
                BOOST_CHECK(h_rtag.data[9] < bdata->getN());
                BOOST_CHECK(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[11] == GROUP_NOT_LOCAL);
                }
            break;
         case 5:
            // box 5 should have three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 4,8,10
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_CHECK(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[4] < bdata->getN());
                BOOST_CHECK(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[8] < bdata->getN());
                BOOST_CHECK(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[10] < bdata->getN());
                BOOST_CHECK(h_rtag.data[11] == GROUP_NOT_LOCAL);
                }
            break;
        case 6:
            // box 6 should own three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 0-2
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_CHECK(h_rtag.data[0] < bdata->getN());
                BOOST_CHECK(h_rtag.data[1] < bdata->getN());
                BOOST_CHECK(h_rtag.data[2] < bdata->getN());
                BOOST_CHECK(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[7] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[10] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[11] == GROUP_NOT_LOCAL);

                }
            break;

        case 7:
            // box 7 should have three bonds
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            BOOST_CHECK_EQUAL(bdata->getN(), 3);

                {
                // we should own bonds 7,10,11
                ArrayHandle<unsigned int> h_rtag(bdata->getRTags(), access_location::host, access_mode::read);

                BOOST_CHECK(h_rtag.data[0] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[1] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[2] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[3] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[4] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[5] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[6] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[7] < bdata->getN());
                BOOST_CHECK(h_rtag.data[8] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[9] == GROUP_NOT_LOCAL);
                BOOST_CHECK(h_rtag.data[10] < bdata->getN());
                BOOST_CHECK(h_rtag.data[11] < bdata->getN());

                }
            break;
        }


    }

//! Test particle communication for covalently bonded ghosts
void test_communicator_bonded_ghosts(communicator_creator comm_creator,
                                     boost::shared_ptr<ExecutionConfiguration> exec_conf,
                                     const BoxDim& box,
                                     boost::shared_ptr<DomainDecomposition> decomposition)
    {
    // this test needs to be run on eight processors
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    BOOST_REQUIRE_EQUAL(size,8);

    // create a system with eight particles
    boost::shared_ptr<SystemDefinition> sysdef(new SystemDefinition(8,           // number of particles
                                                             box,         // box dimensions
                                                             1,           // number of particle types
                                                             1,           // number of bond types
                                                             0,           // number of angle types
                                                             0,           // number of dihedral types
                                                             0,           // number of dihedral types
                                                             exec_conf));



    boost::shared_ptr<ParticleData> pdata(sysdef->getParticleData());

    // Set initial atom positions
    // place one particle slightly away from the middle of every box (in direction towards
    // the center of the global box - bonds cannot extend over more than half the box length)
    pdata->setPosition(0, make_scalar3(-0.4,-0.4,-0.4),false);
    pdata->setPosition(1, make_scalar3( 0.4,-0.4,-0.4),false);
    pdata->setPosition(2, make_scalar3(-0.4, 0.4,-0.4),false);
    pdata->setPosition(3, make_scalar3( 0.4, 0.4,-0.4),false);
    pdata->setPosition(4, make_scalar3(-0.4,-0.4, 0.4),false);
    pdata->setPosition(5, make_scalar3( 0.4,-0.4, 0.4),false);
    pdata->setPosition(6, make_scalar3(-0.4, 0.4, 0.4),false);
    pdata->setPosition(7, make_scalar3( 0.4, 0.4, 0.4),false);

    // now bond these particles together, forming a cube

    boost::shared_ptr<BondData> bdata(sysdef->getBondData());

    bdata->addBondedGroup(Bond(0,0,1));  // bond type, tag a, tag b
    bdata->addBondedGroup(Bond(0,0,2));
    bdata->addBondedGroup(Bond(0,0,4));
    bdata->addBondedGroup(Bond(0,1,3));
    bdata->addBondedGroup(Bond(0,1,5));
    bdata->addBondedGroup(Bond(0,2,3));
    bdata->addBondedGroup(Bond(0,2,6));
    bdata->addBondedGroup(Bond(0,3,7));
    bdata->addBondedGroup(Bond(0,4,5));
    bdata->addBondedGroup(Bond(0,4,6));
    bdata->addBondedGroup(Bond(0,5,7));
    bdata->addBondedGroup(Bond(0,6,7));

    SnapshotParticleData<Scalar> snap(8);
    pdata->takeSnapshot(snap);

    BondData::Snapshot snap_bdata(12);
    bdata->takeSnapshot(snap_bdata);

    // initialize a 2x2x2 domain decomposition on processor with rank 0
    boost::shared_ptr<Communicator> comm = comm_creator(sysdef, decomposition);

    // communicate tags, necessary for gpu bond table
    CommFlags flags(0);
    flags[comm_flag::tag] = 1;
    comm->setFlags(flags);

    // width of ghost layer
    ghost_layer_width g(0.1);
    comm->addGhostLayerWidthRequest(bind(&ghost_layer_width::get,g,_1));

    pdata->setDomainDecomposition(decomposition);

    // distribute particle data on processors
    pdata->initializeFromSnapshot(snap);

    bdata->initializeFromSnapshot(snap_bdata);

    // we should have zero ghost particles
    BOOST_CHECK_EQUAL(pdata->getNGhosts(),  0);

    // migrate particles (to initialize bond rank table)
    comm->migrateParticles();

    // exchange ghost particles
    comm->exchangeGhosts();

        {
        // all bonds should be complete, every processor should have three bonds
        ArrayHandle<BondData::members_t> h_gpu_bondlist(bdata->getGPUTable(), access_location::host, access_mode::read);
        ArrayHandle<unsigned int> h_n_bonds(bdata->getNGroupsArray(), access_location::host, access_mode::read);
        ArrayHandle<unsigned int> h_tag(pdata->getTags(), access_location::host, access_mode::read);

        BOOST_CHECK_EQUAL(h_n_bonds.data[0],3);
        unsigned int pitch = bdata->getGPUTableIndexer().getW();

        unsigned int sorted_tags[3];
        sorted_tags[0] = h_tag.data[h_gpu_bondlist.data[0].idx[0]];
        sorted_tags[1] = h_tag.data[h_gpu_bondlist.data[pitch].idx[0]];
        sorted_tags[2] = h_tag.data[h_gpu_bondlist.data[2*pitch].idx[0]];

        std::sort(sorted_tags, sorted_tags + 3);

        // check bond partners
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        switch (rank)
            {
            case 0:
                BOOST_CHECK_EQUAL(sorted_tags[0], 1);
                BOOST_CHECK_EQUAL(sorted_tags[1], 2);
                BOOST_CHECK_EQUAL(sorted_tags[2], 4);
                break;
            case 1:
                BOOST_CHECK_EQUAL(sorted_tags[0], 0);
                BOOST_CHECK_EQUAL(sorted_tags[1], 3);
                BOOST_CHECK_EQUAL(sorted_tags[2], 5);
                break;
            case 2:
                BOOST_CHECK_EQUAL(sorted_tags[0], 0);
                BOOST_CHECK_EQUAL(sorted_tags[1], 3);
                BOOST_CHECK_EQUAL(sorted_tags[2], 6);
                break;
            case 3:
                BOOST_CHECK_EQUAL(sorted_tags[0], 1);
                BOOST_CHECK_EQUAL(sorted_tags[1], 2);
                BOOST_CHECK_EQUAL(sorted_tags[2], 7);
                break;
            case 4:
                BOOST_CHECK_EQUAL(sorted_tags[0], 0);
                BOOST_CHECK_EQUAL(sorted_tags[1], 5);
                BOOST_CHECK_EQUAL(sorted_tags[2], 6);
                break;
            case 5:
                BOOST_CHECK_EQUAL(sorted_tags[0], 1);
                BOOST_CHECK_EQUAL(sorted_tags[1], 4);
                BOOST_CHECK_EQUAL(sorted_tags[2], 7);
                break;
            case 6:
                BOOST_CHECK_EQUAL(sorted_tags[0], 2);
                BOOST_CHECK_EQUAL(sorted_tags[1], 4);
                BOOST_CHECK_EQUAL(sorted_tags[2], 7);
                break;
            case 7:
                BOOST_CHECK_EQUAL(sorted_tags[0], 3);
                BOOST_CHECK_EQUAL(sorted_tags[1], 5);
                BOOST_CHECK_EQUAL(sorted_tags[2], 6);
                break;
            }
        }
    }

bool migrate_request(unsigned int timestep)
    {
    return true;
    }

CommFlags comm_flag_request(unsigned int timestep)
    {
    CommFlags flags(0);
    flags[comm_flag::position] = 1;
    flags[comm_flag::tag] = 1;
    return flags;
    }

void test_communicator_compare(communicator_creator comm_creator_1,
                                 communicator_creator comm_creator_2,
                                 boost::shared_ptr<ExecutionConfiguration> exec_conf_1,
                                 boost::shared_ptr<ExecutionConfiguration> exec_conf_2,
                                 const BoxDim& box,
                                 boost::shared_ptr<DomainDecomposition> decomposition_1,
                                 boost::shared_ptr<DomainDecomposition> decomposition_2)

    {
    if (exec_conf_1->getRank() == 0)
        std::cout << "Begin random ghost comparison test" << std::endl;

    unsigned int n = 1000;
    // create a system with eight particles
    boost::shared_ptr<SystemDefinition> sysdef_1(new SystemDefinition(n,           // number of particles
                                                             box,         // box dimensions
                                                             1,           // number of particle types
                                                             1,           // number of bond types
                                                             0,           // number of angle types
                                                             0,           // number of dihedral types
                                                             0,           // number of dihedral types
                                                             exec_conf_1));
    boost::shared_ptr<SystemDefinition> sysdef_2(new SystemDefinition(n,           // number of particles
                                                             box,         // box dimensions
                                                             1,           // number of particle types
                                                             1,           // number of bond types
                                                             0,           // number of angle types
                                                             0,           // number of dihedral types
                                                             0,           // number of dihedral types
                                                             exec_conf_2));

    boost::shared_ptr<ParticleData> pdata_1 = sysdef_1->getParticleData();
    boost::shared_ptr<ParticleData> pdata_2 = sysdef_2->getParticleData();

    Scalar3 lo = pdata_1->getBox().getLo();
    Scalar3 L = pdata_1->getBox().getL();

    SnapshotParticleData<Scalar> snap(n);
    snap.type_mapping.push_back("A");

    srand(12345);
    for (unsigned int i = 0; i < n; ++i)
        {
        snap.pos[i] = vec3<Scalar>(lo.x + (Scalar)rand()/(Scalar)RAND_MAX*L.x,
                                   lo.y + (Scalar)rand()/(Scalar)RAND_MAX*L.y,
                                   lo.z + (Scalar)rand()/(Scalar)RAND_MAX*L.z);
        }

    // setup communicators
    boost::shared_ptr<Communicator> comm_1 = comm_creator_1(sysdef_1, decomposition_1);
    boost::shared_ptr<Communicator> comm_2 = comm_creator_2(sysdef_2, decomposition_2);

    // width of ghost layer
    ghost_layer_width g(0.2);
    comm_1->addGhostLayerWidthRequest(bind(&ghost_layer_width::get,g,_1));
    comm_2->addGhostLayerWidthRequest(bind(&ghost_layer_width::get,g,_1));

    pdata_1->setDomainDecomposition(decomposition_1);
    pdata_2->setDomainDecomposition(decomposition_2);

    // distribute particle data on processors
    pdata_1->initializeFromSnapshot(snap);
    pdata_2->initializeFromSnapshot(snap);

    // Create ConstForceComputes
//    boost::shared_ptr<ConstForceCompute> fc_1(new ConstForceCompute(sysdef_1, Scalar(-0.3), Scalar(0.2), Scalar(-0.123)));
//    boost::shared_ptr<ConstForceCompute> fc_2(new ConstForceCompute(sysdef_2, Scalar(-0.3), Scalar(0.2), Scalar(-0.123)));

    boost::shared_ptr<ParticleSelector> selector_all_1(new ParticleSelectorTag(sysdef_1, 0, pdata_1->getNGlobal()-1));
    boost::shared_ptr<ParticleGroup> group_all_1(new ParticleGroup(sysdef_1, selector_all_1));

    boost::shared_ptr<ParticleSelector> selector_all_2(new ParticleSelectorTag(sysdef_2, 0, pdata_2->getNGlobal()-1));
    boost::shared_ptr<ParticleGroup> group_all_2(new ParticleGroup(sysdef_2, selector_all_2));

    boost::shared_ptr<TwoStepNVE> two_step_nve_1(new TwoStepNVE(sysdef_1, group_all_1));
    boost::shared_ptr<TwoStepNVE> two_step_nve_2(new TwoStepNVE(sysdef_2, group_all_2));

    Scalar deltaT=0.001;
    boost::shared_ptr<IntegratorTwoStep> nve_up_1(new IntegratorTwoStep(sysdef_1, deltaT));
    boost::shared_ptr<IntegratorTwoStep> nve_up_2(new IntegratorTwoStep(sysdef_2, deltaT));
    nve_up_1->addIntegrationMethod(two_step_nve_1);
    nve_up_2->addIntegrationMethod(two_step_nve_2);

//    nve_up_1->addForceCompute(fc_1);
//    nve_up_2->addForceCompute(fc_2);

    // set constant velocities
    for (unsigned int tag= 0; tag < n; ++tag)
        {
        pdata_1->setVelocity(tag, make_scalar3(0.01,0.02,0.03));
        pdata_2->setVelocity(tag, make_scalar3(0.01,0.02,0.03));
        }

    comm_1->addMigrateRequest(bind(&migrate_request,_1));
    comm_2->addMigrateRequest(bind(&migrate_request,_1));

    comm_1->addCommFlagsRequest(bind(&comm_flag_request, _1));
    comm_2->addCommFlagsRequest(bind(&comm_flag_request, _1));

    nve_up_1->setCommunicator(comm_1);
    nve_up_2->setCommunicator(comm_2);

    nve_up_1->prepRun(0);
    nve_up_2->prepRun(0);
    exec_conf_1->msg->notice(1) << "Running 1000 steps..." << std::endl;
    bool err = false;
    for (unsigned int step = 0; step < 1000; ++step)
        {
        if (! (step % 100)) exec_conf_1->msg->notice(1) << "Step " << step << std::endl;

        // both communicators should replicate the same number of ghosts
        BOOST_CHECK_EQUAL(pdata_1->getNGhosts(), pdata_2->getNGhosts());

            {
            ArrayHandle<unsigned int> h_rtag_1(pdata_1->getRTags(), access_location::host, access_mode::read);
            ArrayHandle<Scalar4> h_pos_1(pdata_1->getPositions(), access_location::host, access_mode::read);
            ArrayHandle<unsigned int> h_rtag_2(pdata_2->getRTags(), access_location::host, access_mode::read);
            ArrayHandle<Scalar4> h_pos_2(pdata_2->getPositions(), access_location::host, access_mode::read);
            for (unsigned int i = 0; i < n; ++i)
                {
                bool has_ghost_1 = false, has_ghost_2 = false;

                if (h_rtag_1.data[i] >= pdata_1->getN() && (h_rtag_1.data[i] < (pdata_1->getN() + pdata_1->getNGhosts())))
                    has_ghost_1 = true;

                if (h_rtag_2.data[i] >= pdata_2->getN() && (h_rtag_2.data[i] < (pdata_2->getN() + pdata_2->getNGhosts())))
                    has_ghost_2 = true;

                //  particle is either in both systems' ghost layers or in none
                BOOST_REQUIRE((has_ghost_1 && has_ghost_2) || (!has_ghost_1 && !has_ghost_2));

                if (has_ghost_1 && has_ghost_2)
                    {
                    Scalar tol_rough = .1;
                    BOOST_CHECK_CLOSE(h_pos_1.data[h_rtag_1.data[i]].x, h_pos_2.data[h_rtag_2.data[i]].x,tol_rough);
                    BOOST_CHECK_CLOSE(h_pos_1.data[h_rtag_1.data[i]].y, h_pos_2.data[h_rtag_2.data[i]].y,tol_rough);
                    BOOST_CHECK_CLOSE(h_pos_1.data[h_rtag_1.data[i]].z, h_pos_2.data[h_rtag_2.data[i]].z,tol_rough);
                    }
                }
            }
       // error out on first time step where test fails
       BOOST_REQUIRE(!err);

       nve_up_1->update(step);
       nve_up_2->update(step);
       }

    if (exec_conf_1->getRank() == 0)
        std::cout << "Finish random ghosts test" << std::endl;
    }

//! Test ghost particle communication
void test_communicator_ghost_fields(communicator_creator comm_creator, boost::shared_ptr<ExecutionConfiguration> exec_conf)
    {
    // this test needs to be run on eight processors
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    BOOST_REQUIRE_EQUAL(size,8);

    // create a system with eight + 1 one ptls (1 ptl in ghost layer)
    boost::shared_ptr<SystemDefinition> sysdef(new SystemDefinition(9,          // number of particles
                                                             BoxDim(2.0), // box dimensions
                                                             1,           // number of particle types
                                                             0,           // number of bond types
                                                             0,           // number of angle types
                                                             0,           // number of dihedral types
                                                             0,           // number of dihedral types
                                                             exec_conf));


    boost::shared_ptr<ParticleData> pdata(sysdef->getParticleData());

    // Set initial atom positions
    // place one particle in the middle of every box (outside the ghost layer)
    pdata->setPosition(0, make_scalar3(-0.5,-0.5,-0.5),false);
    pdata->setPosition(1, make_scalar3( 0.5,-0.5,-0.5),false);
    pdata->setPosition(2, make_scalar3(-0.5, 0.5,-0.5),false);
    pdata->setPosition(3, make_scalar3( 0.5, 0.5,-0.5),false);
    pdata->setPosition(4, make_scalar3(-0.5,-0.5, 0.5),false);
    pdata->setPosition(5, make_scalar3( 0.5,-0.5, 0.5),false);
    pdata->setPosition(6, make_scalar3(-0.5, 0.5, 0.5),false);
    pdata->setPosition(7, make_scalar3( 0.5, 0.5, 0.5),false);

    // particle 8 in the ghost layer of its +x neighbor
    pdata->setPosition(8, make_scalar3( -0.05, -0.5, -0.5),false);

    // set other properties of ptl 8
    pdata->setVelocity(8, make_scalar3(1.0,2.0,3.0));
    pdata->setMass(8, 4.0);
    pdata->setCharge(8, 5.0);
    pdata->setDiameter(8, 6.0);
    pdata->setOrientation(8,make_scalar4(97.0,98.0,99.0,100.0));

    // distribute particle data on processors
    SnapshotParticleData<Scalar> snap(9);
    pdata->takeSnapshot(snap);

    // initialize a 2x2x2 domain decomposition on processor with rank 0
    boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf,  pdata->getBox().getL()));
    boost::shared_ptr<Communicator> comm = comm_creator(sysdef, decomposition);

    pdata->setDomainDecomposition(decomposition);

    pdata->initializeFromSnapshot(snap);

    // width of ghost layer
    ghost_layer_width g(0.1);
    comm->addGhostLayerWidthRequest(bind(&ghost_layer_width::get,g,_1));

    // Check number of particles
    switch (exec_conf->getRank())
        {
        case 0:
            BOOST_CHECK_EQUAL(pdata->getN(), 2);
            break;
        case 1:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 2:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 3:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 4:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 5:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 6:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 7:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        }

    // we should have zero ghosts before the exchange
    BOOST_CHECK_EQUAL(pdata->getNGhosts(),0);

    // set ghost exchange flags for position
    CommFlags flags(0);
    flags[comm_flag::position] = 1;
    flags[comm_flag::velocity] = 1;
    flags[comm_flag::orientation] = 1;
    flags[comm_flag::charge] = 1;
    flags[comm_flag::diameter] = 1;
    flags[comm_flag::tag] = 1;
    comm->setFlags(flags);

    // reset numbers of ghosts
    comm->migrateParticles();

    // exchange ghosts
    comm->exchangeGhosts();

        {
        // check ghost atom numbers, positions, velocities, etc.
        ArrayHandle<Scalar4> h_pos(pdata->getPositions(), access_location::host, access_mode::read);
        ArrayHandle<Scalar4> h_vel(pdata->getVelocities(), access_location::host, access_mode::read);
        ArrayHandle<Scalar> h_charge(pdata->getCharges(), access_location::host, access_mode::read);
        ArrayHandle<Scalar> h_diameter(pdata->getDiameters(), access_location::host, access_mode::read);
        ArrayHandle<Scalar4> h_orientation(pdata->getOrientationArray(), access_location::host, access_mode::read);
        ArrayHandle<unsigned int> h_global_rtag(pdata->getRTags(), access_location::host, access_mode::read);

        unsigned int rtag;
        switch (exec_conf->getRank())
            {
            case 0:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 0);
                break;

            case 1:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 1);

                rtag = h_global_rtag.data[8];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                BOOST_CHECK_CLOSE(h_pos.data[rtag].x, -0.05,tol);
                BOOST_CHECK_CLOSE(h_pos.data[rtag].y, -0.5,tol);
                BOOST_CHECK_CLOSE(h_pos.data[rtag].z, -0.5,tol);

                BOOST_CHECK_CLOSE(h_vel.data[rtag].x, 1.0,tol);
                BOOST_CHECK_CLOSE(h_vel.data[rtag].y, 2.0,tol);
                BOOST_CHECK_CLOSE(h_vel.data[rtag].z, 3.0,tol);
                BOOST_CHECK_CLOSE(h_vel.data[rtag].w, 4.0,tol); // mass

                BOOST_CHECK_CLOSE(h_charge.data[rtag], 5.0,tol);
                BOOST_CHECK_CLOSE(h_diameter.data[rtag], 6.0,tol);

                BOOST_CHECK_CLOSE(h_orientation.data[rtag].x, 97.0,tol);
                BOOST_CHECK_CLOSE(h_orientation.data[rtag].y, 98.0,tol);
                BOOST_CHECK_CLOSE(h_orientation.data[rtag].z, 99.0,tol);
                break;

            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 0);
                break;
            }
        }

   // set some new fields for the ghost particles
   pdata->setPosition(8, make_scalar3(-0.13,-0.5,-0.5),false);
   pdata->setVelocity(8, make_scalar3(-3.0,-2.0,-1.0));
   pdata->setMass(8, 0.1);
   pdata->setOrientation(8,make_scalar4(22.0,23.0,24.0,25.0));


   // update ghosts
    comm->beginUpdateGhosts(0);
    comm->finishUpdateGhosts(0);

        {
        // check ghost atom numbers, positions, velocities, etc.
        ArrayHandle<Scalar4> h_pos(pdata->getPositions(), access_location::host, access_mode::read);
        ArrayHandle<Scalar4> h_vel(pdata->getVelocities(), access_location::host, access_mode::read);
        ArrayHandle<Scalar> h_charge(pdata->getCharges(), access_location::host, access_mode::read);
        ArrayHandle<Scalar> h_diameter(pdata->getDiameters(), access_location::host, access_mode::read);
        ArrayHandle<Scalar4> h_orientation(pdata->getOrientationArray(), access_location::host, access_mode::read);
        ArrayHandle<unsigned int> h_global_rtag(pdata->getRTags(), access_location::host, access_mode::read);

        unsigned int rtag;
        switch (exec_conf->getRank())
            {
            case 1:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 1);

                rtag = h_global_rtag.data[8];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                BOOST_CHECK_CLOSE(h_pos.data[rtag].x, -0.13,tol);
                BOOST_CHECK_CLOSE(h_pos.data[rtag].y, -0.5,tol);
                BOOST_CHECK_CLOSE(h_pos.data[rtag].z, -0.5,tol);

                BOOST_CHECK_CLOSE(h_vel.data[rtag].x, -3.0,tol);
                BOOST_CHECK_CLOSE(h_vel.data[rtag].y, -2.0,tol);
                BOOST_CHECK_CLOSE(h_vel.data[rtag].z, -1.0,tol);
                BOOST_CHECK_CLOSE(h_vel.data[rtag].w, 0.1,tol); // mass

                // charge and diameter should be unchanged
                BOOST_CHECK_CLOSE(h_charge.data[rtag], 5.0,tol);
                BOOST_CHECK_CLOSE(h_diameter.data[rtag], 6.0,tol);

                BOOST_CHECK_CLOSE(h_orientation.data[rtag].x, 22.0,tol);
                BOOST_CHECK_CLOSE(h_orientation.data[rtag].y, 23.0,tol);
                BOOST_CHECK_CLOSE(h_orientation.data[rtag].z, 24.0,tol);
                break;

            case 0:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                BOOST_CHECK_EQUAL(pdata->getNGhosts(), 0);
                break;
            }
        }
    }

Scalar ghost_layer_width_request_1(unsigned int type)
    {
    return 0.0123;
    }

Scalar ghost_layer_width_request_2(unsigned int type)
    {
    return 0.0001;
    }

Scalar ghost_layer_width_request_3(unsigned int type)
    {
    return 0.1;
    }

//! Ghost layer subscriber for two particle types
struct two_type_ghost_layer
    {
    //! Constructor
    /*!
     * \param r_A First cutoff
     * \param r_B Second cutoff
     */
    two_type_ghost_layer(Scalar r_A, Scalar r_B) : m_r_A(r_A), m_r_B(r_B) {}

    //! Get the ghost width layer by type
    /*!
     * \param type Type index
     * \returns second cutoff radius if type is non-zero, first cutoff radius otherwise
     */
    Scalar get(unsigned int type)
        {
        return (type) ? m_r_B : m_r_A;
        }
    Scalar m_r_A;   //!< First cutoff
    Scalar m_r_B;   //<! Second cutoff
    };

//! Test setting the ghost layer width
void test_communicator_ghost_layer_width(communicator_creator comm_creator, boost::shared_ptr<ExecutionConfiguration> exec_conf)
    {
    // this test needs to be run on eight processors
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    BOOST_REQUIRE_EQUAL(size,8);

    // just create some system
    boost::shared_ptr<SystemDefinition> sysdef(new SystemDefinition(8,          // number of particles
                                                             BoxDim(2.0), // box dimensions
                                                             2,           // number of particle types
                                                             0,           // number of bond types
                                                             0,           // number of angle types
                                                             0,           // number of dihedral types
                                                             0,           // number of dihedral types
                                                             exec_conf));


    boost::shared_ptr<ParticleData> pdata(sysdef->getParticleData());

    // Set initial atom positions
    // place one particle in the middle of every box (outside the ghost layer)
    pdata->setPosition(0, make_scalar3(-0.5,-0.5,-0.5),false);
    pdata->setPosition(1, make_scalar3( 0.5,-0.5,-0.5),false);
    pdata->setPosition(2, make_scalar3(-0.5, 0.5,-0.5),false);
    pdata->setPosition(3, make_scalar3( 0.5, 0.5,-0.5),false);
    pdata->setPosition(4, make_scalar3(-0.5,-0.5, 0.5),false);
    pdata->setPosition(5, make_scalar3( 0.5,-0.5, 0.5),false);
    pdata->setPosition(6, make_scalar3(-0.5, 0.5, 0.5),false);
    pdata->setPosition(7, make_scalar3( 0.5, 0.5, 0.5),false);
    for (unsigned int i=0; i < pdata->getN(); ++i)
        {
        pdata->setType(i, i%2);
        }

    // distribute particle data on processors
    SnapshotParticleData<Scalar> snap(9);
    pdata->takeSnapshot(snap);

    // initialize a 2x2x2 domain decomposition on processor with rank 0
    boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf,  pdata->getBox().getL()));
    boost::shared_ptr<Communicator> comm = comm_creator(sysdef, decomposition);

    pdata->setDomainDecomposition(decomposition);

    pdata->initializeFromSnapshot(snap);

    // set ghost exchange flags for position
    CommFlags flags(0);
    flags[comm_flag::position] = 1;
    comm->setFlags(flags);

    // reset numbers of ghosts
    comm->migrateParticles();

    // exchange ghosts
    comm->exchangeGhosts();

    BOOST_CHECK_SMALL(comm->getGhostLayerMaxWidth(), tol_small);

    // width of ghost layer
    comm->addGhostLayerWidthRequest(bind(&ghost_layer_width_request_1,_1));
    pdata->removeAllGhostParticles();
    comm->exchangeGhosts();
    BOOST_CHECK_CLOSE(comm->getGhostLayerMaxWidth(), 0.0123, tol);

    comm->addGhostLayerWidthRequest(bind(&ghost_layer_width_request_2,_1));
    pdata->removeAllGhostParticles();
    comm->exchangeGhosts();
    BOOST_CHECK_CLOSE(comm->getGhostLayerMaxWidth(), 0.0123, tol);

    comm->addGhostLayerWidthRequest(bind(&ghost_layer_width_request_3,_1));
    pdata->removeAllGhostParticles();
    comm->exchangeGhosts();
    BOOST_CHECK_CLOSE(comm->getGhostLayerMaxWidth(), 0.1, tol);
    
    // check that when using two types, only one gets updated
    two_type_ghost_layer g(Scalar(0.05), Scalar(0.2));
    comm->addGhostLayerWidthRequest(bind(&two_type_ghost_layer::get,g,_1));
    pdata->removeAllGhostParticles();
    comm->exchangeGhosts();
        {
        ArrayHandle<Scalar> h_r_ghost(comm->getGhostLayerWidth(), access_location::host, access_mode::read);
        BOOST_CHECK_CLOSE(h_r_ghost.data[0], 0.1, tol);
        BOOST_CHECK_CLOSE(h_r_ghost.data[1], 0.2, tol);
        }
    
    // now update the other type
    two_type_ghost_layer g2(Scalar(0.3), Scalar(0.2));
    comm->addGhostLayerWidthRequest(bind(&two_type_ghost_layer::get,g2,_1));
    pdata->removeAllGhostParticles();
    comm->exchangeGhosts();
        {
        ArrayHandle<Scalar> h_r_ghost(comm->getGhostLayerWidth(), access_location::host, access_mode::read);
        BOOST_CHECK_CLOSE(h_r_ghost.data[0], 0.3, tol);
        BOOST_CHECK_CLOSE(h_r_ghost.data[1], 0.2, tol);
        }
    }

//! Test per-type ghost layer
void test_communicator_ghosts_per_type(communicator_creator comm_creator, boost::shared_ptr<ExecutionConfiguration> exec_conf, const BoxDim& dest_box)
    {
    // this test needs to be run on eight processors
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    BOOST_REQUIRE_EQUAL(size,8);

    // create a system with fourteen particles
    boost::shared_ptr<SystemDefinition> sysdef(new SystemDefinition(14,          // number of particles
                                                             dest_box, // box dimensions
                                                             2,           // number of particle types
                                                             0,           // number of bond types
                                                             0,           // number of angle types
                                                             0,           // number of dihedral types
                                                             0,           // number of dihedral types
                                                             exec_conf));



   boost::shared_ptr<ParticleData> pdata(sysdef->getParticleData());
   BoxDim ref_box = BoxDim(2.0);

    // Set initial atom positions
    // place one particle in the middle of every box (outside the ghost layer)
    pdata->setPosition(0, TO_TRICLINIC(make_scalar3(-0.5,-0.5,-0.5)),false);
    pdata->setPosition(1, TO_TRICLINIC(make_scalar3( 0.5,-0.5,-0.5)),false);
    pdata->setPosition(2, TO_TRICLINIC(make_scalar3(-0.5, 0.5,-0.5)),false);
    pdata->setPosition(3, TO_TRICLINIC(make_scalar3( 0.5, 0.5,-0.5)),false);
    pdata->setPosition(4, TO_TRICLINIC(make_scalar3(-0.5,-0.5, 0.5)),false);
    pdata->setPosition(5, TO_TRICLINIC(make_scalar3( 0.5,-0.5, 0.5)),false);
    pdata->setPosition(6, TO_TRICLINIC(make_scalar3(-0.5, 0.5, 0.5)),false);
    pdata->setPosition(7, TO_TRICLINIC(make_scalar3( 0.5, 0.5, 0.5)),false);
    // toggle the types back and forth
    for (unsigned int i=0; i < 8; ++i)
        {
        pdata->setType(i, i%2);
        }

    // 8: A, same rank as 0, within +x
    pdata->setPosition(8, TO_TRICLINIC(make_scalar3(-0.02,-0.5,-0.5)),false);
    pdata->setType(8, 0);

    // 9: B, same rank as 0, within +x
    pdata->setPosition(9, TO_TRICLINIC(make_scalar3(-0.03,-0.5,-0.5)),false);
    pdata->setType(9, 1);

    // 10: A, same rank as 1, outside +y
    pdata->setPosition(10, TO_TRICLINIC(make_scalar3(0.5, -0.12,-0.5)),false);
    pdata->setType(10, 0);

    // 11: B, same rank as 1, inside +y
    pdata->setPosition(11, TO_TRICLINIC(make_scalar3(0.5, -0.12,-0.5)),false);
    pdata->setType(11, 1);

    // 12: A, same rank as 4, inside -z
    pdata->setPosition(12, TO_TRICLINIC(make_scalar3(-0.5, -0.5, 0.05)),false);
    pdata->setType(12, 0);
    
    // 13: B, same rank as 4, outside -z
    pdata->setPosition(13, TO_TRICLINIC(make_scalar3(-0.5, -0.5, 0.25)),false);
    pdata->setType(13, 1);

    // distribute particle data on processors
    SnapshotParticleData<Scalar> snap(14);
    pdata->takeSnapshot(snap);

    // initialize a 2x2x2 domain decomposition on processor with rank 0
    boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf,  pdata->getBox().getL()));
    boost::shared_ptr<Communicator> comm = comm_creator(sysdef, decomposition);
    
    pdata->setDomainDecomposition(decomposition);

    pdata->initializeFromSnapshot(snap);

    // width of ghost layer
    two_type_ghost_layer g(Scalar(0.1), Scalar(0.2));
    comm->addGhostLayerWidthRequest(bind(&two_type_ghost_layer::get,g,_1));

    // Check number of particles
    switch (exec_conf->getRank())
        {
        case 0:
            BOOST_CHECK_EQUAL(pdata->getN(), 3);
            break;
        case 1:
            BOOST_CHECK_EQUAL(pdata->getN(), 3);
            break;
        case 2:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 3:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 4:
            BOOST_CHECK_EQUAL(pdata->getN(), 3);
            break;
        case 5:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 6:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        case 7:
            BOOST_CHECK_EQUAL(pdata->getN(), 1);
            break;
        }

    // we should have zero ghosts before the exchange
    BOOST_CHECK_EQUAL(pdata->getNGhosts(),0);

    // set ghost exchange flags for position
    CommFlags flags(0);
    flags[comm_flag::position] = 1;
    flags[comm_flag::tag] = 1;
    comm->setFlags(flags);

    // exchange ghosts
    comm->exchangeGhosts();

    Scalar3 cmp;
   // check ghost atom numbers and positions
        {
        ArrayHandle<Scalar4> h_pos(pdata->getPositions(), access_location::host, access_mode::read);
        ArrayHandle<unsigned int> h_global_rtag(pdata->getRTags(), access_location::host, access_mode::read);
        unsigned int rtag;
        switch (exec_conf->getRank())
            {
            case 0:
                BOOST_REQUIRE_EQUAL(pdata->getNGhosts(), 1);

                rtag = h_global_rtag.data[12];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.5,tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.5,tol);
                BOOST_CHECK_CLOSE(cmp.z, 0.05,tol);

                break;
            case 1:
                BOOST_REQUIRE_EQUAL(pdata->getNGhosts(), 2);

                rtag = h_global_rtag.data[8];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.02, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.5, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);

                rtag = h_global_rtag.data[9];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, -0.03, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.5, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);
                break;
            case 2:
                BOOST_REQUIRE_EQUAL(pdata->getNGhosts(), 0);
                break;
            case 3:
                BOOST_REQUIRE_EQUAL(pdata->getNGhosts(), 1);

                rtag = h_global_rtag.data[11];
                BOOST_CHECK(rtag >= pdata->getN() && rtag < pdata->getN()+pdata->getNGhosts());
                cmp = FROM_TRICLINIC(h_pos.data[rtag]);
                BOOST_CHECK_CLOSE(cmp.x, 0.5, tol);
                BOOST_CHECK_CLOSE(cmp.y, -0.12, tol);
                BOOST_CHECK_CLOSE(cmp.z, -0.5, tol);
                break;
            case 4:
                BOOST_REQUIRE_EQUAL(pdata->getNGhosts(), 0);
                break;
            case 5:
                BOOST_REQUIRE_EQUAL(pdata->getNGhosts(), 0);
                break;
            case 6:
                BOOST_REQUIRE_EQUAL(pdata->getNGhosts(), 0);
                break;
            case 7:
                BOOST_REQUIRE_EQUAL(pdata->getNGhosts(), 0);
                break;
            }
        }
    }

//! Communicator creator for unit tests
boost::shared_ptr<Communicator> base_class_communicator_creator(boost::shared_ptr<SystemDefinition> sysdef,
                                                         boost::shared_ptr<DomainDecomposition> decomposition)
    {
    return boost::shared_ptr<Communicator>(new Communicator(sysdef, decomposition) );
    }

#ifdef ENABLE_CUDA
boost::shared_ptr<Communicator> gpu_communicator_creator(boost::shared_ptr<SystemDefinition> sysdef,
                                                  boost::shared_ptr<DomainDecomposition> decomposition)
    {
    return boost::shared_ptr<Communicator>(new CommunicatorGPU(sysdef, decomposition) );
    }
#endif

//! Tests particle distribution
BOOST_AUTO_TEST_CASE( DomainDecomposition_test )
    {
    boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::CPU));
    BoxDim box(2.0);
    boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, box.getL()));
    test_domain_decomposition(exec_conf, box, decomposition);
    }

//! Tests balanced particle distribution on CPU
BOOST_AUTO_TEST_CASE( BalancedDomainDecomposition_test )
    {
    boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::CPU));
    BoxDim box(2.0);
        
    // first test the fallback to the uniform grid using the standard DomainDecomposition test
    std::vector<Scalar> fxs(2), fys(2), fzs(3);
    fxs[0] = Scalar(0.5); fxs[1] = Scalar(0.5);
    fys[0] = Scalar(0.25); fys[1] = Scalar(0.75);
    fzs[0] = Scalar(0.4); fzs[1] = Scalar(0.2); fzs[2] = Scalar(0.4);
    boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, box.getL(), fxs, fys, fzs));
    test_domain_decomposition(exec_conf, box, decomposition);
    
    // then test the balanced decomposition in the test for nonuniform particles and decomposition
    test_balanced_domain_decomposition(exec_conf);
    }

BOOST_AUTO_TEST_CASE( communicator_migrate_test )
    {
    communicator_creator communicator_creator_base = bind(base_class_communicator_creator, _1, _2);
    // cubic box
    test_communicator_migrate(communicator_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)),BoxDim(2.0));
    // orthorhombic box
    test_communicator_migrate(communicator_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)),BoxDim(1.0,2.0,3.0));
    // triclinic box 1
    test_communicator_migrate(communicator_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)),BoxDim(1.0,0.5,0.6,0.8));
    // triclinic box 1
    test_communicator_migrate(communicator_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)),BoxDim(1.0,-0.5,0.7,0.3));
    }

BOOST_AUTO_TEST_CASE( communicator_balanced_migrate_test )
    {
    communicator_creator communicator_creator_base = bind(base_class_communicator_creator, _1, _2);
    // cubic box
    test_communicator_balanced_migrate(communicator_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)),BoxDim(2.0));
    // orthorhombic box
    test_communicator_balanced_migrate(communicator_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)),BoxDim(1.0,2.0,3.0));
    // triclinic box 1
    test_communicator_balanced_migrate(communicator_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)),BoxDim(1.0,0.5,0.6,0.8));
    // triclinic box 1
    test_communicator_balanced_migrate(communicator_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)),BoxDim(1.0,-0.5,0.7,0.3));
    }

BOOST_AUTO_TEST_CASE( communicator_ghosts_test )
    {
    communicator_creator communicator_creator_base = bind(base_class_communicator_creator, _1, _2);

    /////////////////////
    // uniform version //
    /////////////////////
    // test in a cubic box
        {
        BoxDim box(2.0);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        test_communicator_ghosts(communicator_creator_base,
                                 exec_conf,
                                 box,
                                 boost::shared_ptr<DomainDecomposition>(new DomainDecomposition(exec_conf,box.getL())),
                                 make_scalar3(0.0,0.0,0.0));
        }
    // triclinic box 1
        {
        BoxDim box(1.0,.1,.2,.3);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        test_communicator_ghosts(communicator_creator_base,
                                 exec_conf,
                                 box,
                                 boost::shared_ptr<DomainDecomposition>(new DomainDecomposition(exec_conf,box.getL())),
                                 make_scalar3(0.0,0.0,0.0));
        }
    // triclinic box 2
        {
        BoxDim box(1.0,-.6,.7,.5);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        test_communicator_ghosts(communicator_creator_base,
                                 exec_conf,
                                 box,
                                 boost::shared_ptr<DomainDecomposition>(new DomainDecomposition(exec_conf,box.getL())),
                                 make_scalar3(0.0,0.0,0.0));
        }

    //////////////////////
    // balanced version //
    //////////////////////
    // reference fractions for the given origin in the reference BoxDim(2.0)
    Scalar3 origin = make_scalar3(0.1,-0.12,0.14);
    vector<Scalar> fx(1), fy(1), fz(1);
    fx[0] = 0.55; fy[0] = 0.44; fz[0] = 0.57;
    // test in a cubic box
        {
        BoxDim box(2.0);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        test_communicator_ghosts(communicator_creator_base,
                                 exec_conf,
                                 box,
                                 boost::shared_ptr<DomainDecomposition>(new DomainDecomposition(exec_conf,box.getL(), fx, fy, fz)),
                                 origin);
        }
    // triclinic box 1
        {
        BoxDim box(1.0,.1,.2,.3);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        test_communicator_ghosts(communicator_creator_base,
                                 exec_conf,
                                 box,
                                 boost::shared_ptr<DomainDecomposition>(new DomainDecomposition(exec_conf,box.getL(), fx, fy, fz)),
                                 origin);
        }
    // triclinic box 2
        {
        BoxDim box(1.0,-.6,.7,.5);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        test_communicator_ghosts(communicator_creator_base,
                                 exec_conf,
                                 box,
                                 boost::shared_ptr<DomainDecomposition>(new DomainDecomposition(exec_conf,box.getL(), fx, fy, fz)),
                                 origin);
        }
    }

BOOST_AUTO_TEST_CASE( communicator_bonded_ghosts_test )
    {
    communicator_creator communicator_creator_base = bind(base_class_communicator_creator, _1, _2);
    // uniform version
        {
        BoxDim box(2.0);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, box.getL()));        
        test_communicator_bonded_ghosts(communicator_creator_base,exec_conf, box, decomposition);
        }
    // balanced version
        {
        BoxDim box(2.0);
        vector<Scalar> fx(1), fy(1), fz(1);
        fx[0] = 0.52; fy[0] = 0.48; fz[0] = 0.54;
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, box.getL(), fx, fy, fz));
        test_communicator_bonded_ghosts(communicator_creator_base,exec_conf, box, decomposition);
        }
    }

BOOST_AUTO_TEST_CASE( communicator_bond_exchange_test )
    {
    communicator_creator communicator_creator_base = bind(base_class_communicator_creator, _1, _2);
    // uniform version
        {
        BoxDim box(2.0);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, box.getL()));        
        test_communicator_bond_exchange(communicator_creator_base,exec_conf, box, decomposition);
        }
    // balanced version
        {
        BoxDim box(2.0);
        vector<Scalar> fx(1), fy(1), fz(1);
        fx[0] = 0.52; fy[0] = 0.48; fz[0] = 0.54;
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, box.getL(), fx, fy, fz));
        test_communicator_bond_exchange(communicator_creator_base,exec_conf, box, decomposition);
        }
    }

BOOST_AUTO_TEST_CASE( communicator_ghost_fields_test )
    {
    communicator_creator communicator_creator_base = bind(base_class_communicator_creator, _1, _2);
    test_communicator_ghost_fields(communicator_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)));
    }

BOOST_AUTO_TEST_CASE( communicator_ghost_layer_width_test )
    {
    communicator_creator communicator_creator_base = bind(base_class_communicator_creator, _1, _2);
    test_communicator_ghost_layer_width(communicator_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)));
    }

BOOST_AUTO_TEST_CASE( communicator_ghost_layer_per_type_test )
    {
    communicator_creator communicator_creator_base = bind(base_class_communicator_creator, _1, _2);
    test_communicator_ghosts_per_type(communicator_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)),BoxDim(2.0));
    }

#ifdef ENABLE_CUDA

//! Tests particle distribution on GPU
BOOST_AUTO_TEST_CASE( DomainDecomposition_test_GPU )
    {
    boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));
    BoxDim box(2.0);
    boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, box.getL()));
    test_domain_decomposition(exec_conf, box, decomposition);
    }

//! Tests balanced particle distribution on GPU
BOOST_AUTO_TEST_CASE( BalancedDomainDecomposition_test_GPU )
    {
    boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));
    BoxDim box(2.0);
        
    // first test the fallback to the uniform grid using the standard DomainDecomposition test
    std::vector<Scalar> fxs(2), fys(2), fzs(3);
    fxs[0] = Scalar(0.5); fxs[1] = Scalar(0.5);
    fys[0] = Scalar(0.25); fys[1] = Scalar(0.75);
    fzs[0] = Scalar(0.4); fzs[1] = Scalar(0.2); fzs[2] = Scalar(0.4);
    boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, box.getL(), fxs, fys, fzs));
    test_domain_decomposition(exec_conf, box, decomposition);
    
    // then test the balanced decomposition in the test for nonuniform particles and decomposition
    test_balanced_domain_decomposition(exec_conf);
    }

BOOST_AUTO_TEST_CASE( communicator_migrate_test_GPU )
    {
    communicator_creator communicator_creator_gpu = bind(gpu_communicator_creator, _1, _2);
    // cubic box
    test_communicator_migrate(communicator_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)),BoxDim(2.0));
    // orthorhombic box
    test_communicator_migrate(communicator_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)),BoxDim(1.0,2.0,3.0));
    // triclinic box 1
    test_communicator_migrate(communicator_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)),BoxDim(1.0,0.5,0.6,0.8));
    // triclinic box 2
    test_communicator_migrate(communicator_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)),BoxDim(1.0,-0.5,0.7,0.3));
    }

BOOST_AUTO_TEST_CASE( communicator_balanced_migrate_test_GPU )
    {
    communicator_creator communicator_creator_gpu = bind(gpu_communicator_creator, _1, _2);
    // cubic box
    test_communicator_balanced_migrate(communicator_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)),BoxDim(2.0));
    // orthorhombic box
    test_communicator_balanced_migrate(communicator_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)),BoxDim(1.0,2.0,3.0));
    // triclinic box 1
    test_communicator_balanced_migrate(communicator_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)),BoxDim(1.0,0.5,0.6,0.8));
    // triclinic box 1
    test_communicator_balanced_migrate(communicator_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)),BoxDim(1.0,-0.5,0.7,0.3));
    }

BOOST_AUTO_TEST_CASE( communicator_ghosts_test_GPU )
    {
    communicator_creator communicator_creator_gpu = bind(gpu_communicator_creator, _1, _2);

    /////////////////////
    // uniform version //
    /////////////////////
    // test in a cubic box
        {
        BoxDim box(2.0);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));
        test_communicator_ghosts(communicator_creator_gpu,
                                 exec_conf,
                                 box,
                                 boost::shared_ptr<DomainDecomposition>(new DomainDecomposition(exec_conf,box.getL())),
                                 make_scalar3(0.0,0.0,0.0));
        }
    // triclinic box 1
        {
        BoxDim box(1.0,.1,.2,.3);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));
        test_communicator_ghosts(communicator_creator_gpu,
                                 exec_conf,
                                 box,
                                 boost::shared_ptr<DomainDecomposition>(new DomainDecomposition(exec_conf,box.getL())),
                                 make_scalar3(0.0,0.0,0.0));
        }
    // triclinic box 2
        {
        BoxDim box(1.0,-.6,.7,.5);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));
        test_communicator_ghosts(communicator_creator_gpu,
                                 exec_conf,
                                 box,
                                 boost::shared_ptr<DomainDecomposition>(new DomainDecomposition(exec_conf,box.getL())),
                                 make_scalar3(0.0,0.0,0.0));
        }

    //////////////////////
    // balanced version //
    //////////////////////
    // reference fractions for the given origin in the reference BoxDim(2.0)
    Scalar3 origin = make_scalar3(0.1,-0.12,0.14);
    vector<Scalar> fx(1), fy(1), fz(1);
    fx[0] = 0.55; fy[0] = 0.44; fz[0] = 0.57;
    // test in a cubic box
        {
        BoxDim box(2.0);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));
        test_communicator_ghosts(communicator_creator_gpu,
                                 exec_conf,
                                 box,
                                 boost::shared_ptr<DomainDecomposition>(new DomainDecomposition(exec_conf,box.getL(), fx, fy, fz)),
                                 origin);
        }
    // triclinic box 1
        {
        BoxDim box(1.0,.1,.2,.3);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));
        test_communicator_ghosts(communicator_creator_gpu,
                                 exec_conf,
                                 box,
                                 boost::shared_ptr<DomainDecomposition>(new DomainDecomposition(exec_conf,box.getL(), fx, fy, fz)),
                                 origin);
        }
    // triclinic box 2
        {
        BoxDim box(1.0,-.6,.7,.5);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));
        test_communicator_ghosts(communicator_creator_gpu,
                                 exec_conf,
                                 box,
                                 boost::shared_ptr<DomainDecomposition>(new DomainDecomposition(exec_conf,box.getL(), fx, fy, fz)),
                                 origin);
        }
    }

BOOST_AUTO_TEST_CASE( communicator_bonded_ghosts_test_GPU )
    {
    communicator_creator communicator_creator_gpu = bind(gpu_communicator_creator, _1, _2);
    // uniform version
        {
        BoxDim box(2.0);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));
        boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, box.getL()));        
        test_communicator_bonded_ghosts(communicator_creator_gpu, exec_conf, box, decomposition);
        }
    // balanced version
        {
        BoxDim box(2.0);
        vector<Scalar> fx(1), fy(1), fz(1);
        fx[0] = 0.52; fy[0] = 0.48; fz[0] = 0.54;
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));
        boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, box.getL(), fx, fy, fz));
        test_communicator_bonded_ghosts(communicator_creator_gpu, exec_conf, box, decomposition);
        }
    }

BOOST_AUTO_TEST_CASE( communicator_bond_exchange_test_GPU )
    {
    communicator_creator communicator_creator_gpu = bind(gpu_communicator_creator, _1, _2);
    // uniform version
        {
        BoxDim box(2.0);
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));
        boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, box.getL()));        
        test_communicator_bond_exchange(communicator_creator_gpu, exec_conf, box, decomposition);
        }
    // balanced version
        {
        BoxDim box(2.0);
        vector<Scalar> fx(1), fy(1), fz(1);
        fx[0] = 0.52; fy[0] = 0.48; fz[0] = 0.54;
        boost::shared_ptr<ExecutionConfiguration> exec_conf(new ExecutionConfiguration(ExecutionConfiguration::GPU));
        boost::shared_ptr<DomainDecomposition> decomposition(new DomainDecomposition(exec_conf, box.getL(), fx, fy, fz));
        test_communicator_bond_exchange(communicator_creator_gpu, exec_conf, box, decomposition);
        }
    }

BOOST_AUTO_TEST_CASE( communicator_ghost_fields_test_GPU )
    {
    communicator_creator communicator_creator_gpu = bind(gpu_communicator_creator, _1, _2);
    test_communicator_ghost_fields(communicator_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)));
    }

BOOST_AUTO_TEST_CASE( communicator_ghost_layer_width_test_GPU )
    {
    communicator_creator communicator_creator_gpu = bind(gpu_communicator_creator, _1, _2);
    test_communicator_ghost_layer_width(communicator_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)));
    }

BOOST_AUTO_TEST_CASE( communicator_ghost_layer_per_type_test_GPU )
    {
    communicator_creator communicator_creator_base = bind(base_class_communicator_creator, _1, _2);
    test_communicator_ghosts_per_type(communicator_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)),BoxDim(2.0));
    }

BOOST_AUTO_TEST_CASE ( communicator_compare_test )
    {
    communicator_creator communicator_creator_gpu = bind(gpu_communicator_creator, _1, _2);
    communicator_creator communicator_creator_cpu = bind(base_class_communicator_creator, _1, _2);

    // uniform case: compare cpu and gpu
        {
        BoxDim box(2.0);

        boost::shared_ptr<ExecutionConfiguration> exec_conf_1(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        boost::shared_ptr<ExecutionConfiguration> exec_conf_2(new ExecutionConfiguration(ExecutionConfiguration::GPU));
        
        boost::shared_ptr<DomainDecomposition> decomposition_1(new DomainDecomposition(exec_conf_1,box.getL()));
        boost::shared_ptr<DomainDecomposition> decomposition_2(new DomainDecomposition(exec_conf_2,box.getL()));
        test_communicator_compare(communicator_creator_cpu, communicator_creator_gpu, exec_conf_1, exec_conf_2, box, decomposition_1, decomposition_2);
        }

    // balanced case: compare cpu and gpu
        {
        BoxDim box(2.0);
        vector<Scalar> fx(1), fy(1), fz(1);
        fx[0] = 0.55; fy[0] = 0.45; fz[0] = 0.7;

        boost::shared_ptr<ExecutionConfiguration> exec_conf_1(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        boost::shared_ptr<ExecutionConfiguration> exec_conf_2(new ExecutionConfiguration(ExecutionConfiguration::GPU));
        
        boost::shared_ptr<DomainDecomposition> decomposition_1(new DomainDecomposition(exec_conf_1,box.getL(), fx, fy, fz));
        boost::shared_ptr<DomainDecomposition> decomposition_2(new DomainDecomposition(exec_conf_2,box.getL(), fx, fy, fz));
        test_communicator_compare(communicator_creator_cpu, communicator_creator_gpu, exec_conf_1, exec_conf_2, box, decomposition_1, decomposition_2);
        }

    // sanity check: compare cpu uniform and balanced with equal cuts
        {
        BoxDim box(2.0);
        vector<Scalar> fx(1), fy(1), fz(1);
        fx[0] = 0.5; fy[0] = 0.5; fz[0] = 0.5;

        boost::shared_ptr<ExecutionConfiguration> exec_conf_1(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        boost::shared_ptr<ExecutionConfiguration> exec_conf_2(new ExecutionConfiguration(ExecutionConfiguration::CPU));
        
        boost::shared_ptr<DomainDecomposition> decomposition_1(new DomainDecomposition(exec_conf_1,box.getL()));
        boost::shared_ptr<DomainDecomposition> decomposition_2(new DomainDecomposition(exec_conf_2,box.getL(),fx,fy,fz));
        test_communicator_compare(communicator_creator_cpu, communicator_creator_cpu, exec_conf_1, exec_conf_2, box, decomposition_1, decomposition_2);
        }
    }
#endif

#endif //ENABLE_MPI
