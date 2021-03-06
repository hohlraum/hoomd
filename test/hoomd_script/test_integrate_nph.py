# -*- coding: iso-8859-1 -*-
# Maintainer: joaander

from hoomd_script import *
import unittest
import os

# unit tests for integrate.nph
class integrate_nph_tests (unittest.TestCase):
    def setUp(self):
        print
        init.create_random(N=100, phi_p=0.05);
        force.constant(fx=0.1, fy=0.1, fz=0.1)

        sorter.set_params(grid=8)

    # tests basic creation of the integrator
    def test(self):
        all = group.all();
        integrate.mode_standard(dt=0.005);
        integrate.nph(group=all, tau=0.5, P=1.0, tauP=0.5);
        run(100);

    def test_mtk_cubic(self):
        all = group.all();
        integrate.mode_standard(dt=0.005);
        integrate.nph(group=all, tau=0.5, P=1.0, tauP=0.5);
        run(100);

    def test_mtk_orthorhombic(self):
        all = group.all();
        integrate.mode_standard(dt=0.005);
        integrate.nph(group=all, tau=0.5, P=1.0, tauP=0.5, couple="none");
        run(100);

    def test_mtk_tetragonal(self):
        all = group.all();
        integrate.mode_standard(dt=0.005);
        integrate.nph(group=all, tau=0.5, P=1.0, tauP=0.5, couple="xy");
        run(100);

    def test_mtk_triclinic(self):
        all = group.all();
        integrate.mode_standard(dt=0.005);
        integrate.nph(group=all, tau=0.5, P=1.0, tauP=0.5, couple="none", all=True);
        run(100);

    # test set_params
    def test_set_params(self):
        integrate.mode_standard(dt=0.005);
        all = group.all();
        nph = integrate.nph(group=all, tau=0.5, P=1.0, tauP=0.5);
        nph.set_params(T=1.3);
        nph.set_params(tau=0.6);
        nph.set_params(P=0.5);
        nph.set_params(tauP=0.6);
        run(100);

    # test w/ empty group
    def test_empty_mtk(self):
        empty = group.cuboid(name="empty", xmin=-100, xmax=-100, ymin=-100, ymax=-100, zmin=-100, zmax=-100)
        mode = integrate.mode_standard(dt=0.005);
        with self.assertRaises(RuntimeError):
            nph = integrate.nph(group=empty, P=1.0, tau=0.5, tauP=0.5)
            run(1);

    def tearDown(self):
        init.reset();


if __name__ == '__main__':
    unittest.main(argv = ['test.py', '-v'])
