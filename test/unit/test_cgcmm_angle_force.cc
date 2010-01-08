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

// Maintainer: akohlmey

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4103 4244 )
#endif

#include <iostream>

#include <boost/bind.hpp>
#include <boost/function.hpp>

#include "CGCMMAngleForceCompute.h"
#include "ConstForceCompute.h"
#ifdef ENABLE_CUDA
#include "CGCMMAngleForceComputeGPU.h"
#endif

#include <stdio.h>

#include "Initializers.h"

using namespace std;
using namespace boost;

//! Name the boost unit test module
#define BOOST_TEST_MODULE CGCMMAngleForceTests
#include "boost_utf_configure.h"

//! Typedef to make using the boost::function factory easier
typedef boost::function<shared_ptr<CGCMMAngleForceCompute>  (shared_ptr<SystemDefinition> sysdef)> cgcmm_angleforce_creator;

//! Perform some simple functionality tests of any AngleForceCompute
void angle_force_basic_tests(cgcmm_angleforce_creator af_creator, ExecutionConfiguration exec_conf)
    {
#ifdef ENABLE_CUDA
    g_gpu_error_checking = true;
#endif
    
    /////////////////////////////////////////////////////////
    // start with the simplest possible test: 3 particles in a huge box with only one angle type !!!! NO ANGLES
    shared_ptr<SystemDefinition> sysdef_3(new SystemDefinition(3, BoxDim(1000.0), 1, 1, 1, 0, 0,  exec_conf));
    shared_ptr<ParticleData> pdata_3 = sysdef_3->getParticleData();
    
    ParticleDataArrays arrays = pdata_3->acquireReadWrite();
    arrays.x[0] = Scalar(-1.23); // put atom a at (-1,0,0.1)
    arrays.y[0] = Scalar(2.0);
    arrays.z[0] = Scalar(0.1);
    
    arrays.x[1] = arrays.y[1] = arrays.z[1] = Scalar(1.0); // put atom b at (0,0,0)
    
    arrays.x[2] = Scalar(1.0); // put atom c at (1,0,0.5)
    arrays.y[2] = 0.0;
    arrays.z[2] = Scalar(0.500);
    
    pdata_3->release();
    
    // create the angle force compute to check
    shared_ptr<CGCMMAngleForceCompute> fc_3 = af_creator(sysdef_3);
    fc_3->setParams(0, 1.0, 0.785398, 1, 1.0, 2.0); // type=0, K=1.0,theta_0=pi/4=0.785398, cg_type=1, eps=2.0, sigma=1.0
    
    // compute the force and check the results
    fc_3->compute(0);
    ForceDataArrays force_arrays = fc_3->acquire();
    
    // check that the force is correct, it should be 0 since we haven't created any angles yet
    MY_BOOST_CHECK_SMALL(force_arrays.fx[0], tol);
    MY_BOOST_CHECK_SMALL(force_arrays.fy[0], tol);
    MY_BOOST_CHECK_SMALL(force_arrays.fz[0], tol);
    MY_BOOST_CHECK_SMALL(force_arrays.pe[0], tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[0], tol);
    
    // add an angle and check again
    sysdef_3->getAngleData()->addAngle(Angle(0,0,1,2)); // add type 0 bewtween angle formed by atom 0-1-2
    fc_3->compute(1);
    
    
    // this time there should be a force
    force_arrays = fc_3->acquire();
    MY_BOOST_CHECK_CLOSE(force_arrays.fx[0], -0.061684, tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fy[0], -0.313469, tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fz[0], -0.195460, tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.pe[0], 0.158576, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[0], tol);
    
    
    // rearrange the two particles in memory and see if they are properly updated
    arrays = pdata_3->acquireReadWrite();
    
    
    arrays.x[1] = Scalar(-1.23); // put atom a at (-1,0,0.1)
    arrays.y[1] = Scalar(2.0);
    arrays.z[1] = Scalar(0.1);
    
    arrays.x[0] = arrays.y[0] = arrays.z[0] = Scalar(1.0); // put atom b at (0,0,0)
    
    arrays.tag[0] = 1;
    arrays.tag[1] = 0;
    arrays.rtag[0] = 1;
    arrays.rtag[1] = 0;
    pdata_3->release();
    
    // notify that we made the sort
    pdata_3->notifyParticleSort();
    // recompute at the same timestep, the forces should still be updated
    fc_3->compute(1);
    
    force_arrays = fc_3->acquire();
    
    MY_BOOST_CHECK_CLOSE(force_arrays.fx[1], -0.0616840, tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fy[1], -0.3134695, tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fz[1], -0.195460, tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.pe[1], 0.158576, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[1], tol);
    //pdata_3->release();
    
    ////////////////////////////////////////////////////////////////////
    // now, lets do a more thorough test and include boundary conditions
    // there are way too many permutations to test here, so I will simply
    // test +x, -x, +y, -y, +z, and -z independantly
    // build a 6 particle system with particles across each boundary
    // also test more than one type of angle
    unsigned int num_angles_to_test = 2;
    shared_ptr<SystemDefinition> sysdef_6(new SystemDefinition(6, BoxDim(20.0, 40.0, 60.0), 1, 1, num_angles_to_test, 0, 0, exec_conf));
    shared_ptr<ParticleData> pdata_6 = sysdef_6->getParticleData();
    
    arrays = pdata_6->acquireReadWrite();
    arrays.x[0] = Scalar(-9.6); arrays.y[0] = 0; arrays.z[0] = 0.0;
    arrays.x[1] =  Scalar(9.6); arrays.y[1] = 0; arrays.z[1] = 0.0;
    arrays.x[2] = 0; arrays.y[2] = Scalar(-19.6); arrays.z[2] = 0.0;
    arrays.x[3] = 0; arrays.y[3] = Scalar(19.6); arrays.z[3] = 0.0;
    arrays.x[4] = 0; arrays.y[4] = 0; arrays.z[4] = Scalar(-29.6);
    arrays.x[5] = 0; arrays.y[5] = 0; arrays.z[5] =  Scalar(29.6);
    pdata_6->release();
    
    shared_ptr<CGCMMAngleForceCompute> fc_6 = af_creator(sysdef_6);
    fc_6->setParams(0, 1.0, 0.785398, 1, 1.0, 2.0);
    fc_6->setParams(1, 2.0, 1.46, 2, 1.0, 2.0);
    
    sysdef_6->getAngleData()->addAngle(Angle(0, 0,1,2));
    sysdef_6->getAngleData()->addAngle(Angle(1, 3,4,5));
    
    fc_6->compute(0);
    // check that the forces are correctly computed
    force_arrays = fc_6->acquire();
    
    
    MY_BOOST_CHECK_SMALL(force_arrays.fx[0], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fy[0], -1.5510634,tol);
    MY_BOOST_CHECK_SMALL(force_arrays.fz[0], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.pe[0], 0.256618, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[0], tol);
    
    MY_BOOST_CHECK_CLOSE(force_arrays.fx[1], -0.0510595, tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fy[1], 1.5760721,tol);
    MY_BOOST_CHECK_SMALL(force_arrays.fz[1], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.pe[1], 0.256618, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[1], tol);
    
    MY_BOOST_CHECK_CLOSE(force_arrays.fx[2], 0.0510595,tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fy[2], -0.0250087, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.fz[2], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.pe[2], 0.256618, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[2], tol);
    
    MY_BOOST_CHECK_SMALL(force_arrays.fx[3], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fy[3], 0.0515151, tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fz[3], -0.03411135,tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.pe[3], 0.400928, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[3], tol);
    
    MY_BOOST_CHECK_SMALL(force_arrays.fx[4], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fy[4], -2.793305,tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fz[4], 0.0341109, tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.pe[4], 0.400928, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[4], tol);
    
    MY_BOOST_CHECK_SMALL(force_arrays.fx[5], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fy[5], 2.74178982,tol);
    MY_BOOST_CHECK_SMALL(force_arrays.fz[5], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.pe[5], 0.400928, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[5], tol);
    
    //////////////////////////////////////////////////////////////////////
    // THE DREADED 4 PARTICLE TEST -- see CGCMMAngleForceGPU.cu //
    //////////////////////////////////////////////////////////////////////
    // one more test: this one will test two things:
    // 1) That the forces are computed correctly even if the particles are rearranged in memory
    // and 2) That two forces can add to the same particle
    shared_ptr<SystemDefinition> sysdef_4(new SystemDefinition(4, BoxDim(100.0, 100.0, 100.0), 1, 1, 3, 0, 0, exec_conf));
    shared_ptr<ParticleData> pdata_4 = sysdef_4->getParticleData();
    
    arrays = pdata_4->acquireReadWrite();
    // make a square of particles
    arrays.x[0] = 0.0; arrays.y[0] = 0.0; arrays.z[0] = 0.0;
    arrays.x[1] = 1.0; arrays.y[1] = 0; arrays.z[1] = 0.0;
    arrays.x[2] = 0; arrays.y[2] = 1.0; arrays.z[2] = 0.0;
    arrays.x[3] = 1.0; arrays.y[3] = 1.0; arrays.z[3] = 0.0;
    
    arrays.tag[0] = 2;
    arrays.tag[1] = 3;
    arrays.tag[2] = 0;
    arrays.tag[3] = 1;
    arrays.rtag[arrays.tag[0]] = 0;
    arrays.rtag[arrays.tag[1]] = 1;
    arrays.rtag[arrays.tag[2]] = 2;
    arrays.rtag[arrays.tag[3]] = 3;
    pdata_4->release();
    
    // build the angle force compute and try it out
    shared_ptr<CGCMMAngleForceCompute> fc_4 = af_creator(sysdef_4);
//  fc_4->setParams(0, 1.5, 1.75, 2, 1.0, 2.0);
    fc_4->setParams(0, 1.0, 0.785398, 1, 1.0, 0.45);
    fc_4->setParams(1, 12.3, 0.21112, 2, 1.0, 0.45);
    fc_4->setParams(2, 22.0, 0.3772, 3, 1.0, 0.65);
    // only add angles on the left, top, and bottom of the square
    sysdef_4->getAngleData()->addAngle(Angle(0, 0,1,2));
    sysdef_4->getAngleData()->addAngle(Angle(1, 1,2,3));
    sysdef_4->getAngleData()->addAngle(Angle(0, 0,1,3));
    
    fc_4->compute(0);
    force_arrays = fc_4->acquire();
    
    MY_BOOST_CHECK_CLOSE(force_arrays.fx[0], -3.531810,tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fy[0], -3.531810, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.fz[0], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.pe[0], 0.676081, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[0], tol);
    
    MY_BOOST_CHECK_CLOSE(force_arrays.fx[1], -0.785398,tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fy[1], 7.063621,tol);
    MY_BOOST_CHECK_SMALL(force_arrays.fz[1], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.pe[1], 0.778889, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[1], tol);
    
    
    MY_BOOST_CHECK_SMALL(force_arrays.fx[2], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fy[2], -0.785399,tol);
    MY_BOOST_CHECK_SMALL(force_arrays.fz[2], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.pe[2], 0.102808, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[2], tol);
    
    
    MY_BOOST_CHECK_CLOSE(force_arrays.fx[3], 4.317209,tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.fy[3], -2.746412,tol);
    MY_BOOST_CHECK_SMALL(force_arrays.fz[3], tol);
    MY_BOOST_CHECK_CLOSE(force_arrays.pe[3], 0.778889, tol);
    MY_BOOST_CHECK_SMALL(force_arrays.virial[3], tol);
    
    }





//! Compares the output of two CGCMMAngleForceComputes
void angle_force_comparison_tests(cgcmm_angleforce_creator af_creator1, cgcmm_angleforce_creator af_creator2, ExecutionConfiguration exec_conf)
    {
#ifdef ENABLE_CUDA
    g_gpu_error_checking = true;
#endif
    
    const unsigned int N = 1000;
    
    // create a particle system to sum forces on
    // just randomly place particles. We don't really care how huge the angle forces get: this is just a unit test
    RandomInitializer rand_init(N, Scalar(0.2), Scalar(0.9), "A");
    shared_ptr<SystemDefinition> sysdef(new SystemDefinition(rand_init, exec_conf));
    
    shared_ptr<CGCMMAngleForceCompute> fc1 = af_creator1(sysdef);
    shared_ptr<CGCMMAngleForceCompute> fc2 = af_creator2(sysdef);
    fc1->setParams(0, Scalar(1.0), Scalar(1.348), 1, Scalar(1.0), Scalar(0.05));
    fc2->setParams(0, Scalar(1.0), Scalar(1.348), 1, Scalar(1.0), Scalar(0.05));
    
    // add angles
    for (unsigned int i = 0; i < N-2; i++)
        {
        sysdef->getAngleData()->addAngle(Angle(0, i, i+1,i+2));
        }
        
    // compute the forces
    fc1->compute(0);
    fc2->compute(0);
    
    // verify that the forces are identical (within roundoff errors)
    ForceDataArrays arrays1 = fc1->acquire();
    ForceDataArrays arrays2 = fc2->acquire();
    
    // compare average deviation between the two computes
    double deltaf2 = 0.0;
    double deltape2 = 0.0;
        
    for (unsigned int i = 0; i < N; i++)
        {
        deltaf2 += double(arrays1.fx[i] - arrays2.fx[i]) * double(arrays1.fx[i] - arrays2.fx[i]);
        deltaf2 += double(arrays1.fy[i] - arrays2.fy[i]) * double(arrays1.fy[i] - arrays2.fy[i]);
        deltaf2 += double(arrays1.fz[i] - arrays2.fz[i]) * double(arrays1.fz[i] - arrays2.fz[i]);
        deltape2 += double(arrays1.pe[i] - arrays2.pe[i]) * double(arrays1.pe[i] - arrays2.pe[i]);

        // also check that each individual calculation is somewhat close
        BOOST_CHECK_CLOSE(arrays1.fx[i], arrays2.fx[i], loose_tol);
        BOOST_CHECK_CLOSE(arrays1.fy[i], arrays2.fy[i], loose_tol);
        BOOST_CHECK_CLOSE(arrays1.fz[i], arrays2.fz[i], loose_tol);
        BOOST_CHECK_CLOSE(arrays1.pe[i], arrays2.pe[i], loose_tol);
        }
    deltaf2 /= double(sysdef->getParticleData()->getN());
    deltape2 /= double(sysdef->getParticleData()->getN());
    BOOST_CHECK_SMALL(deltaf2, double(tol_small));
    BOOST_CHECK_SMALL(deltape2, double(tol_small));
    }


//! CGCMMAngleForceCompute creator for angle_force_basic_tests()
shared_ptr<CGCMMAngleForceCompute> base_class_af_creator(shared_ptr<SystemDefinition> sysdef)
    {
    return shared_ptr<CGCMMAngleForceCompute>(new CGCMMAngleForceCompute(sysdef));
    }

#ifdef ENABLE_CUDA
//! AngleForceCompute creator for angle_force_basic_tests()
shared_ptr<CGCMMAngleForceCompute> gpu_af_creator(shared_ptr<SystemDefinition> sysdef)
    {
    return shared_ptr<CGCMMAngleForceCompute>(new CGCMMAngleForceComputeGPU(sysdef));
    }
#endif

//! boost test case for angle forces on the CPU
BOOST_AUTO_TEST_CASE( CGCMMAngleForceCompute_basic )
    {
    printf(" IN BOOST_AUTO_TEST_CASE: CPU \n");
    cgcmm_angleforce_creator af_creator = bind(base_class_af_creator, _1);
    angle_force_basic_tests(af_creator, ExecutionConfiguration(ExecutionConfiguration::CPU));
    }

#ifdef ENABLE_CUDA
//! boost test case for angle forces on the GPU
BOOST_AUTO_TEST_CASE( CGCMMAngleForceComputeGPU_basic )
    {
    printf(" IN BOOST_AUTO_TEST_CASE: GPU \n");
    cgcmm_angleforce_creator af_creator = bind(gpu_af_creator, _1);
    angle_force_basic_tests(af_creator, ExecutionConfiguration(ExecutionConfiguration::GPU));
    }


//! boost test case for comparing angle GPU and CPU AngleForceComputes
BOOST_AUTO_TEST_CASE( CGCMMAngleForceComputeGPU_compare )
    {
    cgcmm_angleforce_creator af_creator_gpu = bind(gpu_af_creator, _1);
    cgcmm_angleforce_creator af_creator = bind(base_class_af_creator, _1);
    angle_force_comparison_tests(af_creator, af_creator_gpu, ExecutionConfiguration(ExecutionConfiguration::GPU));
    }

#endif
