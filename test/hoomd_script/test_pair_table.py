# -*- coding: iso-8859-1 -*-
# Maintainer: joaander

from hoomd_script import *
import unittest
import os

# pair.table
class pair_table_tests (unittest.TestCase):
    def setUp(self):
        print
        self.s = init.create_random(N=100, phi_p=0.05);

        sorter.set_params(grid=8)

    # basic test of creation
    def test(self):
        table = pair.table(width=1000);
        table.pair_coeff.set('A', 'A', rmin=0.0, rmax=1.0, func=lambda r, rmin, rmax: (r, 2*r), coeff=dict());
        table.update_coeffs();

    # test missing coefficients
    def test_set_missing_epsilon(self):
        table = pair.table(width=1000);
        table.pair_coeff.set('A', 'A', rmin=0.0, rmax=1.0);
        self.assertRaises(RuntimeError, table.update_coeffs);

    # test missing coefficients
    def test_missing_AA(self):
        table = pair.table(width=1000);
        self.assertRaises(RuntimeError, table.update_coeffs);

    # test nlist global subscribe
    def test_nlist_global_subscribe(self):
        table = pair.table(width=1000);
        table.pair_coeff.set('A', 'A', rmin=0.0, rmax=1.0, func=lambda r, rmin, rmax: (r, 2*r), coeff=dict());
        table.update_coeffs();
        globals.neighbor_list.update_rcut();
        self.assertAlmostEqual(1.0, globals.neighbor_list.r_cut.get_pair('A','A'));

        table.pair_coeff.set('A', 'A', rmax = 2.5)
        globals.neighbor_list.update_rcut();
        self.assertAlmostEqual(2.5, globals.neighbor_list.r_cut.get_pair('A','A'));
    
    # test nlist subscribe
    def test_nlist_subscribe(self):
        nl = nlist.cell()
        table = pair.table(width=1000, nlist=nl);
        self.assertEqual(globals.neighbor_list, None)

        table.pair_coeff.set('A', 'A', rmin=0.0, rmax=1.0, func=lambda r, rmin, rmax: (r, 2*r), coeff=dict());
        table.update_coeffs();
        nl.update_rcut();
        self.assertAlmostEqual(1.0, nl.r_cut.get_pair('A','A'));

        table.pair_coeff.set('A', 'A', rmax = 2.5)
        nl.update_rcut();
        self.assertAlmostEqual(2.5, nl.r_cut.get_pair('A','A'));

    # test adding types
    def test_type_add(self):
        table = pair.table(width=1000);
        table.pair_coeff.set('A', 'A', rmin=0.0, rmax=1.0, func=lambda r, rmin, rmax: (r, 2*r), coeff=dict());
        self.s.particles.types.add('B')
        self.assertRaises(RuntimeError, table.update_coeffs);
        table.pair_coeff.set('A', 'B', rmin=0.0, rmax=1.0, func=lambda r, rmin, rmax: (r, 2*r), coeff=dict());
        table.pair_coeff.set('B', 'B', rmin=0.0, rmax=1.0, func=lambda r, rmin, rmax: (r, 2*r), coeff=dict());
        table.update_coeffs();

    def tearDown(self):
        del self.s
        init.reset();


if __name__ == '__main__':
    unittest.main(argv = ['test.py', '-v'])
