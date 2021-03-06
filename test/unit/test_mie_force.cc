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
#include <fstream>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include "AllPairPotentials.h"

#include "NeighborListTree.h"
#include "Initializers.h"

#include <math.h>

using namespace std;
using namespace boost;

/*! \file mie_force_test.cc
    \brief Implements unit tests for PotentialPairMie and PotentialPairMieGPU and descendants
    \ingroup unit_tests
*/

//! Name the unit test module
#define BOOST_TEST_MODULE PotentialPairMieTests
#include "boost_utf_configure.h"

//! Typedef'd MieForceCompute factory
typedef boost::function<boost::shared_ptr<PotentialPairMie> (boost::shared_ptr<SystemDefinition> sysdef,
                                                     boost::shared_ptr<NeighborList> nlist)> mieforce_creator;

//! Test the ability of the mie force compute to actually calucate forces
void mie_force_particle_test(mieforce_creator mie_creator, boost::shared_ptr<ExecutionConfiguration> exec_conf)
    {
    // this 3-particle test subtly checks several conditions
    // the particles are arranged on the x axis,  1   2   3
    // such that 2 is inside the cuttoff radius of 1 and 3, but 1 and 3 are outside the cuttoff
    // of course, the buffer will be set on the neighborlist so that 3 is included in it
    // thus, this case tests the ability of the force summer to sum more than one force on
    // a particle and ignore a particle outside the radius

    // periodic boundary conditions will be handeled in another test
    boost::shared_ptr<SystemDefinition> sysdef_3(new SystemDefinition(3, BoxDim(1000.0), 1, 0, 0, 0, 0, exec_conf));
    boost::shared_ptr<ParticleData> pdata_3 = sysdef_3->getParticleData();
    pdata_3->setFlags(~PDataFlags(0));

    {
    ArrayHandle<Scalar4> h_pos(pdata_3->getPositions(), access_location::host, access_mode::readwrite);
    h_pos.data[0].x = h_pos.data[0].y = h_pos.data[0].z = 0.0;
    h_pos.data[1].x = Scalar(pow(27.0/13.0, 1.0/7.0)); h_pos.data[1].y = h_pos.data[1].z = 0.0;
    h_pos.data[2].x = Scalar(2.0*pow(27.0/13.0 ,1.0/7.0)); h_pos.data[2].y = h_pos.data[2].z = 0.0;
    }
    boost::shared_ptr<NeighborList> nlist_3(new NeighborListTree(sysdef_3, Scalar(1.3), Scalar(3.0)));
    boost::shared_ptr<PotentialPairMie> fc_3 = mie_creator(sysdef_3, nlist_3);
    fc_3->setRcut(0, 0, Scalar(1.3));

    // first test: setup a sigma of 1.0 so that all forces will be 0
    Scalar epsilon = Scalar(1.15);
    Scalar sigma = Scalar(1.0);
    Scalar mie3 = Scalar(13.5);
    Scalar mie4 = Scalar(6.5);
    Scalar mie1 = epsilon * Scalar(pow(sigma,mie3)) * Scalar(mie3/(mie3-mie4)) * Scalar(pow(mie3/mie4,(mie4/(mie3-mie4))));
    Scalar mie2 = epsilon * Scalar(pow(sigma,mie4)) * Scalar(mie3/(mie3-mie4)) * Scalar(pow(mie3/mie4,(mie4/(mie3-mie4))));
    fc_3->setParams(0,0,make_scalar4(mie1,mie2,mie3,mie4));

    // compute the forces
    fc_3->compute(0);

    {
    GPUArray<Scalar4>& force_array_1 =  fc_3->getForceArray();
    GPUArray<Scalar>& virial_array_1 =  fc_3->getVirialArray();
    unsigned int pitch = virial_array_1.getPitch();
    ArrayHandle<Scalar4> h_force_1(force_array_1,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_1(virial_array_1,access_location::host,access_mode::read);
    MY_BOOST_CHECK_SMALL(h_force_1.data[0].x, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_1.data[0].y, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_1.data[0].z, tol_small);
    MY_BOOST_CHECK_CLOSE(h_force_1.data[0].w, -0.575, tol);
    MY_BOOST_CHECK_SMALL(h_virial_1.data[0*pitch+0]
                        +h_virial_1.data[3*pitch+0]
                        +h_virial_1.data[5*pitch+0], tol_small);

    MY_BOOST_CHECK_SMALL(h_force_1.data[1].x, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_1.data[1].y, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_1.data[1].z, tol_small);
    MY_BOOST_CHECK_CLOSE(h_force_1.data[1].w, -1.15, tol);
    MY_BOOST_CHECK_SMALL(h_virial_1.data[0*pitch+1]
                        +h_virial_1.data[3*pitch+1]
                        +h_virial_1.data[5*pitch+1], tol_small);

    MY_BOOST_CHECK_SMALL(h_force_1.data[2].x, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_1.data[2].y, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_1.data[2].z, tol_small);
    MY_BOOST_CHECK_CLOSE(h_force_1.data[2].w, -0.575, tol);
    MY_BOOST_CHECK_SMALL(h_virial_1.data[0*pitch+2]
                        +h_virial_1.data[3*pitch+2]
                        +h_virial_1.data[5*pitch+2], tol_small);
    }

    // now change sigma and alpha so we can check that it is computing the right force
    sigma = Scalar(1.2); // < bigger sigma should push particle 0 left and particle 2 right
    mie1 = epsilon * Scalar(pow(sigma,mie3)) * Scalar(mie3/(mie3-mie4)) * Scalar(pow(mie3/mie4,(mie4/(mie3-mie4))));
    mie2 = epsilon * Scalar(pow(sigma,mie4)) * Scalar(mie3/(mie3-mie4)) * Scalar(pow(mie3/mie4,(mie4/(mie3-mie4))));
    fc_3->setParams(0,0,make_scalar4(mie1,mie2,mie3,mie4));
    fc_3->compute(1);

    {
    GPUArray<Scalar4>& force_array_2 =  fc_3->getForceArray();
    GPUArray<Scalar>& virial_array_2 =  fc_3->getVirialArray();
    unsigned int pitch = virial_array_2.getPitch();
    ArrayHandle<Scalar4> h_force_2(force_array_2,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_2(virial_array_2,access_location::host,access_mode::read);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[0].x, -109.7321922512963, tol);
    MY_BOOST_CHECK_SMALL(h_force_2.data[0].y, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_2.data[0].z, tol_small);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[0].w, 2.6306347172235, tol);
    MY_BOOST_CHECK_CLOSE(Scalar(1./3.)*(h_virial_2.data[0*pitch+0]
                                       +h_virial_2.data[3*pitch+0]
                                       +h_virial_2.data[5*pitch+0]), 20.301521082055, tol);

    // center particle should still be a 0 force by symmetry
    MY_BOOST_CHECK_SMALL(h_force_2.data[1].x, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_2.data[1].y, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_2.data[1].z, tol_small);
    // there is still an energy and virial, though
    MY_BOOST_CHECK_CLOSE(h_force_2.data[1].w, 5.2612694344471, tol);
    MY_BOOST_CHECK_CLOSE(Scalar(1./3.)*(h_virial_2.data[0*pitch+1]
                                       +h_virial_2.data[3*pitch+1]
                                       +h_virial_2.data[5*pitch+1]), 40.603042164109, tol);

    MY_BOOST_CHECK_CLOSE(h_force_2.data[2].x, 109.7321922512963, tol);
    MY_BOOST_CHECK_SMALL(h_force_2.data[2].y, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_2.data[2].z, tol_small);
    MY_BOOST_CHECK_CLOSE(h_force_2.data[2].w, 2.6306347172235, tol);
    MY_BOOST_CHECK_CLOSE(Scalar(1./3.)*(h_virial_2.data[0*pitch+2]
                                       +h_virial_2.data[3*pitch+2]
                                       +h_virial_2.data[5*pitch+2]), 20.301521082055, tol);
    }

    // swap the order of particles 0 ans 2 in memory to check that the force compute handles this properly
    {
    ArrayHandle<Scalar4> h_pos(pdata_3->getPositions(), access_location::host, access_mode::readwrite);
    ArrayHandle<unsigned int> h_tag(pdata_3->getTags(), access_location::host, access_mode::readwrite);
    ArrayHandle<unsigned int> h_rtag(pdata_3->getRTags(), access_location::host, access_mode::readwrite);

    h_pos.data[2].x = h_pos.data[2].y = h_pos.data[2].z = 0.0;
    h_pos.data[0].x = Scalar(2.0*pow(27.0/13.0, 1.0/7.0)); h_pos.data[0].y = h_pos.data[0].z = 0.0;

    h_tag.data[0] = 2;
    h_tag.data[2] = 0;
    h_rtag.data[0] = 2;
    h_rtag.data[2] = 0;
    }

    // notify the particle data that we changed the order
    pdata_3->notifyParticleSort();

    // recompute the forces at the same timestep, they should be updated
    fc_3->compute(1);

    {
    GPUArray<Scalar4>& force_array_3 =  fc_3->getForceArray();
    GPUArray<Scalar>& virial_array_3 =  fc_3->getVirialArray();
    ArrayHandle<Scalar4> h_force_3(force_array_3,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_3(virial_array_3,access_location::host,access_mode::read);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[0].x, 109.7321922512963, tol);
    MY_BOOST_CHECK_CLOSE(h_force_3.data[2].x, -109.7321922512963, tol);
    }
    }


//! Unit test a comparison between 2 MieForceComputes on a "real" system
void mie_force_comparison_test(mieforce_creator mie_creator1, mieforce_creator mie_creator2, boost::shared_ptr<ExecutionConfiguration> exec_conf)
    {
    const unsigned int N = 5000;

    // create a random particle system to sum forces on
    RandomInitializer rand_init(N, Scalar(0.2), Scalar(0.9), "A");
    boost::shared_ptr< SnapshotSystemData<Scalar> > snap = rand_init.getSnapshot();
    boost::shared_ptr<SystemDefinition> sysdef(new SystemDefinition(snap, exec_conf));
    boost::shared_ptr<ParticleData> pdata = sysdef->getParticleData();
    pdata->setFlags(~PDataFlags(0));

    boost::shared_ptr<NeighborListTree> nlist(new NeighborListTree(sysdef, Scalar(3.0), Scalar(0.8)));

    boost::shared_ptr<PotentialPairMie> fc1 = mie_creator1(sysdef, nlist);
    boost::shared_ptr<PotentialPairMie> fc2 = mie_creator2(sysdef, nlist);
    fc1->setRcut(0, 0, Scalar(3.0));
    fc2->setRcut(0, 0, Scalar(3.0));

    // setup some values for alpha and sigma
    Scalar epsilon = Scalar(1.0);
    Scalar sigma = Scalar(1.2);
    Scalar mie3 = Scalar(13.5);
    Scalar mie4 = Scalar(6.5);
    Scalar mie1 = epsilon * Scalar(pow(sigma,mie3)) * Scalar(mie3/(mie3-mie4)) * Scalar(pow(mie3/mie4,(mie4/(mie3-mie4))));
    Scalar mie2 = epsilon * Scalar(pow(sigma,mie4)) * Scalar(mie3/(mie3-mie4)) * Scalar(pow(mie3/mie4,(mie4/(mie3-mie4)))); 

    // specify the force parameters
    fc1->setParams(0,0,make_scalar4(mie1,mie2,mie3,mie4));
    fc2->setParams(0,0,make_scalar4(mie1,mie2,mie3,mie4));

    // compute the forces
    fc1->compute(0);
    fc2->compute(0);

    {
    // verify that the forces are identical (within roundoff errors)
    GPUArray<Scalar4>& force_array_5 =  fc1->getForceArray();
    GPUArray<Scalar>& virial_array_5 =  fc1->getVirialArray();
    unsigned int pitch = virial_array_5.getPitch();
    ArrayHandle<Scalar4> h_force_5(force_array_5,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_5(virial_array_5,access_location::host,access_mode::read);
    GPUArray<Scalar4>& force_array_6 =  fc2->getForceArray();
    GPUArray<Scalar>& virial_array_6 =  fc2->getVirialArray();
    ArrayHandle<Scalar4> h_force_6(force_array_6,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_6(virial_array_6,access_location::host,access_mode::read);

    // compare average deviation between the two computes
    double deltaf2 = 0.0;
    double deltape2 = 0.0;
    double deltav2[6];
    for (unsigned int i = 0; i < 6; i++)
        deltav2[i] = 0.0;

    for (unsigned int i = 0; i < N; i++)
        {
        deltaf2 += double(h_force_6.data[i].x - h_force_5.data[i].x) * double(h_force_6.data[i].x - h_force_5.data[i].x);
        deltaf2 += double(h_force_6.data[i].y - h_force_5.data[i].y) * double(h_force_6.data[i].y - h_force_5.data[i].y);
        deltaf2 += double(h_force_6.data[i].z - h_force_5.data[i].z) * double(h_force_6.data[i].z - h_force_5.data[i].z);
        deltape2 += double(h_force_6.data[i].w - h_force_5.data[i].w) * double(h_force_6.data[i].w - h_force_5.data[i].w);
        for (unsigned int j = 0; j < 6; j++)
            deltav2[j] += double(h_virial_6.data[j*pitch+i] - h_virial_5.data[j*pitch+i]) * double(h_virial_6.data[j*pitch+i] - h_virial_5.data[j*pitch+i]);

        // also check that each individual calculation is somewhat close
        }
    deltaf2 /= double(pdata->getN());
    deltape2 /= double(pdata->getN());
    for (unsigned int j = 0; j < 6; j++)
        deltav2[j] /= double(pdata->getN());
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

//! Test the ability of the mie force compute to compute forces with different shift modes
void mie_force_shift_test(mieforce_creator mie_creator, boost::shared_ptr<ExecutionConfiguration> exec_conf)
    {
    // this 2-particle test is just to get a plot of the potential and force vs r cut
    boost::shared_ptr<SystemDefinition> sysdef_2(new SystemDefinition(2, BoxDim(1000.0), 1, 0, 0, 0, 0, exec_conf));
    boost::shared_ptr<ParticleData> pdata_2 = sysdef_2->getParticleData();
    pdata_2->setFlags(~PDataFlags(0));

    {
    ArrayHandle<Scalar4> h_pos(pdata_2->getPositions(), access_location::host, access_mode::readwrite);

    h_pos.data[0].x = h_pos.data[0].y = h_pos.data[0].z = 0.0;
    h_pos.data[1].x = Scalar(2.8); h_pos.data[1].y = h_pos.data[1].z = 0.0;
    }

    boost::shared_ptr<NeighborList> nlist_2(new NeighborListTree(sysdef_2, Scalar(3.0), Scalar(0.8)));
    boost::shared_ptr<PotentialPairMie> fc_no_shift = mie_creator(sysdef_2, nlist_2);
    fc_no_shift->setRcut(0, 0, Scalar(3.0));
    fc_no_shift->setShiftMode(PotentialPairMie::no_shift);

    boost::shared_ptr<PotentialPairMie> fc_shift = mie_creator(sysdef_2, nlist_2);
    fc_shift->setRcut(0, 0, Scalar(3.0));
    fc_shift->setShiftMode(PotentialPairMie::shift);

    boost::shared_ptr<PotentialPairMie> fc_xplor = mie_creator(sysdef_2, nlist_2);
    fc_xplor->setRcut(0, 0, Scalar(3.0));
    fc_xplor->setShiftMode(PotentialPairMie::xplor);
    fc_xplor->setRon(0, 0, Scalar(2.0));

    nlist_2->setStorageMode(NeighborList::full);

    // setup a standard epsilon and sigma
    Scalar epsilon = Scalar(1.0);
    Scalar sigma = Scalar(1.0);
    Scalar mie3 = Scalar(13.5);
    Scalar mie4 = Scalar(6.5);
    Scalar mie1 = epsilon * Scalar(pow(sigma,mie3)) * Scalar(mie3/(mie3-mie4)) * Scalar(pow(mie3/mie4,(mie4/(mie3-mie4))));
    Scalar mie2 = epsilon * Scalar(pow(sigma,mie4)) * Scalar(mie3/(mie3-mie4)) * Scalar(pow(mie3/mie4,(mie4/(mie3-mie4))));
    fc_no_shift->setParams(0,0,make_scalar4(mie1,mie2,mie3,mie4));
    fc_shift->setParams(0,0,make_scalar4(mie1,mie2,mie3,mie4));
    fc_xplor->setParams(0,0,make_scalar4(mie1,mie2,mie3,mie4));

    fc_no_shift->compute(0);
    fc_shift->compute(0);
    fc_xplor->compute(0);

    {
    GPUArray<Scalar4>& force_array_7 =  fc_no_shift->getForceArray();
    GPUArray<Scalar>& virial_array_7 =  fc_no_shift->getVirialArray();
    ArrayHandle<Scalar4> h_force_7(force_array_7,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_7(virial_array_7,access_location::host,access_mode::read);

    MY_BOOST_CHECK_CLOSE(h_force_7.data[0].x, 0.010928042234617, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[0].w, -0.0023556136748908, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[1].x, -0.010928042234617, tol);
    MY_BOOST_CHECK_CLOSE(h_force_7.data[1].w, -0.0023556136748908, tol);

    // shifted just has pe shifted by a given amount
    GPUArray<Scalar4>& force_array_8 =  fc_shift->getForceArray();
    GPUArray<Scalar>& virial_array_8 =  fc_shift->getVirialArray();
    ArrayHandle<Scalar4> h_force_8(force_array_8,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_8(virial_array_8,access_location::host,access_mode::read);
    MY_BOOST_CHECK_CLOSE(h_force_8.data[0].x, 0.010928042234617, tol);
    MY_BOOST_CHECK_CLOSE(h_force_8.data[0].w, -0.00085085631210834, tol);
    MY_BOOST_CHECK_CLOSE(h_force_8.data[1].x, -0.010928042234617, tol);
    MY_BOOST_CHECK_CLOSE(h_force_8.data[1].w, -0.00085085631210834, tol);

    // xplor has slight tweaks
    GPUArray<Scalar4>& force_array_9 =  fc_xplor->getForceArray();
    GPUArray<Scalar>& virial_array_9 =  fc_xplor->getVirialArray();
    ArrayHandle<Scalar4> h_force_9(force_array_9,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_9(virial_array_9,access_location::host,access_mode::read);
    MY_BOOST_CHECK_CLOSE(h_force_9.data[0].x, 0.0071326060066445, tol);
    MY_BOOST_CHECK_CLOSE(h_force_9.data[0].w, -0.00032153576390906, tol);
    MY_BOOST_CHECK_CLOSE(h_force_9.data[1].x, -0.0071326060066445, tol);
    MY_BOOST_CHECK_CLOSE(h_force_9.data[1].w, -0.00032153576390906, tol);
    }

    // check again, prior to r_on to make sure xplor isn't doing something weird
    {
    ArrayHandle<Scalar4> h_pos(pdata_2->getPositions(), access_location::host, access_mode::readwrite);

    h_pos.data[0].x = h_pos.data[0].y = h_pos.data[0].z = 0.0;
    h_pos.data[1].x = Scalar(1.5); h_pos.data[1].y = h_pos.data[1].z = 0.0;
    }

    fc_no_shift->compute(1);
    fc_shift->compute(1);
    fc_xplor->compute(1);

    {
    GPUArray<Scalar4>& force_array_10 =  fc_no_shift->getForceArray();
    GPUArray<Scalar>& virial_array_10 =  fc_no_shift->getVirialArray();
    ArrayHandle<Scalar4> h_force_10(force_array_10,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_10(virial_array_10,access_location::host,access_mode::read);

    MY_BOOST_CHECK_CLOSE(h_force_10.data[0].x, 1.0373505201621, tol);
    MY_BOOST_CHECK_CLOSE(h_force_10.data[0].w, -0.12828256582666, tol);
    MY_BOOST_CHECK_CLOSE(h_force_10.data[1].x, -1.0373505201621, tol);
    MY_BOOST_CHECK_CLOSE(h_force_10.data[1].w, -0.12828256582666, tol);

    // shifted just has pe shifted by a given amount
    GPUArray<Scalar4>& force_array_11 =  fc_shift->getForceArray();
    GPUArray<Scalar>& virial_array_11 =  fc_shift->getVirialArray();
    ArrayHandle<Scalar4> h_force_11(force_array_11,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_11(virial_array_11,access_location::host,access_mode::read);
    MY_BOOST_CHECK_CLOSE(h_force_11.data[0].x, 1.0373505201621, tol);
    MY_BOOST_CHECK_CLOSE(h_force_11.data[0].w, -0.12677780846387, tol);
    MY_BOOST_CHECK_CLOSE(h_force_11.data[1].x, -1.0373505201621, tol);
    MY_BOOST_CHECK_CLOSE(h_force_11.data[1].w, -0.12677780846387, tol);

    // xplor has slight tweaks
    GPUArray<Scalar4>& force_array_12 =  fc_xplor->getForceArray();
    GPUArray<Scalar>& virial_array_12 =  fc_xplor->getVirialArray();
    ArrayHandle<Scalar4> h_force_12(force_array_12,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_12(virial_array_12,access_location::host,access_mode::read);
    MY_BOOST_CHECK_CLOSE(h_force_12.data[0].x, 1.0373505201621, tol);
    MY_BOOST_CHECK_CLOSE(h_force_12.data[0].w, -0.12828256582666, tol);
    MY_BOOST_CHECK_CLOSE(h_force_12.data[1].x, -1.0373505201621, tol);
    MY_BOOST_CHECK_CLOSE(h_force_12.data[1].w, -0.12828256582666, tol);
    }

    // check once again to verify that nothing fish happens past r_cut
    {
    ArrayHandle<Scalar4> h_pos(pdata_2->getPositions(), access_location::host, access_mode::readwrite);

    h_pos.data[0].x = h_pos.data[0].y = h_pos.data[0].z = 0.0;
    h_pos.data[1].x = Scalar(3.1); h_pos.data[1].y = h_pos.data[1].z = 0.0;
    }

    fc_no_shift->compute(2);
    fc_shift->compute(2);
    fc_xplor->compute(2);

    {
    GPUArray<Scalar4>& force_array_13 =  fc_no_shift->getForceArray();
    GPUArray<Scalar>& virial_array_13 =  fc_no_shift->getVirialArray();
    ArrayHandle<Scalar4> h_force_13(force_array_13,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_13(virial_array_13,access_location::host,access_mode::read);

    MY_BOOST_CHECK_SMALL(h_force_13.data[0].x, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_13.data[0].w, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_13.data[1].x, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_13.data[1].w, tol_small);

    // shifted just has pe shifted by a given amount
    GPUArray<Scalar4>& force_array_14 =  fc_shift->getForceArray();
    GPUArray<Scalar>& virial_array_14 =  fc_shift->getVirialArray();
    ArrayHandle<Scalar4> h_force_14(force_array_14,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_14(virial_array_14,access_location::host,access_mode::read);
    MY_BOOST_CHECK_SMALL(h_force_14.data[0].x, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_14.data[0].w, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_14.data[1].x, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_14.data[1].w, tol_small);

    // xplor has slight tweaks
    GPUArray<Scalar4>& force_array_15 =  fc_xplor->getForceArray();
    GPUArray<Scalar>& virial_array_15 =  fc_xplor->getVirialArray();
    ArrayHandle<Scalar4> h_force_15(force_array_15,access_location::host,access_mode::read);
    ArrayHandle<Scalar> h_virial_15(virial_array_15,access_location::host,access_mode::read);
    MY_BOOST_CHECK_SMALL(h_force_15.data[0].x, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_15.data[0].w, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_15.data[1].x, tol_small);
    MY_BOOST_CHECK_SMALL(h_force_15.data[1].w, tol_small);
    }
    }

//! MieForceCompute creator for unit tests
boost::shared_ptr<PotentialPairMie> base_class_mie_creator(boost::shared_ptr<SystemDefinition> sysdef,
                                                  boost::shared_ptr<NeighborList> nlist)
    {
    return boost::shared_ptr<PotentialPairMie>(new PotentialPairMie(sysdef, nlist));
    }

#ifdef ENABLE_CUDA
//! MieForceComputeGPU creator for unit tests
boost::shared_ptr<PotentialPairMieGPU> gpu_mie_creator(boost::shared_ptr<SystemDefinition> sysdef,
                                          boost::shared_ptr<NeighborList> nlist)
    {
    nlist->setStorageMode(NeighborList::full);
    boost::shared_ptr<PotentialPairMieGPU> mie(new PotentialPairMieGPU(sysdef, nlist));
    return mie;
    }
#endif

//! boost test case for particle test on CPU
BOOST_AUTO_TEST_CASE( PotentialPairMie_particle )
    {
    mieforce_creator mie_creator_base = bind(base_class_mie_creator, _1, _2);
    mie_force_particle_test(mie_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)));
    }

//! boost test case for shift test on CPU
BOOST_AUTO_TEST_CASE( PotentialPairMie_shift )
    {
    mieforce_creator mie_creator_base = bind(base_class_mie_creator, _1, _2);
    mie_force_shift_test(mie_creator_base, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::CPU)));
    }

# ifdef ENABLE_CUDA
//! boost test case for particle test on GPU
BOOST_AUTO_TEST_CASE( MieForceGPU_particle )
    {
    mieforce_creator mie_creator_gpu = bind(gpu_mie_creator, _1, _2);
    mie_force_particle_test(mie_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)));
    }

//! boost test case for shift test on GPU
BOOST_AUTO_TEST_CASE( MieForceGPU_shift )
    {
    mieforce_creator mie_creator_gpu = bind(gpu_mie_creator, _1, _2);
    mie_force_shift_test(mie_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)));
    }

//! boost test case for comparing GPU output to base class output
/*BOOST_AUTO_TEST_CASE( MieForceGPU_compare )
    {
    mieforce_creator mie_creator_gpu = bind(gpu_mie_creator, _1, _2);
    mieforce_creator mie_creator_base = bind(base_class_mie_creator, _1, _2);
    mie_force_comparison_test(mie_creator_base, mie_creator_gpu, boost::shared_ptr<ExecutionConfiguration>(new ExecutionConfiguration(ExecutionConfiguration::GPU)));
    }*/

#endif
