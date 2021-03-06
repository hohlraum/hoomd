# -*- coding: iso-8859-1 -*-
# Maintainer: joaander

from hoomd_script import *
import unittest
import os
import numpy
import math
context.initialize()

def almost_equal(u, v, e=0.001):
    return math.fabs(u-v)/math.fabs(u) <= e and math.fabs(u-v) / math.fabs(v) <= e;

# unit tests for integrate.langevin
class integrate_langevin_tests (unittest.TestCase):
    def setUp(self):
        print
        self.s = init.create_random(N=100, phi_p=0.05);
        force.constant(fx=0.1, fy=0.1, fz=0.1)

        sorter.set_params(grid=8)

    # tests basic creation of the integration method
    def test(self):
        all = group.all();
        integrate.mode_standard(dt=0.005);
        bd = integrate.langevin(all, T=1.2, seed=52);
        run(100);
        bd.disable();
        bd = integrate.langevin(all, T=1.2, seed=1, dscale=1.0);
        run(100);
        bd.disable();
        bd = integrate.langevin(all, T=1.2, seed=1, dscale=1.0, tally=True);
        run(100);
        bd.disable();

    # test set_params
    def test_set_params(self):
        all = group.all();
        bd = integrate.langevin(all, T=1.2, seed=1);
        bd.set_params(T=1.3);
        bd.set_params(tally=False);

    # test set_gamma
    def test_set_gamma(self):
        all = group.all();
        bd = integrate.langevin(all, T=1.2, seed=1);
        bd.set_gamma('A', 0.5);
        bd.set_gamma('B', 1.0);

    # test w/ empty group
    def test_empty(self):
        empty = group.cuboid(name="empty", xmin=-100, xmax=-100, ymin=-100, ymax=-100, zmin=-100, zmax=-100)
        mode = integrate.mode_standard(dt=0.005);
        nve = integrate.langevin(group=empty, T=1.2, seed=1)
        run(1);

    # test adding types
    def test_add_type(self):
        all = group.all();
        bd = integrate.langevin(all, T=1.2, seed=1);
        bd.set_gamma('A', 0.5);
        bd.set_gamma('B', 1.0);
        run(100);

        self.s.particles.types.add('B')
        run(100);

    def tearDown(self):
        init.reset();


# validate langevin diffusion
class integrate_langevin_diffusion (unittest.TestCase):
    def setUp(self):
        print
        snap = data.make_snapshot(N=1000, box=data.boxdim(L=100000), particle_types=['A'])
        # this defaults all particles to position 0, which is what we want for this test
        self.s = init.read_snapshot(snap)

        sorter.set_params(grid=8)

    def test_gamma(self):
        # Setup an ideal gas with a gamma and T and validate the MSD
        T=1.8
        gamma=1;
        dt=0.01;
        steps=10000;

        integrate.mode_standard(dt=dt);
        bd = integrate.langevin(group.all(), T=T, seed=1, dscale=False);
        bd.set_gamma('A', gamma);

        run(steps);

        snap = self.s.take_snapshot();
        if comm.get_rank() == 0:
            msd = numpy.mean(snap.particles.position[:,0] * snap.particles.position[:,0] +
                             snap.particles.position[:,1] * snap.particles.position[:,1] +
                             snap.particles.position[:,2] * snap.particles.position[:,2])

            D = msd / (6*dt*steps);

            vsq = numpy.mean(snap.particles.velocity[:,0] * snap.particles.velocity[:,0] +
                             snap.particles.velocity[:,1] * snap.particles.velocity[:,1] +
                             snap.particles.velocity[:,2] * snap.particles.velocity[:,2])

            # check for a very crude overlap - we are not doing much averaging here to keep the test short
            self.assert_(almost_equal(D, T/gamma, 0.1))

            self.assert_(almost_equal(vsq, 3*T, 0.1))

    def test_dscale(self):
        # Setup an ideal gas with a gamma and T and validate the MSD
        T=1.8
        gamma=2;
        dt=0.01;
        steps=10000;

        integrate.mode_standard(dt=dt);
        bd = integrate.langevin(group.all(), T=T, seed=1, dscale=gamma);

        run(steps);

        snap = self.s.take_snapshot();
        if comm.get_rank() == 0:
            msd = numpy.mean(snap.particles.position[:,0] * snap.particles.position[:,0] +
                             snap.particles.position[:,1] * snap.particles.position[:,1] +
                             snap.particles.position[:,2] * snap.particles.position[:,2])

            vsq = numpy.mean(snap.particles.velocity[:,0] * snap.particles.velocity[:,0] +
                             snap.particles.velocity[:,1] * snap.particles.velocity[:,1] +
                             snap.particles.velocity[:,2] * snap.particles.velocity[:,2])

            D = msd / (6*dt*steps);

            # check for a very crude overlap - we are not doing much averaging here to keep the test short
            self.assert_(almost_equal(D, T/gamma, 0.1))

            self.assert_(almost_equal(vsq, 3*T, 0.1))
            print(vsq)

    def tearDown(self):
        init.reset();

if __name__ == '__main__':
    unittest.main(argv = ['test.py', '-v'])
