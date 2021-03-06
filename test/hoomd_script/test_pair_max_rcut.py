# -*- coding: iso-8859-1 -*-
# Maintainer: joaander

from hoomd_script import *
import unittest
import os

# pair - multiple type max_rcut test
class pair_max_rcut_tests (unittest.TestCase):
    def setUp(self):
        #print
        init.create_empty(N=100, box=data.boxdim(L=40), particle_types=['A', 'B']);

        sorter.set_params(grid=8)

    def test_max_rcut(self):
        lj = pair.lj(r_cut=2.5);
        lj.pair_coeff.set('A', 'A', simga=1.0, epsilon=1.0)
        lj.pair_coeff.set('A', 'B', simga=1.0, epsilon=1.0)
        lj.pair_coeff.set('B', 'B', simga=1.0, epsilon=1.0)
        self.assertAlmostEqual(2.5, lj.get_max_rcut());
        lj.pair_coeff.set('A', 'A', r_cut = 2.0)
        self.assertAlmostEqual(2.5, lj.get_max_rcut());
        lj.pair_coeff.set('A', 'B', r_cut = 3.0)
        self.assertAlmostEqual(3.0, lj.get_max_rcut());
        lj.pair_coeff.set('B', 'B', r_cut = 3.5)
        self.assertAlmostEqual(3.5, lj.get_max_rcut());

    def test_nlist_subscribe(self):
        lj = pair.lj(r_cut=2.5);
        lj.pair_coeff.set('A', 'A', simga=1.0, epsilon=1.0, r_cut=3.0)
        lj.pair_coeff.set('A', 'B', simga=1.0, epsilon=1.0)
        lj.pair_coeff.set('B', 'B', simga=1.0, epsilon=1.0, r_cut=3.1)

        # check that everything is initialized correctly
        globals.neighbor_list.update_rcut()
        self.assertAlmostEqual(3.0, globals.neighbor_list.r_cut.get_pair('A','A'));
        self.assertAlmostEqual(2.5, globals.neighbor_list.r_cut.get_pair('A','B'));
        self.assertAlmostEqual(3.1, globals.neighbor_list.r_cut.get_pair('B','B'));
        
        # update a pair coefficient, and check
        lj.pair_coeff.set('A','B', r_cut = 5.0)
        globals.neighbor_list.update_rcut()
        self.assertAlmostEqual(5.0, globals.neighbor_list.r_cut.get_pair('B','A'))

        # a second potential, only (B,B) should be bigger than the LJ
        gauss = pair.gauss(r_cut=1.0)
        gauss.pair_coeff.set('A', 'A', simga=1.0, epsilon=1.0, r_cut=1.0)
        gauss.pair_coeff.set('A', 'B', simga=1.0, epsilon=1.0, r_cut=2.0)
        gauss.pair_coeff.set('B', 'B', simga=1.0, epsilon=1.0, r_cut=5.1)

        globals.neighbor_list.update_rcut()
        self.assertAlmostEqual(3.0, globals.neighbor_list.r_cut.get_pair('A','A'));
        self.assertAlmostEqual(5.0, globals.neighbor_list.r_cut.get_pair('B','A'));
        self.assertAlmostEqual(5.1, globals.neighbor_list.r_cut.get_pair('B','B'));

        # change B,B back down, and make sure you get the LJ cutoff instead
        gauss.pair_coeff.set('B', 'B', simga=1.0, epsilon=1.0, r_cut=1.0)
        run(1)
        self.assertAlmostEqual(3.1, globals.neighbor_list.r_cut.get_pair('B','B'));
    
    # test independent subscription to two neighbor list
    def test_multi_nlist_subscribe(self):
        # subscribe to a tree neighbor list
        nl = nlist.cell()
        lj = pair.lj(r_cut=2.5, nlist=nl);
        lj.pair_coeff.set('A', 'A', simga=1.0, epsilon=1.0, r_cut=3.0)
        lj.pair_coeff.set('A', 'B', simga=1.0, epsilon=1.0)
        lj.pair_coeff.set('B', 'B', simga=1.0, epsilon=1.0, r_cut=3.1)

        # check that everything is initialized correctly
        nl.update_rcut()
        self.assertAlmostEqual(3.0, nl.r_cut.get_pair('A','A'));
        self.assertAlmostEqual(2.5, nl.r_cut.get_pair('A','B'));
        self.assertAlmostEqual(3.1, nl.r_cut.get_pair('B','B'));
        
        # update a pair coefficient, and check
        lj.pair_coeff.set('A','B', r_cut = 5.0)
        nl.update_rcut()
        self.assertAlmostEqual(5.0, nl.r_cut.get_pair('B','A'))

        # a second potential, only (B,B) should be bigger than the LJ
        # but, it's attached to the global neighbor list, so nothing should change
        gauss = pair.gauss(r_cut=1.0)
        gauss.pair_coeff.set('A', 'A', simga=1.0, epsilon=1.0, r_cut=1.0)
        gauss.pair_coeff.set('A', 'B', simga=1.0, epsilon=1.0, r_cut=2.0)
        gauss.pair_coeff.set('B', 'B', simga=1.0, epsilon=1.0, r_cut=5.1)

        nl.update_rcut()
        globals.neighbor_list.update_rcut()
        self.assertAlmostEqual(3.0, nl.r_cut.get_pair('A','A'));
        self.assertAlmostEqual(5.0, nl.r_cut.get_pair('A','B'));
        self.assertAlmostEqual(3.1, nl.r_cut.get_pair('B','B'));
        self.assertAlmostEqual(1.0, globals.neighbor_list.r_cut.get_pair('A','A'));
        self.assertAlmostEqual(2.0, globals.neighbor_list.r_cut.get_pair('B','A'));
        self.assertAlmostEqual(5.1, globals.neighbor_list.r_cut.get_pair('B','B'));
        
        # now, attach a third potential to the tree neighbor list, and things should change there
        slj = pair.slj(r_cut=1.0, nlist=nl, d_max=1.0)
        slj.pair_coeff.set('A', 'A', simga=1.0, epsilon=1.0, r_cut=1.0)
        slj.pair_coeff.set('A', 'B', simga=1.0, epsilon=1.0, r_cut=2.0)
        slj.pair_coeff.set('B', 'B', simga=1.0, epsilon=1.0, r_cut=5.1)

        nl.update_rcut()
        self.assertAlmostEqual(3.0, nl.r_cut.get_pair('A','A'));
        self.assertAlmostEqual(5.0, nl.r_cut.get_pair('B','A'));
        self.assertAlmostEqual(5.1, nl.r_cut.get_pair('B','B'));

        # change B,B back down, and make sure you get the LJ cutoff instead
        slj.pair_coeff.set('B', 'B', simga=1.0, epsilon=1.0, r_cut=1.0)
        run(1)
        self.assertAlmostEqual(3.1, nl.r_cut.get_pair('B','B'));

    def tearDown(self):
        init.reset();


if __name__ == '__main__':
    unittest.main(argv = ['test.py', '-v'])
