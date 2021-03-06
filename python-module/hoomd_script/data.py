# -- start license --
# Highly Optimized Object-oriented Many-particle Dynamics -- Blue Edition
# (HOOMD-blue) Open Source Software License Copyright 2009-2015 The Regents of
# the University of Michigan All rights reserved.

# HOOMD-blue may contain modifications ("Contributions") provided, and to which
# copyright is held, by various Contributors who have granted The Regents of the
# University of Michigan the right to modify and/or distribute such Contributions.

# You may redistribute, use, and create derivate works of HOOMD-blue, in source
# and binary forms, provided you abide by the following conditions:

# * Redistributions of source code must retain the above copyright notice, this
# list of conditions, and the following disclaimer both in the code and
# prominently in any materials provided with the distribution.

# * Redistributions in binary form must reproduce the above copyright notice, this
# list of conditions, and the following disclaimer in the documentation and/or
# other materials provided with the distribution.

# * All publications and presentations based on HOOMD-blue, including any reports
# or published results obtained, in whole or in part, with HOOMD-blue, will
# acknowledge its use according to the terms posted at the time of submission on:
# http://codeblue.umich.edu/hoomd-blue/citations.html

# * Any electronic documents citing HOOMD-Blue will link to the HOOMD-Blue website:
# http://codeblue.umich.edu/hoomd-blue/

# * Apart from the above required attributions, neither the name of the copyright
# holder nor the names of HOOMD-blue's contributors may be used to endorse or
# promote products derived from this software without specific prior written
# permission.

# Disclaimer

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND/OR ANY
# WARRANTIES THAT THIS SOFTWARE IS FREE OF INFRINGEMENT ARE DISCLAIMED.

# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# -- end license --

# Maintainer: joaander

import hoomd
from hoomd_script import globals
from hoomd_script import util
from hoomd_script import meta
import hoomd_script

## \package hoomd_script.data
# \brief Access particles, bonds, and other state information inside scripts
#
# Code in the data package provides high-level access to all of the particle, bond and other %data that define the
# current state of the system. You can use python code to directly read and modify this data, allowing you to analyze
# simulation results while the simulation runs, or to create custom initial configurations with python code.
#
# There are two ways to access the data.
#
# 1. Snapshots record the system configuration at one instant in time. You can store this state to analyze the data,
#    restore it at a future point in time, or to modify it and reload it. Use snapshots for initializing simulations,
#    or when you need to access or modify the entire simulation state.
# 2. Data proxies directly access the current simulation state. Use data proxies if you need to only touch a few
#    particles or bonds at a a time.
#
# \section data_snapshot Snapshots
# <hr>
#
# <h3>Relevant methods:</h3>
#
# * system_data.take_snapshot() captures a snapshot of the current system state. A snapshot is a copy of the simulation
# state. As the simulation continues to progress, data in a captured snapshot will remain constant.
# * system_data.restore_snapshot() replaces the current system state with the state stored in a snapshot.
# * data.make_snapshot() creates an empty snapshot that you can populate with custom data.
# * init.read_snapshot() initializes a simulation from a snapshot.
#
# \code
# snapshot = system.take_snapshot()
# system.restore_snapshot(snapshot)
# snapshot = data.make_snapshot(N=100, particle_types=['A', 'B'], box=data.boxdim(L=10))
# # ... populate snapshot with data ...
# init.read_snapshot(snapshot)
# \endcode
#
# <hr>
# <h3>Snapshot and MPI</h3>
# In MPI simulations, the snapshot is only valid on rank 0. make_snapshot, read_snapshot, and take_snapshot, restore_snapshot are
# collective calls, and need to be called on all ranks. But only rank 0 can access data in the snapshot.
# \code
# snapshot = system.take_snapshot(all=True)
# if comm.get_rank() == 0:
#     print(numpy.mean(snapshot.particles.velocity))
#     snapshot.particles.position[0] = [1,2,3];
#
# system.restore_snapshot(snapshot);
# snapshot = data.make_snapshot(N=10, box=data.boxdim(L=10))
# if comm.get_rank() == 0:
#     snapshot.particles.position[:] = ....
# init.read_snapshot(snapshot)
# \endcode
#
# <hr>
# <h3>Simulation box</h3>
# You can access the simulation box from a snapshot:
# \code
# >>> print(snapshot.box)
# Box: Lx=17.3646569289 Ly=17.3646569289 Lz=17.3646569289 xy=0.0 xz=0.0 yz=0.0 dimensions=3
# \endcode
# and can change it:
# \code
# >>> snapsot.box = data.boxdim(Lx=10, Ly=20, Lz=30, xy=1.0, xz=0.1, yz=2.0)
# >>> print(snapshot.box)
# Box: Lx=10 Ly=20 Lz=30 xy=1.0 xz=0.1 yz=2.0 dimensions=3
# \endcode
# \b All particles must be inside the box before using the snapshot to initialize a simulation or restoring it.
# The dimensionality of the system (2D/3D) cannot change after initialization.
#
# <h3>Particle properties</h3>
#
# Particle properties are present in `snapshot.particles`. Each property is stored in a numpy array that directly
# accesses the memory of the snapshot. References to these arrays will become invalid when the snapshot itself is
# garbage collected.
#
# - `N` is the number of particles in the particle data snapshot
# \code
# >>> print(snapshot.particles.N)
# 64000
# \endcode
# - Change the number of particles in the snapshot with resize. Existing particle properties are
#   preserved after the resize. Any newly created particles will have default values. After resizing,
#   existing references to the numpy arrays will be invalid, access them again
#   from `snapshot.particles.*`
# \code
# >>> snapshot.particles.resize(1000);
# \endcode
# - The list of all particle types in the simulation can be accessed and modified
# \code
# >>> print(snapshot.particles.types)
# ['A', 'B', 'C']
# >>> snapshot.particles.types = ['1', '2', '3', '4'];
# \endcode
# - Individual particles properties are stored in numpy arrays. Vector quantities are stored in Nx3 arrays of floats
#   (or doubles) and scalar quantities are stored in N length 1D arrays.
# \code
# >>> print(snapshot.particles.position[10])
# [ 1.2398  -10.2687  100.6324]
# \endcode
# - Various properties can be accessed of any particle, and the numpy arrays can be sliced or passed whole to other
#   routines.
# \code
# >>> print(snapshot.particles.typeid[10])
# 2
# >>> print(snapshot.particles.velocity[10])
# (-0.60267972946166992, 2.6205904483795166, -1.7868227958679199)
# >>> print(snapshot.particles.mass[10])
# 1.0
# >>> print(snapshot.particles.diameter[10])
# 1.0
# \endcode
# - Particle properties can be set in the same way. This modifies the data in the snapshot, not the
#   current simulation state.
# \code
# >>> snapshot.particles.position[10] = [1,2,3]
# >>> print(snapshot.particles.position[10])
# [ 1.  2.  3.]
# \endcode
# - Snapshots store particle types as integers that index into the type name array:
# \code
# >>> print(snapshot.particles.typeid)
# [ 0.  1.  2.  0.  1.  2.  0.  1.  2.  0.]
# >>> snapshot.particles.types = ['A', 'B', 'C'];
# >>> snapshot.particles.typeid[0] = 2;   # C
# >>> snapshot.particles.typeid[1] = 0;   # A
# >>> snapshot.particles.typeid[2] = 1;   # B
# \endcode
#
# For a list of all particle properties in the snapshot see SnapshotParticleData.
#
# <h3>Bonds</h3>
#
# Bonds are stored in `snapshot.bonds`. system_data.take_snapshot() does not record the bonds by default, you need to
# request them with the argument `bonds=True`.
#
# - `N` is the number of bonds in the bond data snapshot
# \code
# >>> print(snapshot.bonds.N)
# 100
# \endcode
# - Change the number of bonds in the snapshot with resize. Existing bonds are
#   preserved after the resize. Any newly created bonds will be initialized to 0. After resizing,
#   existing references to the numpy arrays will be invalid, access them again
#   from `snapshot.bonds.*`
# \code
# >>> snapshot.bonds.resize(1000);
# \endcode
# - Bonds are stored in an Nx2 numpy array `group`. The first axis accesses the bond `i`. The second axis `j` goes over
#   the individual particles in the bond. The value of each element is the tag of the particle participating in the
#   bond.
# \code
# >>> print(snapshot.bonds.group)
# [[0 1]
# [1 2]
# [3 4]
# [4 5]]
# >>> snapshot.bonds.group[0] = [10,11]
# \endcode
# - Snapshots store bond types as integers that index into the type name array:
# \code
# >>> print(snapshot.bonds.typeid)
# [ 0.  1.  2.  0.  1.  2.  0.  1.  2.  0.]
# >>> snapshot.bonds.types = ['A', 'B', 'C'];
# >>> snapshot.bonds.typeid[0] = 2;   # C
# >>> snapshot.bonds.typeid[1] = 0;   # A
# >>> snapshot.bonds.typeid[2] = 1;   # B
# \endcode
#
# <h3>Angles, dihedrals and impropers</h3>
#
# Angles, dihedrals, and impropers are stored similar to bonds. The only difference is that the group array is sized
# appropriately to store the number needed for each type of bond.
#
# * `snapshot.angles.group` is Nx3
# * `snapshot.dihedrals.group` is Nx4
# * `snapshot.impropers.group` is Nx4
#
# \section data_proxy Proxy access
#
# For most of the cases below, it is assumed that the result of the initialization command was saved at the beginning
# of the script, like so:
# \code
# system = init.read_xml(filename="input.xml")
# \endcode
#
# <hr>
# <h3>Simulation box</h3>
# You can access the simulation box like so:
# \code
# >>> print(system.box)
# Box: Lx=17.3646569289 Ly=17.3646569289 Lz=17.3646569289 xy=0.0 xz=0.0 yz=0.0
# \endcode
# and can change it like so:
# \code
# >>> system.box = data.boxdim(Lx=10, Ly=20, Lz=30, xy=1.0, xz=0.1, yz=2.0)
# >>> print(system.box)
# Box: Lx=10 Ly=20 Lz=30 xy=1.0 xz=0.1 yz=2.0
# \endcode
# \b All particles must \b always remain inside the box. If a box is set in this way such that a particle ends up outside of the box, expect
# errors to be thrown or for hoomd to just crash. The dimensionality of the system cannot change after initialization.
# <hr>
# <h3>Particle properties</h3>
# For a list of all particle properties that can be read and/or set, see the particle_data_proxy. The examples
# here only demonstrate changing a few of them.
#
# With the result of an init command saved in the variable \c system (see above), \c system.particles is a window
# into all of the particles in the system. It behaves like standard python list in many ways.
# - Its length (the number of particles in the system) can be queried
# \code
# >>> len(system.particles)
# 64000
# \endcode
# - A short summary can be printed of the list
# \code
# >>> print(system.particles)
# Particle Data for 64000 particles of 1 type(s)
# \endcode
# - The list of all particle types in the simulation can be accessed
# \code
# >>> print(system.particles.types)
# ['A']
# >>> print system.particles.types
# Particle types: ['A']
# \endcode
# - Particle types can be added between subsequent run() commands:
# \code
# >>> system.particles.types.add('newType')
# \endcode
# - Individual particles can be accessed at random.
# \code
# >>> i = 4
# >>> p = system.particles[i]
# \endcode
# - Various properties can be accessed of any particle
# \code
# >>> p.tag
# 4
# >>> p.position
# (27.296911239624023, -3.5986068248748779, 10.364067077636719)
# >>> p.velocity
# (-0.60267972946166992, 2.6205904483795166, -1.7868227958679199)
# >>> p.mass
# 1.0
# >>> p.diameter
# 1.0
# >>> p.type
# 'A'
# >>> p.tag
# 4
# \endcode
# (note that p can be replaced with system.particles[i] above and the results are the same)
# - Particle properties can be set in the same way:
# \code
# >>> p.position = (1,2,3)
# >>> p.position
# (1.0, 2.0, 3.0)
# \endcode
# - Finally, all particles can be easily looped over
# \code
# for p in system.particles:
#     p.velocity = (0,0,0)
# \endcode
#
# Performance is decent, but not great. The for loop above that sets all velocities to 0 takes 0.86 seconds to execute
# on a 2.93 GHz core2 iMac. The interface has been designed to be flexible and easy to use for the widest variety of
# initialization tasks, not efficiency.
# For doing modifications that operate on the whole system data efficiently, snapshots can be used.
# Their usage is described below.
#
# Particles may be added at any time in the job script, and a unique tag is returned.
# \code
# >>> system.particles.add('A')
# >>> t = system.particles.add('B')
# \endcode
#
# Particles may be deleted by index.
# \code
# >>> del system.particles[0]
# >>> print(system.particles[0])
# tag         : 1
# position    : (23.846603393554688, -27.558368682861328, -20.501256942749023)
# image       : (0, 0, 0)
# velocity    : (0.0, 0.0, 0.0)
# acceleration: (0.0, 0.0, 0.0)
# charge      : 0.0
# mass        : 1.0
# diameter    : 1.0
# type        : A
# typeid      : 0
# body        : 4294967295
# orientation : (1.0, 0.0, 0.0, 0.0)
# net_force   : (0.0, 0.0, 0.0)
# net_energy  : 0.0
# net_torque  : (0.0, 0.0, 0.0)
# \endcode
# \note The particle with tag 1 is now at index 0. No guarantee is made about how the
# order of particles by index will or will not change, so do not write any job scripts which assume a given ordering.
#
# To access particles in an index-independent manner, use their tags. For example, to remove all particles
# of type 'A', do
# \code
# tags = []
# for p in system.particles:
#     if p.type == 'A'
#         tags.append(p.tag)
# \endcode
# Then remove each of the bonds by their unique tag.
# \code
# for t in tags:
#     system.particles.remove(t)
# \endcode
# Particles can also be accessed through their unique tag:
# \code
# t = system.particles.add('A')
# p = system.particles.get(t)
# \endcode
#
# There is a second way to access the particle data. Any defined group can be used in exactly the same way as
# \c system.particles above, only the particles accessed will be those just belonging to the group. For a specific
# example, the following will set the velocity of all particles of type A to 0.
# \code
# groupA = group.type(name="a-particles", type='A')
# for p in groupA:
#     p.velocity = (0,0,0)
# \endcode
#
# <hr>
# <h3>Rigid Body Data</h3>
# Rigid Body data can be accessed via the body_data_proxy.  Here are examples
#
#\code
#
# >>> b = system.bodies[0]
# >>> print(b)
#num_particles    : 5
#mass             : 5.0
# COM              : (0.33264800906181335, -2.495814800262451, -1.2669427394866943)
# velocity         : (0.0, 0.0, 0.0)
# orientation      : (0.9244732856750488, -0.3788720965385437, -0.029276784509420395, 0.0307924821972847)
# angular_momentum (space frame) : (0.0, 0.0, 0.0)
# moment_inertia: (10.000000953674316, 10.0, 0.0)
# particle_tags    : [0, 1, 2, 3, 4]
# particle_disp    : [[-3.725290298461914e-09, -4.172325134277344e-07, 2.0], [-2.421438694000244e-08, -2.086162567138672e-07, 0.9999998211860657], [-2.6206091519043184e-08, -2.073889504572435e-09, -3.361484459674102e-07], [-5.029141902923584e-08, 2.682209014892578e-07, -1.0000004768371582], [-3.3527612686157227e-08, -2.980232238769531e-07, -2.0]]
# >>> print(b.COM)
# (0.33264800906181335, -2.495814800262451, -1.2669427394866943)
# >>> b.particle_disp = [[0,0,0], [0,0,0], [0,0,0.0], [0,0,0], [0,0,0]]
#
#\endcode
#
# <hr>
# <h3>Bond Data</h3>
# Bonds may be added at any time in the job script.
# \code
# >>> system.bonds.add("bondA", 0, 1)
# >>> system.bonds.add("bondA", 1, 2)
# >>> system.bonds.add("bondA", 2, 3)
# >>> system.bonds.add("bondA", 3, 4)
# \endcode
#
# Individual bonds may be accessed by index.
# \code
# >>> bnd = system.bonds[0]
# >>> print(bnd)
# tag          : 0
# typeid       : 0
# a            : 0
# b            : 1
# type         : bondA
# >>> print(bnd.type)
# bondA
# >>> print(bnd.a)
# 0
# >>> print(bnd.b)
#1
# \endcode
# \note The order in which bonds appear by index is not static and may change at any time!
#
# Bonds may be deleted by index.
# \code
# >>> del system.bonds[0]
# >>> print(system.bonds[0])
# tag          : 3
# typeid       : 0
# a            : 3
# b            : 4
# type         : bondA
# \endcode
# \note Regarding the previous note: see how the last bond added is now at index 0. No guarantee is made about how the
# order of bonds by index will or will not change, so do not write any job scripts which assume a given ordering.
#
# To access bonds in an index-independent manner, use their tags. For example, to delete all bonds which connect to
# particle 2, first loop through the bonds and build a list of bond tags that match the criteria.
# \code
# tags = []
# for b in system.bonds:
#     if b.a == 2 or b.b == 2:
#         tags.append(b.tag)
# \endcode
# Then remove each of the bonds by their unique tag.
# \code
# for t in tags:
#     system.bonds.remove(t)
# \endcode
# Bonds can also be accessed through their unique tag:
# \code
# t = system.bonds.add('polymer',0,1)
# p = system.bonds.get(t)
# \endcode
#
# <hr>
# <h3>Angle, Dihedral, and Improper Data</h3>
# Angles, Dihedrals, and Impropers may be added at any time in the job script.
# \code
# >>> system.angles.add("angleA", 0, 1, 2)
# >>> system.dihedrals.add("dihedralA", 1, 2, 3, 4)
# >>> system.impropers.add("dihedralA", 2, 3, 4, 5)
# \endcode
#
# Individual angles, dihedrals, and impropers may be accessed, deleted by index or removed by tag with the same syntax
# as described for bonds, just replace \em bonds with \em angles, \em dihedrals, or, \em impropers and access the
# appropriate number of tag elements (a,b,c for angles) (a,b,c,d for dihedrals/impropers).
# <hr>
#
# <h3>Forces</h3>
# Forces can be accessed in a similar way.
# \code
# >>> lj = pair.lj(r_cut=3.0)
# >>> lj.pair_coeff.set('A', 'A', epsilon=1.0, sigma=1.0)
# >>> print(lj.forces[0])
# tag         : 0
# force       : (-0.077489577233791351, -0.029512746259570122, -0.13215918838977814)
# virial      : -0.0931386947632
# energy      : -0.0469368174672
# >>> f0 = lj.forces[0]
# >>> print(f0.force)
# (-0.077489577233791351, -0.029512746259570122, -0.13215918838977814)
# >>> print(f0.virial)
# -0.093138694763n
# >>> print(f0.energy)
# -0.0469368174672
# \endcode
#
# In this manner, forces due to the lj %pair %force, bonds, and any other %force commands in hoomd can be accessed
# independently from one another. See force_data_proxy for a definition of each parameter accessed.
#
# <hr>
# <h3>Proxy references</h3>
#
# For advanced code using the particle data access from python, it is important to understand that the hoomd_script
# particles, forces, bonds, et cetera, are accessed as proxies. This means that after
# \code
# p = system.particles[i]
# \endcode
# is executed, \a p \b does \b not store the position, velocity, ... of particle \a i. Instead, it just stores \a i and
# provides an interface to get/set the properties on demand. This has some side effects you need to be aware of.
# - First, it means that \a p (or any other proxy reference) always references the current state of the particle.
# As an example, note how the position of particle p moves after the run() command.
# \code
# >>> p.position
# (-21.317455291748047, -23.883811950683594, -22.159387588500977)
# >>> run(1000)
# ** starting run **
# ** run complete **
# >>> p.position
# (-19.774742126464844, -23.564577102661133, -21.418502807617188)
# \endcode
# - Second, it means that copies of the proxy reference cannot be changed independently.
# \code
# p.position
# >>> a = p
# >>> a.position
# (-19.774742126464844, -23.564577102661133, -21.418502807617188)
# >>> p.position = (0,0,0)
# >>> a.position
# (0.0, 0.0, 0.0)
# \endcode
#
# If you need to store some particle properties at one time in the simulation and access them again later, you will need
# to make copies of the actual property values themselves and not of the proxy references.
#

## Define box dimensions
#
# Simulation boxes in hoomd are specified by six parameters, *Lx*, *Ly*, *Lz*, *xy*, *xz* and *yz*. For full details,
# see \ref page_box. A boxdim provides a way to specify all six parameters for a given box and perform some common
# operations with them. Modifying a boxdim does not modify the underlying simulation box in hoomd. A boxdim can be passed
# to an initialization method or to assigned to a saved sysdef variable (`system.box = new_box`) to set the simulation
# box.
#
# boxdim parameters may be accessed directly.
# ~~~~
# b = data.boxdim(L=20);
# b.xy = 1.0;
# b.yz = 0.5;
# b.Lz = 40;
# ~~~~
#
# **Two dimensional systems**
#
# 2D simulations in hoomd are embedded in 3D boxes with short heights in the z direction. To create a 2D box,
# set dimensions=2 when creating the boxdim. This will force Lz=1 and xz=yz=0. init commands that support 2D boxes
# will pass the dimensionality along to the system. When you assign a new boxdim to an already initialized system,
# the dimensionality flag is ignored. Changing the number of dimensions during a simulation run is not supported.
#
# In 2D boxes, *volume* is in units of area.
#
# **Shorthand notation**
#
# data.boxdim accepts the keyword argument *L=x* as shorthand notation for `Lx=x, Ly=x, Lz=x` in 3D
# and `Lx=x, Ly=z, Lz=1` in 2D. If you specify both `L=` and `Lx,Ly, or Lz`, then the value for `L` will override
# the others.
#
# **Examples:**
#
# There are many ways to define boxes.
#
# * Cubic box with given volume: `data.boxdim(volume=V)`
# * Triclinic box in 2D with given area: `data.boxdim(xy=1.0, dimensions=2, volume=A)`
# * Rectangular box in 2D with given area and aspect ratio: `data.boxdim(Lx=1, Ly=aspect, dimensions=2, volume=A)`
# * Cubic box with given length: `data.boxdim(L=10)`
# * Fully define all box parameters: `data.boxdim(Lx=10, Ly=20, Lz=30, xy=1.0, xz=0.5, yz=0.1)`
#
# system = init.read_xml('init.xml')
# system.box = system.box.scale(s=2)
# ~~~
class boxdim(meta._metadata):
    ## Initialize a boxdim object
    #
    # \param Lx box extent in the x direction (distance units)
    # \param Ly box extent in the y direction (distance units)
    # \param Lz box extent in the z direction (distance units)
    # \param xy tilt factor xy (dimensionless)
    # \param xz tilt factor xz (dimensionless)
    # \param yz tilt factor yz (dimensionless)
    # \param dimensions Number of dimensions in the box (2 or 3).
    # \param L shorthand for specifying Lx=Ly=Lz=L (distance units)
    # \param volume Scale the given box dimensions up to the this volume (area if dimensions=2)
    #
    def __init__(self, Lx=1.0, Ly=1.0, Lz=1.0, xy=0.0, xz=0.0, yz=0.0, dimensions=3, L=None, volume=None):
        if L is not None:
            Lx = L;
            Ly = L;
            Lz = L;

        if dimensions == 2:
            Lz = 1.0;
            xz = yz = 0.0;

        self.Lx = Lx;
        self.Ly = Ly;
        self.Lz = Lz;
        self.xy = xy;
        self.xz = xz;
        self.yz = yz;
        self.dimensions = dimensions;

        if volume is not None:
            self.set_volume(volume);

        # base class constructor
        meta._metadata.__init__(self)

    ## Scale box dimensions
    #
    # \param sx scale factor in the x direction
    # \param sy scale factor in the y direction
    # \param sz scale factor in the z direction
    #
    # Scales the box by the given scale factors. Tilt factors are not modified.
    #
    # \returns a reference to itself
    def scale(self, sx=1.0, sy=1.0, sz=1.0, s=None):
        if s is not None:
            sx = s;
            sy = s;
            sz = s;

        self.Lx = self.Lx * sx;
        self.Ly = self.Ly * sy;
        self.Lz = self.Lz * sz;
        return self

    ## Set the box volume
    #
    # \param volume new box volume (area if dimensions=2)
    #
    # setVolume() scales the box to the given volume (or area).
    #
    # \returns a reference to itself
    def set_volume(self, volume):
        cur_vol = self.get_volume();

        if self.dimensions == 3:
            s = (volume / cur_vol)**(1.0/3.0)
            self.scale(s, s, s);
        else:
            s = (volume / cur_vol)**(1.0/2.0)
            self.scale(s, s, 1.0);
        return self

    ## Get the box volume
    #
    # Returns the box volume (area in 2D).
    #
    def get_volume(self):
        b = self._getBoxDim();
        return b.getVolume(self.dimensions == 2);

    ## Get a lattice vector
    #
    # \param i (=0,1,2) direction of lattice vector
    #
    # \returns a lattice vector (3-tuple) along direction \a i
    #
    def get_lattice_vector(self,i):
        b = self._getBoxDim();
        v = b.getLatticeVector(int(i))
        return (v.x, v.y, v.z)

    ## Wrap a vector using the periodic boundary conditions
    #
    # \param v The vector to wrap
    # \param img A vector of integer image flags that will be updated (optional)
    #
    # \returns the wrapped vector and the image flags
    #
    def wrap(self,v, img=(0,0,0)):
        u = hoomd.make_scalar3(v[0],v[1],v[2])
        i = hoomd.make_int3(int(img[0]),int(img[1]),int(img[2]))
        c = hoomd.make_char3(0,0,0)
        self._getBoxDim().wrap(u,i,c)
        img = (i.x,i.y,i.z)
        return (u.x, u.y, u.z),img

    ## Apply the minimum image convention to a vector using periodic boundary conditions
    #
    # \param v The vector to apply minimum image to
    #
    # \returns the minimum image
    #
    def min_image(self,v):
        u = hoomd.make_scalar3(v[0],v[1],v[2])
        u = self._getBoxDim().minImage(u)
        return (u.x, u.y, u.z)

    ## Scale a vector to fractional coordinates
    #
    # \param v The vector to convert to fractional coordinates
    #
    # make_fraction() takes a vector in a box and computes a vector where all components are
    # between 0 and 1.
    #
    # \returns the scaled vector
    def make_fraction(self,v):
        u = hoomd.make_scalar3(v[0],v[1],v[2])
        w = hoomd.make_scalar3(0,0,0)

        u = self._getBoxDim().makeFraction(u,w)
        return (u.x, u.y, u.z)

    ## \internal
    # \brief Get a C++ boxdim
    def _getBoxDim(self):
        b = hoomd.BoxDim(self.Lx, self.Ly, self.Lz);
        b.setTiltFactors(self.xy, self.xz, self.yz);
        return b

    def __str__(self):
        return 'Box: Lx=' + str(self.Lx) + ' Ly=' + str(self.Ly) + ' Lz=' + str(self.Lz) + ' xy=' + str(self.xy) + \
                    ' xz='+ str(self.xz) + ' yz=' + str(self.yz) + ' dimensions=' + str(self.dimensions);

    ## \internal
    # \brief Get a dictionary representation of the box dimensions
    def get_metadata(self):
        data = meta._metadata.get_metadata(self)
        data['d'] = self.dimensions
        data['Lx'] = self.Lx
        data['Ly'] = self.Ly
        data['Lz'] = self.Lz
        data['xy'] = self.xy
        data['xz'] = self.xz
        data['yz'] = self.yz
        data['V'] = self.get_volume()
        return data

##
# \brief Access system data
#
# system_data provides access to the different data structures that define the current state of the simulation.
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# system_data, documented by example.
#
class system_data(meta._metadata):
    ## \internal
    # \brief create a system_data
    #
    # \param sysdef SystemDefinition to connect
    def __init__(self, sysdef):
        self.sysdef = sysdef;
        self.particles = particle_data(sysdef.getParticleData());
        self.bonds = bond_data(sysdef.getBondData());
        self.angles = angle_data(sysdef.getAngleData());
        self.dihedrals = dihedral_data(sysdef.getDihedralData());
        self.impropers = dihedral_data(sysdef.getImproperData());
        self.bodies = body_data(sysdef.getRigidData());

        # base class constructor
        meta._metadata.__init__(self)

    ## Take a snapshot of the current system data
    #
    # This functions returns a snapshot object. It contains the current
    # partial or complete simulation state. With appropriate options
    # it is possible to select which data properties should be included
    # in the snapshot.
    #
    # \param particles If true, particle data is included in the snapshot
    # \param bonds If true, bond, angle, dihedral, and improper data is included in the snapshot
    # \param rigid_bodies If true, rigid body data is included in the snapshot
    # \param integrators If true, integrator data is included the snapshot
    # \param all If true, the entire system state is saved in the snapshot
    # \param dtype Datatype for the snapshot numpy arrays. Must be either 'float' or 'double'.
    #
    # \returns the snapshot object.
    #
    # \code
    # snapshot = system.take_snapshot()
    # snapshot = system.take_snapshot()
    # snapshot = system.take_snapshot(bonds=true)
    # \endcode
    #
    # \MPI_SUPPORTED
    def take_snapshot(self,
                      particles=True,
                      bonds=False,
                      rigid_bodies=False,
                      integrators=False,
                      all=False,
                      dtype='float'):
        util.print_status_line();

        if all is True:
                particles=True
                bonds=True
                rigid_bodies=True
                integrators=True

        if not (particles or bonds or angles or dihedrals or impropers or rigid_bodies or integrators):
            globals.msg.warning("No options specified. Ignoring request to create an empty snapshot.\n")
            return None

        # take the snapshot
        if dtype == 'float':
            cpp_snapshot = self.sysdef.takeSnapshot_float(particles,bonds,bonds,bonds,bonds,rigid_bodies,integrators)
        elif dtype == 'double':
            cpp_snapshot = self.sysdef.takeSnapshot_double(particles,bonds,bonds,bonds,bonds,rigid_bodies,integrators)
        else:
            raise ValueError("dtype must be float or double");

        return cpp_snapshot

    ## Replicates the system along the three spatial dimensions
    #
    # \param nx Number of times to replicate the system along the x-direction
    # \param ny Number of times to replicate the system along the y-direction
    # \param nz Number of times to replicate the system along the z-direction
    #
    # This method explictly replicates particles along all three spatial directions, as
    # opposed to replication implied by periodic boundary conditions.
    # The box is resized and the number of particles is updated so that the new box
    # holds the specified number of replicas of the old box along all directions.
    # Particle coordinates are updated accordingly to fit into the new box. All velocities and
    # other particle properties are replicated as well. Also bonded groups between particles
    # are replicated.
    #
    # Example usage:
    # \code
    # system = init.read_xml("some_file.xml")
    # system.replicate(nx=2,ny=2,nz=2)
    # \endcode
    #
    # \note Replication of rigid bodies is currently not supported.
    #
    # \note It is a limitation that in MPI simulations the dimensions of the processor grid
    # are not updated upon replication. For example, if an initially cubic box is replicated along only one
    # spatial direction, this could lead to decreased performance if the processor grid was
    # optimal for the original box dimensions, but not for the new ones.
    #
    # \MPI_SUPPORTED
    def replicate(self, nx=1, ny=1, nz=1):
        util.print_status_line()

        nx = int(nx)
        ny = int(ny)
        nz = int(nz)

        if nx == ny == nz == 1:
            globals.msg.warning("All replication factors == 1. Not replicating system.\n")
            return

        if nx <= 0 or ny <= 0 or nz <= 0:
            globals.msg.error("Cannot replicate by zero or by a negative value along any direction.")
            raise RuntimeError("nx, ny, nz need to be positive integers")

        # Take a snapshot
        util._disable_status_lines = True
        cpp_snapshot = self.take_snapshot(all=True)
        util._disable_status_lines = False

        if hoomd_script.comm.get_rank() == 0:
            # replicate
            cpp_snapshot.replicate(nx, ny, nz)

        # restore from snapshot
        util._disable_status_lines = True
        self.restore_snapshot(cpp_snapshot)
        util._disable_status_lines = False

    ## Re-initializes the system from a snapshot
    #
    # \param snapshot The snapshot to initialize the system from
    #
    # Snapshots temporarily store system %data. Snapshots contain the complete simulation state in a
    # single object. They can be used to restart a simulation.
    #
    # Example use cases in which a simulation may be restarted from a snapshot include python-script-level
    # \b Monte-Carlo schemes, where the system state is stored after a move has been accepted (according to
    # some criterium), and where the system is re-initialized from that same state in the case
    # when a move is not accepted.
    #
    # Example for the procedure of taking a snapshot and re-initializing from it:
    # \code
    # system = init.read_xml("some_file.xml")
    #
    # ... run a simulation ...
    #
    # snapshot = system.take_snapshot(all=True)
    # ...
    # system.restore_snapshot(snapshot)
    # \endcode
    #
    # \warning restore_snapshot() may invalidate force coefficients, neighborlist r_cut values, and other per type quantities if called within a callback
    #          during a run(). You can restore a snapshot during a run only if the snapshot is of a previous state of the currently running system.
    #          Otherwise, you need to use restore_snapshot() between run() commands to ensure that all per type coefficients are updated properly.
    #
    # \sa hoomd_script.data
    # \MPI_SUPPORTED
    def restore_snapshot(self, snapshot):
        util.print_status_line();

        self.sysdef.initializeFromSnapshot(snapshot);

    ## \var sysdef
    # \internal
    # \brief SystemDefinition to which this instance is connected

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __setattr__(self, name, value):
        if name == "box":
            if not isinstance(value, boxdim):
                raise TypeError('box must be a data.boxdim object');
            self.sysdef.getParticleData().setGlobalBox(value._getBoxDim());

        # otherwise, consider this an internal attribute to be set in the normal way
        self.__dict__[name] = value;

    ## \internal
    # \brief Get particle metadata
    def get_metadata(self):
        data = meta._metadata.get_metadata(self)
        data['box'] = self.box
        data['particles'] = self.particles
        data['number_density'] = len(self.particles)/self.box.get_volume()

        data['bonds'] = self.bonds
        data['angles'] = self.angles
        data['dihedrals'] = self.dihedrals
        data['impropers'] = self.impropers
        data['bodies'] = self.bodies

        data['timestep'] = globals.system.getCurrentTimeStep()
        return data

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __getattr__(self, name):
        if name == "box":
            b = self.sysdef.getParticleData().getGlobalBox();
            L = b.getL();
            return boxdim(Lx=L.x, Ly=L.y, Lz=L.z, xy=b.getTiltFactorXY(), xz=b.getTiltFactorXZ(), yz=b.getTiltFactorYZ(), dimensions=self.sysdef.getNDimensions());

        # if we get here, we haven't found any names that match, post an error
        raise AttributeError;

## \internal
# \brief Access the list of types
#
# pdata_types_proxy provides access to the type names and the possibility to add types to the simulation
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# particle_data, documented by example.
#
class pdata_types_proxy:
    ## \internal
    # \brief particle_data iterator
    class pdata_types_iterator:
        def __init__(self, data):
            self.data = data;
            self.index = 0;
        def __iter__(self):
            return self;
        def __next__(self):
            if self.index == len(self.data):
                raise StopIteration;

            result = self.data[self.index];
            self.index += 1;
            return result;

        # support python2
        next = __next__;

    ## \internal
    # \brief create a pdata_types_proxy
    #
    # \param pdata ParticleData to connect
    def __init__(self, pdata):
        self.pdata = pdata;

    ## \var pdata
    # \internal
    # \brief ParticleData to which this instance is connected

    ## \internal
    # \brief Get a the name of a type
    # \param type_idx Type index
    def __getitem__(self, type_idx):
        ntypes = self.pdata.getNTypes();
        if type_idx >= ntypes or type_idx < 0:
            raise IndexError;
        return self.pdata.getNameByType(type_idx);

    ## \internal
    # \brief Set the name of a type
    # \param type_idx Particle tag to set
    # \param name New type name
    def __setitem__(self, type_idx, name):
        ntypes = self.pdata.getNTypes();
        if type_idx >= ntypes or type_idx < 0:
            raise IndexError;
        self.pdata.setTypeName(type_idx, name);

    ## \internal
    # \brief Get the number of types
    def __len__(self):
        return self.pdata.getNTypes();

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        ntypes = self.pdata.getNTypes();
        result = "Particle types: ["
        for i in range(0,ntypes):
            result += "'" + self.pdata.getNameByType(i) + "'"
            if (i != ntypes-1):
                result += ", "
            else:
                result += "]"

        return result

    ## \internal
    # \brief Return an interator
    def __iter__(self):
        return pdata_types_proxy.pdata_types_iterator(self);

    ## \internal
    # \brief Add a new particle type
    # \param name Name of type to add
    # \returns Index of newly added type
    def add(self, name):
        # check that type does not yet exist
        ntypes = self.pdata.getNTypes();
        for i in range(0,ntypes):
            if self.pdata.getNameByType(i) == name:
                globals.msg.warning("Type '"+name+"' already defined.\n");
                return i

        typeid = self.pdata.addType(name);
        return typeid


## \internal
# \brief Access particle data
#
# particle_data provides access to the per-particle data of all particles in the system.
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# particle_data, documented by example.
#
class particle_data(meta._metadata):
    ## \internal
    # \brief particle_data iterator
    class particle_data_iterator:
        def __init__(self, data):
            self.data = data;
            self.index = 0;
        def __iter__(self):
            return self;
        def __next__(self):
            if self.index == len(self.data):
                raise StopIteration;

            result = self.data[self.index];
            self.index += 1;
            return result;

        # support python2
        next = __next__;

    ## \internal
    # \brief create a particle_data
    #
    # \param pdata ParticleData to connect
    def __init__(self, pdata):
        self.pdata = pdata;

        self.types = pdata_types_proxy(globals.system_definition.getParticleData())

        # base class constructor
        meta._metadata.__init__(self)

    ## \var pdata
    # \internal
    # \brief ParticleData to which this instance is connected

    ## \internal
    # \brief Get a particle_proxy reference to the particle with contiguous id \a id
    # \param id Contiguous particle id to access
    def __getitem__(self, id):
        if id >= len(self) or id < 0:
            raise IndexError;
        tag = self.pdata.getNthTag(id);
        return particle_data_proxy(self.pdata, tag);

    ## \internal
    # \brief Get a particle_proxy reference to the particle with tag \a tag
    # \param tag Particle tag to access
    def get(self, tag):
        if tag > self.pdata.getMaximumTag() or tag < 0:
            raise IndexError;
        return particle_data_proxy(self.pdata, tag);

    ## \internal
    # \brief Set a particle's properties
    # \param tag Particle tag to set
    # \param p Value containing properties to set
    def __setitem__(self, tag, p):
        raise RuntimeError('__setitem__ not implemented');

    ## \internal
    # \brief Add a new particle
    # \param type Type name of the particle to add
    # \returns Unique tag identifying this bond
    def add(self, type):
        typeid = self.pdata.getTypeByName(type);
        return self.pdata.addParticle(typeid);

    ## \internal
    # \brief Remove a bond by tag
    # \param tag Unique tag of the bond to remove
    def remove(self, tag):
        self.pdata.removeParticle(tag);

    ## \internal
    # \brief Delete a particle by id
    # \param id Bond id to delete
    def __delitem__(self, id):
        if id >= len(self) or id < 0:
            raise IndexError;
        tag = self.pdata.getNthTag(id);
        self.pdata.removeParticle(tag);

    ## \internal
    # \brief Get the number of particles
    def __len__(self):
        return self.pdata.getNGlobal();

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        result = "Particle Data for %d particles of %d type(s)" % (self.pdata.getNGlobal(), self.pdata.getNTypes());
        return result

    ## \internal
    # \brief Return an iterator
    def __iter__(self):
        return particle_data.particle_data_iterator(self);

    ## \internal
    # \brief Return metadata for this particle_data instance
    def get_metadata(self):
        data = meta._metadata.get_metadata(self)
        data['N'] = len(self)
        data['types'] = list(self.types);
        return data

## Access a single particle via a proxy
#
# particle_data_proxy provides access to all of the properties of a single particle in the system.
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# particle_data_proxy, documented by example.
#
# The following attributes are read only:
# - \c tag          : An integer indexing the particle in the system. Tags run from 0 to N-1;
# - \c acceleration : A 3-tuple of floats   (x, y, z) Note that acceleration is a calculated quantity and cannot be set. (in acceleration units)
# - \c typeid       : An integer defining the type id
#
# The following attributes can be both read and set
# - \c position     : A 3-tuple of floats   (x, y, z) (in distance units)
# - \c image        : A 3-tuple of integers (x, y, z)
# - \c velocity     : A 3-tuple of floats   (x, y, z) (in velocity units)
# - \c charge       : A single float
# - \c mass         : A single float (in mass units)
# - \c diameter     : A single float (in distance units)
# - \c type         : A string naming the type
# - \c body         : Rigid body id integer (-1 for free particles)
# - \c orientation  : Orientation of anisotropic particle (quaternion)
# - \c net_force    : Net force on particle (x, y, z) (in force units)
# - \c net_energy   : Net contribution of particle to the potential energy (in energy units)
# - \c net_torque   : Net torque on the particle (x, y, z) (in torque units)
#
# In the current version of the API, only already defined type names can be used. A future improvement will allow
# dynamic creation of new type names from within the python API.
#
class particle_data_proxy:
    ## \internal
    # \brief create a particle_data_proxy
    #
    # \param pdata ParticleData to which this proxy belongs
    # \param tag Tag this particle in \a pdata
    def __init__(self, pdata, tag):
        self.pdata = pdata;
        self.tag = tag

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        result = "";
        result += "tag         : " + str(self.tag) + "\n"
        result += "position    : " + str(self.position) + "\n";
        result += "image       : " + str(self.image) + "\n";
        result += "velocity    : " + str(self.velocity) + "\n";
        result += "acceleration: " + str(self.acceleration) + "\n";
        result += "charge      : " + str(self.charge) + "\n";
        result += "mass        : " + str(self.mass) + "\n";
        result += "diameter    : " + str(self.diameter) + "\n";
        result += "type        : " + str(self.type) + "\n";
        result += "typeid      : " + str(self.typeid) + "\n";
        result += "body        : " + str(self.body) + "\n";
        result += "orientation : " + str(self.orientation) + "\n";
        result += "mom. inertia: " + str(self.moment_inertia) + "\n";
        result += "angular_momentum: " + str(self.angular_momentum) + "\n";
        result += "net_force   : " + str(self.net_force) + "\n";
        result += "net_energy  : " + str(self.net_energy) + "\n";
        result += "net_torque  : " + str(self.net_torque) + "\n";
        return result;

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __getattr__(self, name):
        if name == "position":
            pos = self.pdata.getPosition(self.tag);
            return (pos.x, pos.y, pos.z);
        if name == "velocity":
            vel = self.pdata.getVelocity(self.tag);
            return (vel.x, vel.y, vel.z);
        if name == "acceleration":
            accel = self.pdata.getAcceleration(self.tag);
            return (accel.x, accel.y, accel.z);
        if name == "image":
            image = self.pdata.getImage(self.tag);
            return (image.x, image.y, image.z);
        if name == "charge":
            return self.pdata.getCharge(self.tag);
        if name == "mass":
            return self.pdata.getMass(self.tag);
        if name == "diameter":
            return self.pdata.getDiameter(self.tag);
        if name == "typeid":
            return self.pdata.getType(self.tag);
        if name == "body":
            return self.pdata.getBody(self.tag);
        if name == "type":
            typeid = self.pdata.getType(self.tag);
            return self.pdata.getNameByType(typeid);
        if name == "orientation":
            o = self.pdata.getOrientation(self.tag);
            return (o.x, o.y, o.z, o.w);
        if name == "angular_momentum":
            a = self.pdata.getAngularMomentum(self.tag);
            return (a.x, a.y, a.z, a.w);
        if name == "moment_inertia":
            m = self.pdata.getMomentsOfInertia(self.tag)
            return (m.x, m.y, m.z);
        if name == "net_force":
            f = self.pdata.getPNetForce(self.tag);
            return (f.x, f.y, f.z);
        if name == "net_energy":
            f = self.pdata.getPNetForce(self.tag);
            return f.w;
        if name == "net_torque":
            f = self.pdata.getNetTorque(self.tag);
            return (f.x, f.y, f.z);

        # if we get here, we haven't found any names that match, post an error
        raise AttributeError;

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __setattr__(self, name, value):
        if name == "position":
            v = hoomd.Scalar3();
            v.x = float(value[0]);
            v.y = float(value[1]);
            v.z = float(value[2]);
            self.pdata.setPosition(self.tag, v, True);
            return;
        if name == "velocity":
            v = hoomd.Scalar3();
            v.x = float(value[0]);
            v.y = float(value[1]);
            v.z = float(value[2]);
            self.pdata.setVelocity(self.tag, v);
            return;
        if name == "image":
            v = hoomd.int3();
            v.x = int(value[0]);
            v.y = int(value[1]);
            v.z = int(value[2]);
            self.pdata.setImage(self.tag, v);
            return;
        if name == "charge":
            self.pdata.setCharge(self.tag, float(value));
            return;
        if name == "mass":
            self.pdata.setMass(self.tag, float(value));
            return;
        if name == "diameter":
            self.pdata.setDiameter(self.tag, value);
            return;
        if name == "body":
            self.pdata.setBody(self.tag, value);
            return;
        if name == "type":
            typeid = self.pdata.getTypeByName(value);
            self.pdata.setType(self.tag, typeid);
            return;
        if name == "typeid":
            raise AttributeError;
        if name == "acceleration":
            raise AttributeError;
        if name == "orientation":
            o = hoomd.Scalar4();
            o.x = float(value[0]);
            o.y = float(value[1]);
            o.z = float(value[2]);
            o.w = float(value[3]);
            self.pdata.setOrientation(self.tag, o);
            return;
        if name == "angular_momentum":
            a = hoomd.Scalar4();
            a.x = float(value[0]);
            a.y = float(value[1]);
            a.z = float(value[2]);
            a.w = float(value[3]);
            self.pdata.setAngularMomentum(self.tag, a);
            return;
        if name == "moment_inertia":
            m = hoomd.Scalar3();
            m.x = float(value[0]);
            m.y = float(value[1]);
            m.z = float(value[2]);
            self.pdata.setMomentsOfInertia(self.tag, m);
            return;
        if name == "net_force":
            raise AttributeError;
        if name == "net_energy":
            raise AttributeError;

        # otherwise, consider this an internal attribute to be set in the normal way
        self.__dict__[name] = value;

## \internal
# Access force data
#
# force_data provides access to the per-particle data of all forces in the system.
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# force_data, documented by example.
#
class force_data:
    ## \internal
    # \brief force_data iterator
    class force_data_iterator:
        def __init__(self, data):
            self.data = data;
            self.index = 0;
        def __iter__(self):
            return self;
        def __next__(self):
            if self.index == len(self.data):
                raise StopIteration;

            result = self.data[self.index];
            self.index += 1;
            return result;

        # support python2
        next = __next__;

    ## \internal
    # \brief create a force_data
    #
    # \param pdata ParticleData to connect
    def __init__(self, force):
        self.force = force;

    ## \var force
    # \internal
    # \brief ForceCompute to which this instance is connected

    ## \internal
    # \brief Get a force_proxy reference to the particle with tag \a tag
    # \param tag Particle tag to access
    def __getitem__(self, tag):
        if tag >= len(self) or tag < 0:
            raise IndexError;
        return force_data_proxy(self.force, tag);

    ## \internal
    # \brief Set a particle's properties
    # \param tag Particle tag to set
    # \param p Value containing properties to set
    def __setitem__(self, tag, p):
        raise RuntimeError('__setitem__ not implemented');

    ## \internal
    # \brief Get the number of particles
    def __len__(self):
        return globals.system_definition.getParticleData().getNGlobal();

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        result = "Force Data for %d particles" % (len(self));
        return result

    ## \internal
    # \brief Return an interator
    def __iter__(self):
        return force_data.force_data_iterator(self);

## Access the %force on a single particle via a proxy
#
# force_data_proxy provides access to the current %force, virial, and energy of a single particle due to a single
# %force computations.
#
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# force_data_proxy, documented by example.
#
# The following attributes are read only:
# - \c %force         : A 3-tuple of floats (x, y, z) listing the current %force on the particle
# - \c virial         : A float containing the contribution of this particle to the total virial
# - \c energy         : A float containing the contribution of this particle to the total potential energy
# - \c torque         : A 3-tuple of floats (x, y, z) listing the current torque on the particle
#
class force_data_proxy:
    ## \internal
    # \brief create a force_data_proxy
    #
    # \param force ForceCompute to which this proxy belongs
    # \param tag Tag of this particle in \a force
    def __init__(self, force, tag):
        self.fdata = force;
        self.tag = tag;

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        result = "";
        result += "tag         : " + str(self.tag) + "\n"
        result += "force       : " + str(self.force) + "\n";
        result += "virial      : " + str(self.virial) + "\n";
        result += "energy      : " + str(self.energy) + "\n";
        result += "torque      : " + str(self.torque) + "\n";
        return result;

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __getattr__(self, name):
        if name == "force":
            f = self.fdata.cpp_force.getForce(self.tag);
            return (f.x, f.y, f.z);
        if name == "virial":
            return (self.fdata.cpp_force.getVirial(self.tag,0),
                    self.fdata.cpp_force.getVirial(self.tag,1),
                    self.fdata.cpp_force.getVirial(self.tag,2),
                    self.fdata.cpp_force.getVirial(self.tag,3),
                    self.fdata.cpp_force.getVirial(self.tag,4),
                    self.fdata.cpp_force.getVirial(self.tag,5));
        if name == "energy":
            energy = self.fdata.cpp_force.getEnergy(self.tag);
            return energy;
        if name == "torque":
            f = self.fdata.cpp_force.getTorque(self.tag);
            return (f.x, f.y, f.z)

        # if we get here, we haven't found any names that match, post an error
        raise AttributeError;

## \internal
# \brief Access bond data
#
# bond_data provides access to the bonds in the system.
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# bond_data, documented by example.
#
class bond_data(meta._metadata):
    ## \internal
    # \brief bond_data iterator
    class bond_data_iterator:
        def __init__(self, data):
            self.data = data;
            self.tag = 0;
        def __iter__(self):
            return self;
        def __next__(self):
            if self.tag == len(self.data):
                raise StopIteration;

            result = self.data[self.tag];
            self.tag += 1;
            return result;

        # support python2
        next = __next__;

    ## \internal
    # \brief create a bond_data
    #
    # \param bdata BondData to connect
    def __init__(self, bdata):
        self.bdata = bdata;

        # base class constructor
        meta._metadata.__init__(self)

    ## \internal
    # \brief Add a new bond
    # \param type Type name of the bond to add
    # \param a Tag of the first particle in the bond
    # \param b Tag of the second particle in the bond
    # \returns Unique tag identifying this bond
    def add(self, type, a, b):
        typeid = self.bdata.getTypeByName(type);
        return self.bdata.addBondedGroup(hoomd.Bond(typeid, a, b));

    ## \internal
    # \brief Remove a bond by tag
    # \param tag Unique tag of the bond to remove
    def remove(self, tag):
        self.bdata.removeBondedGroup(tag);

    ## \var bdata
    # \internal
    # \brief BondData to which this instance is connected

    ## \internal
    # \brief Get a bond_data_proxy reference to the bond with contiguous id \a id
    # \param id Bond id to access
    def __getitem__(self, id):
        if id >= len(self) or id < 0:
            raise IndexError;
        tag = self.bdata.getNthTag(id);
        return bond_data_proxy(self.bdata, tag);

    ## \internal
    # \brief Get a bond_data_proxy reference to the bond with tag \a tag
    # \param tag Bond tag to access
    def get(self, tag):
        if tag > self.bdata.getMaximumTag() or tag < 0:
            raise IndexError;
        return bond_data_proxy(self.bdata, tag);

    ## \internal
    # \brief Set a bond's properties
    # \param id Bond id to set
    # \param b Value containing properties to set
    def __setitem__(self, id, b):
        raise RuntimeError('Cannot change bonds once they are created');

    ## \internal
    # \brief Delete a bond by id
    # \param id Bond id to delete
    def __delitem__(self, id):
        if id >= len(self) or id < 0:
            raise IndexError;
        tag = self.bdata.getNthTag(id);
        self.bdata.removeBondedGroup(tag);

    ## \internal
    # \brief Get the number of bonds
    def __len__(self):
        return self.bdata.getNGlobal();

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        result = "Bond Data for %d bonds of %d typeid(s)" % (self.bdata.getNGlobal(), self.bdata.getNTypes());
        return result

    ## \internal
    # \brief Return an interator
    def __iter__(self):
        return bond_data.bond_data_iterator(self);

    ## \internal
    # \brief Return metadata for this bond_data instance
    def get_metadata(self):
        data = meta._metadata.get_metadata(self)
        data['N'] = len(self)
        data['types'] = [self.bdata.getNameByType(i) for i in range(self.bdata.getNTypes())];
        return data

## Access a single bond via a proxy
#
# bond_data_proxy provides access to all of the properties of a single bond in the system.
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# bond_data_proxy, documented by example.
#
# The following attributes are read only:
# - \c tag          : A unique integer attached to each bond (not in any particular range). A bond's tag remans fixed
#                     during its lifetime. (Tags previously used by removed bonds may be recycled).
# - \c typeid       : An integer indexing the bond type of the bond.
# - \c a            : An integer indexing the A particle in the bond. Particle tags run from 0 to N-1;
# - \c b            : An integer indexing the B particle in the bond. Particle tags run from 0 to N-1;
# - \c type         : A string naming the type
#
# In the current version of the API, only already defined type names can be used. A future improvement will allow
# dynamic creation of new type names from within the python API.
# \MPI_SUPPORTED
class bond_data_proxy:
    ## \internal
    # \brief create a bond_data_proxy
    #
    # \param bdata BondData to which this proxy belongs
    # \param tag Tag of this bond in \a bdata
    def __init__(self, bdata, tag):
        self.bdata = bdata;
        self.tag = tag;

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        result = "";
        result += "typeid       : " + str(self.typeid) + "\n";
        result += "a            : " + str(self.a) + "\n"
        result += "b            : " + str(self.b) + "\n"
        result += "type         : " + str(self.type) + "\n";
        return result;

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __getattr__(self, name):
        if name == "a":
            bond = self.bdata.getGroupByTag(self.tag);
            return bond.a;
        if name == "b":
            bond = self.bdata.getGroupByTag(self.tag);
            return bond.b;
        if name == "typeid":
            bond = self.bdata.getGroupByTag(self.tag);
            return bond.type;
        if name == "type":
            bond = self.bdata.getGroupByTag(self.tag);
            typeid = bond.type;
            return self.bdata.getNameByType(typeid);

        # if we get here, we haven't found any names that match, post an error
        raise AttributeError;

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __setattr__(self, name, value):
        if name == "a":
            raise AttributeError;
        if name == "b":
            raise AttributeError;
        if name == "type":
            raise AttributeError;
        if name == "typeid":
            raise AttributeError;

        # otherwise, consider this an internal attribute to be set in the normal way
        self.__dict__[name] = value;

## \internal
# \brief Access angle data
#
# angle_data provides access to the angles in the system.
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# angle_data, documented by example.
#
class angle_data(meta._metadata):
    ## \internal
    # \brief angle_data iterator
    class angle_data_iterator:
        def __init__(self, data):
            self.data = data;
            self.index = 0;
        def __iter__(self):
            return self;
        def __next__(self):
            if self.index == len(self.data):
                raise StopIteration;

            result = self.data[self.index];
            self.index += 1;
            return result;

        # support python2
        next = __next__;

    ## \internal
    # \brief create a angle_data
    #
    # \param bdata AngleData to connect
    def __init__(self, adata):
        self.adata = adata;

        # base class constructor
        meta._metadata.__init__(self)

    ## \internal
    # \brief Add a new angle
    # \param type Type name of the angle to add
    # \param a Tag of the first particle in the angle
    # \param b Tag of the second particle in the angle
    # \param c Tag of the thrid particle in the angle
    # \returns Unique tag identifying this bond
    def add(self, type, a, b, c):
        typeid = self.adata.getTypeByName(type);
        return self.adata.addBondedGroup(hoomd.Angle(typeid, a, b, c));

    ## \internal
    # \brief Remove an angle by tag
    # \param tag Unique tag of the angle to remove
    def remove(self, tag):
        self.adata.removeBondedGroup(tag);

    ## \var adata
    # \internal
    # \brief AngleData to which this instance is connected

    ## \internal
    # \brief Get an angle_data_proxy reference to the angle with contiguous id \a id
    # \param id Angle id to access
    def __getitem__(self, id):
        if id >= len(self) or id < 0:
            raise IndexError;
        tag = self.adata.getNthTag(id);
        return angle_data_proxy(self.adata, tag);

    ## \internal
    # \brief Get a angle_data_proxy reference to the angle with tag \a tag
    # \param tag Angle tag to access
    def get(self, tag):
        if tag > self.adata.getMaximumTag() or tag < 0:
            raise IndexError;
        return angle_data_proxy(self.adata, tag);

    ## \internal
    # \brief Set an angle's properties
    # \param id Angle id to set
    # \param b Value containing properties to set
    def __setitem__(self, id, b):
        raise RuntimeError('Cannot change angles once they are created');

    ## \internal
    # \brief Delete an angle by id
    # \param id Angle id to delete
    def __delitem__(self, id):
        if id >= len(self) or id < 0:
            raise IndexError;

        # Get the tag of the bond to delete
        tag = self.adata.getNthTag(id);
        self.adata.removeBondedGroup(tag);

    ## \internal
    # \brief Get the number of angles
    def __len__(self):
        return self.adata.getNGlobal();

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        result = "Angle Data for %d angles of %d typeid(s)" % (self.adata.getNGlobal(), self.adata.getNTypes());
        return result;

    ## \internal
    # \brief Return an interator
    def __iter__(self):
        return angle_data.angle_data_iterator(self);

    ## \internal
    # \brief Return metadata for this angle_data instance
    def get_metadata(self):
        data = meta._metadata.get_metadata(self)
        data['N'] = len(self)
        data['types'] = [self.adata.getNameByType(i) for i in range(self.adata.getNTypes())];
        return data

## Access a single angle via a proxy
#
# angle_data_proxy provides access to all of the properties of a single angle in the system.
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# angle_data_proxy, documented by example.
#
# The following attributes are read only:
# - \c tag          : A unique integer attached to each angle (not in any particular range). A angle's tag remans fixed
#                     during its lifetime. (Tags previously used by removed angles may be recycled).
# - \c typeid       : An integer indexing the angle's type.
# - \c a            : An integer indexing the A particle in the angle. Particle tags run from 0 to N-1;
# - \c b            : An integer indexing the B particle in the angle. Particle tags run from 0 to N-1;
# - \c c            : An integer indexing the C particle in the angle. Particle tags run from 0 to N-1;
# - \c type         : A string naming the type
#
# In the current version of the API, only already defined type names can be used. A future improvement will allow
# dynamic creation of new type names from within the python API.
# \MPI_SUPPORTED
class angle_data_proxy:
    ## \internal
    # \brief create a angle_data_proxy
    #
    # \param adata AngleData to which this proxy belongs
    # \param tag Tag of this angle in \a adata
    def __init__(self, adata, tag):
        self.adata = adata;
        self.tag = tag;

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        result = "";
        result += "tag          : " + str(self.tag) + "\n";
        result += "typeid       : " + str(self.typeid) + "\n";
        result += "a            : " + str(self.a) + "\n"
        result += "b            : " + str(self.b) + "\n"
        result += "c            : " + str(self.c) + "\n"
        result += "type         : " + str(self.type) + "\n";
        return result;

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __getattr__(self, name):
        if name == "a":
            angle = self.adata.getGroupByTag(self.tag);
            return angle.a;
        if name == "b":
            angle = self.adata.getGroupByTag(self.tag);
            return angle.b;
        if name == "c":
            angle = self.adata.getGroupByTag(self.tag);
            return angle.c;
        if name == "typeid":
            angle = self.adata.getGroupByTag(self.tag);
            return angle.type;
        if name == "type":
            angle = self.adata.getGroupByTag(self.tag);
            typeid = angle.type;
            return self.adata.getNameByType(typeid);

        # if we get here, we haven't found any names that match, post an error
        raise AttributeError;

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __setattr__(self, name, value):
        if name == "a":
            raise AttributeError;
        if name == "b":
            raise AttributeError;
        if name == "c":
            raise AttributeError;
        if name == "type":
            raise AttributeError;
        if name == "typeid":
            raise AttributeError;

        # otherwise, consider this an internal attribute to be set in the normal way
        self.__dict__[name] = value;

## \internal
# \brief Access dihedral data
#
# dihedral_data provides access to the dihedrals in the system.
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# dihedral_data, documented by example.
#
class dihedral_data(meta._metadata):
    ## \internal
    # \brief dihedral_data iterator
    class dihedral_data_iterator:
        def __init__(self, data):
            self.data = data;
            self.index = 0;
        def __iter__(self):
            return self;
        def __next__(self):
            if self.index == len(self.data):
                raise StopIteration;

            result = self.data[self.index];
            self.index += 1;
            return result;

        # support python2
        next = __next__;

    ## \internal
    # \brief create a dihedral_data
    #
    # \param bdata DihedralData to connect
    def __init__(self, ddata):
        self.ddata = ddata;

        # base class constructor
        meta._metadata.__init__(self)

    ## \internal
    # \brief Add a new dihedral
    # \param type Type name of the dihedral to add
    # \param a Tag of the first particle in the dihedral
    # \param b Tag of the second particle in the dihedral
    # \param c Tag of the thrid particle in the dihedral
    # \param d Tag of the fourth particle in the dihedral
    # \returns Unique tag identifying this bond
    def add(self, type, a, b, c, d):
        typeid = self.ddata.getTypeByName(type);
        return self.ddata.addBondedGroup(hoomd.Dihedral(typeid, a, b, c, d));

    ## \internal
    # \brief Remove an dihedral by tag
    # \param tag Unique tag of the dihedral to remove
    def remove(self, tag):
        self.ddata.removeBondedGroup(tag);

    ## \var ddata
    # \internal
    # \brief DihedralData to which this instance is connected

    ## \internal
    # \brief Get an dihedral_data_proxy reference to the dihedral with contiguous id \a id
    # \param id Dihedral id to access
    def __getitem__(self, id):
        if id >= len(self) or id < 0:
            raise IndexError;
        tag = self.ddata.getNthTag(id);
        return dihedral_data_proxy(self.ddata, tag);

    ## \internal
    # \brief Get a dihedral_data_proxy reference to the dihedral with tag \a tag
    # \param tag Dihedral tag to access
    def get(self, tag):
        if tag > self.ddata.getMaximumTag() or tag < 0:
            raise IndexError;
        return dihedral_data_proxy(self.ddata, tag);

    ## \internal
    # \brief Set an dihedral's properties
    # \param id dihedral id to set
    # \param b Value containing properties to set
    def __setitem__(self, id, b):
        raise RuntimeError('Cannot change angles once they are created');

    ## \internal
    # \brief Delete an dihedral by id
    # \param id Dihedral id to delete
    def __delitem__(self, id):
        if id >= len(self) or id < 0:
            raise IndexError;

        # Get the tag of the bond to delete
        tag = self.ddata.getNthTag(id);
        self.ddata.removeBondedGroup(tag);

    ## \internal
    # \brief Get the number of angles
    def __len__(self):
        return self.ddata.getNGlobal();

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        result = "Dihedral Data for %d angles of %d typeid(s)" % (self.ddata.getNGlobal(), self.ddata.getNTypes());
        return result;

    ## \internal
    # \brief Return an interator
    def __iter__(self):
        return dihedral_data.dihedral_data_iterator(self);

    ## \internal
    # \brief Return metadata for this dihedral_data instance
    def get_metadata(self):
        data = meta._metadata.get_metadata(self)
        data['N'] = len(self)
        data['types'] = [self.ddata.getNameByType(i) for i in range(self.ddata.getNTypes())];
        return data

## Access a single dihedral via a proxy
#
# dihedral_data_proxy provides access to all of the properties of a single dihedral in the system.
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# dihedral_data_proxy, documented by example.
#
# The following attributes are read only:
# - \c tag          : A unique integer attached to each dihedral (not in any particular range). A dihedral's tag remans fixed
#                     during its lifetime. (Tags previously used by removed dihedral may be recycled).
# - \c typeid       : An integer indexing the dihedral's type.
# - \c a            : An integer indexing the A particle in the angle. Particle tags run from 0 to N-1;
# - \c b            : An integer indexing the B particle in the angle. Particle tags run from 0 to N-1;
# - \c c            : An integer indexing the C particle in the angle. Particle tags run from 0 to N-1;
# - \c d            : An integer indexing the D particle in the dihedral. Particle tags run from 0 to N-1;
# - \c type         : A string naming the type
#
# In the current version of the API, only already defined type names can be used. A future improvement will allow
# dynamic creation of new type names from within the python API.
# \MPI_SUPPORTED
class dihedral_data_proxy:
    ## \internal
    # \brief create a dihedral_data_proxy
    #
    # \param ddata DihedralData to which this proxy belongs
    # \param tag Tag of this dihedral in \a ddata
    def __init__(self, ddata, tag):
        self.ddata = ddata;
        self.tag = tag;

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        result = "";
        result += "tag          : " + str(self.tag) + "\n";
        result += "typeid       : " + str(self.typeid) + "\n";
        result += "a            : " + str(self.a) + "\n"
        result += "b            : " + str(self.b) + "\n"
        result += "c            : " + str(self.c) + "\n"
        result += "d            : " + str(self.d) + "\n"
        result += "type         : " + str(self.type) + "\n";
        return result;

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __getattr__(self, name):
        if name == "a":
            dihedral = self.ddata.getGroupByTag(self.tag);
            return dihedral.a;
        if name == "b":
            dihedral = self.ddata.getGroupByTag(self.tag);
            return dihedral.b;
        if name == "c":
            dihedral = self.ddata.getGroupByTag(self.tag);
            return dihedral.c;
        if name == "d":
            dihedral = self.ddata.getGroupByTag(self.tag);
            return dihedral.d;
        if name == "typeid":
            dihedral = self.ddata.getGroupByTag(self.tag);
            return dihedral.type;
        if name == "type":
            dihedral = self.ddata.getGroupByTag(self.tag);
            typeid = dihedral.type;
            return self.ddata.getNameByType(typeid);

        # if we get here, we haven't found any names that match, post an error
        raise AttributeError;

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __setattr__(self, name, value):
        if name == "a":
            raise AttributeError;
        if name == "b":
            raise AttributeError;
        if name == "c":
            raise AttributeError;
        if name == "d":
            raise AttributeError;
        if name == "type":
            raise AttributeError;
        if name == "typeid":
            raise AttributeError;

        # otherwise, consider this an internal attribute to be set in the normal way
        self.__dict__[name] = value;

## \internal
# \brief Access body data
#
# body_data provides access to the per-body data of all bodies in the system.
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# body_data, documented by example.
#
class body_data(meta._metadata):
    ## \internal
    # \brief bond_data iterator
    class body_data_iterator:
        def __init__(self, data):
            self.data = data;
            self.index = 0;
        def __iter__(self):
            return self;
        def __next__(self):
            if self.index == len(self.data):
                raise StopIteration;

            result = self.data[self.index];
            self.index += 1;
            return result;

        # support python2
        next = __next__;

    ## \internal
    # \brief create a body_data
    #
    # \param bdata BodyData to connect
    def __init__(self, bdata):
        self.bdata = bdata;
        meta._metadata.__init__(self)

    # \brief updates the v and x positions of a rigid body
    # \note the second arguement is dt, but the value should not matter as long as not zero
    def updateRV(self):
        self.bdata.setRV(True);

    ## \var bdata
    # \internal
    # \brief BodyData to which this instance is connected

    ## \internal
    # \brief Get a body_proxy reference to the body with body index \a tag
    # \param tag Body tag to access
    def __getitem__(self, tag):
        if tag >= len(self) or tag < 0:
            raise IndexError;
        return body_data_proxy(self.bdata, tag);

    ## \internal
    # \brief Set a body's properties
    # \param tag Body tag to set
    # \param p Value containing properties to set
    def __setitem__(self, tag, p):
        raise RuntimeError('__setitem__ not implemented');

    ## \internal
    # \brief Get the number of bodies
    def __len__(self):
        return self.bdata.getNumBodies();

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        result = "Body Data for %d bodies" % (self.bdata.getNumBodies());
        return result

    ## \internal
    # \brief Return an interator
    def __iter__(self):
        return body_data.body_data_iterator(self);

    ## \internal
    # \brief Return metadata for this body_data instance
    def get_metadata(self):
        data = meta._metadata.get_metadata(self)
        data['nbodies'] = len(self)
        return data


## Access a single body via a proxy
#
# body_data_proxy provides access to all of the properties of a single bond in the system.
# This documentation is intentionally left sparse, see hoomd_script.data for a full explanation of how to use
# body_data_proxy, documented by example.
#
# The following attributes are read only:
# - \c num_particles : The number of particles (or interaction sites) composing the body
# - \c particle_tags : the tags of the particles (or interaction sites) composing the body
# - \c net_force     : Net force acting on the body (x, y, z) (in force units)
# - \c net_torque    : Net torque acting on the body (x, y, z) (in units of force * distance)
#
# The following attributes can be both read and set
# - \c mass          : The mass of the body
# - \c COM           : The Center of Mass position of the body
# - \c velocity      : The velocity vector of the center of mass of the body
# - \c orientation   : The orientation of the body (quaternion)
# - \c angular_momentum : The angular momentum of the body in the space frame
# - \c moment_inertia : the principle components of the moment of inertia
# - \c particle_disp : the displacements of the particles (or interaction sites) of the body relative to the COM in the body frame.
# \MPI_NOT_SUPPORTED
class body_data_proxy:
    ## \internal
    # \brief create a body_data_proxy
    #
    # \param bdata RigidData to which this proxy belongs
    # \param tag tag of this body in \a bdata
    def __init__(self, bdata, tag):

        # Error out in MPI simulations
        if (hoomd.is_MPI_available()):
            if globals.system_definition.getParticleData().getDomainDecomposition():
                globals.msg.error("Rigid bodies are not supported in multi-processor simulations.\n\n")
                raise RuntimeError("Error accessing body data.")

        self.bdata = bdata;
        self.tag = tag;

    ## \internal
    # \brief Get an informal string representing the object
    def __str__(self):
        result = "";
        result += "num_particles    : " + str(self.num_particles) + "\n"
        result += "mass             : " + str(self.mass) + "\n"
        result += "COM              : " + str(self.COM) + "\n"
        result += "velocity         : " + str(self.velocity) + "\n"
        result += "orientation      : " + str(self.orientation) + "\n"
        result += "angular_momentum (space frame) : " + str(self.angular_momentum) + "\n"
        result += "moment_inertia: " + str(self.moment_inertia) + "\n"
        result += "particle_tags    : " + str(self.particle_tags) + "\n"
        result += "particle_disp    : " + str(self.particle_disp) + "\n"
        result += "net_force        : " + str(self.net_force) + "\n"
        result += "net_torque       : " + str(self.net_torque) + "\n"

        return result;

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __getattr__(self, name):
        if name == "COM":
            COM = self.bdata.getBodyCOM(self.tag);
            return (COM.x, COM.y, COM.z);
        if name == "velocity":
            velocity = self.bdata.getBodyVel(self.tag);
            return (velocity.x, velocity.y, velocity.z);
        if name == "orientation":
            orientation = self.bdata.getBodyOrientation(self.tag);
            return (orientation.x, orientation.y, orientation.z, orientation.w);
        if name == "angular_momentum":
            angular_momentum = self.bdata.getBodyAngMom(self.tag);
            return (angular_momentum.x, angular_momentum.y, angular_momentum.z);
        if name == "num_particles":
            num_particles = self.bdata.getBodyNSize(self.tag);
            return num_particles;
        if name == "mass":
            mass = self.bdata.getMass(self.tag);
            return mass;
        if name == "moment_inertia":
            moment_inertia = self.bdata.getBodyMomInertia(self.tag);
            return (moment_inertia.x, moment_inertia.y, moment_inertia.z);
        if name == "particle_tags":
            particle_tags = [];
            for i in range(0, self.num_particles):
               particle_tags.append(self.bdata.getParticleTag(self.tag, i));
            return particle_tags;
        if name == "particle_disp":
            particle_disp = [];
            for i in range(0, self.num_particles):
               disp = self.bdata.getParticleDisp(self.tag, i);
               particle_disp.append([disp.x, disp.y, disp.z]);
            return particle_disp;
        if name == "net_force":
            f = self.bdata.getBodyNetForce(self.tag);
            return (f.x, f.y, f.z);
        if name == "net_torque":
            t = self.bdata.getBodyNetTorque(self.tag);
            return (t.x, t.y, t.z);

        # if we get here, we haven't found any names that match, post an error
        raise AttributeError;

    ## \internal
    # \brief Translate attribute accesses into the low level API function calls
    def __setattr__(self, name, value):
        if name == "COM":
            p = hoomd.Scalar3();
            p.x = float(value[0]);
            p.y = float(value[1]);
            p.z = float(value[2]);
            self.bdata.setBodyCOM(self.tag, p);
            return;
        if name == "velocity":
            v = hoomd.Scalar3();
            v.x = float(value[0]);
            v.y = float(value[1]);
            v.z = float(value[2]);
            self.bdata.setBodyVel(self.tag, v);
            return;
        if name == "mass":
            self.bdata.setMass(self.tag, value);
            return;
        if name == "orientation":
            q = hoomd.Scalar4();
            q.x = float(value[0]);
            q.y = float(value[1]);
            q.z = float(value[2]);
            q.w = float(value[3]);
            self.bdata.setBodyOrientation(self.tag, q);
            return;
        if name == "angular_momentum":
            p = hoomd.Scalar3();
            p.x = float(value[0]);
            p.y = float(value[1]);
            p.z = float(value[2]);
            self.bdata.setAngMom(self.tag, p);
            return;
        if name == "moment_inertia":
            p = hoomd.Scalar3();
            p.x = float(value[0]);
            p.y = float(value[1]);
            p.z = float(value[2]);
            self.bdata.setBodyMomInertia(self.tag, p);
            return;
        if name == "particle_disp":
            p = hoomd.Scalar3();
            for i in range(0, self.num_particles):
                p.x = float(value[i][0]);
                p.y = float(value[i][1]);
                p.z = float(value[i][2]);
                self.bdata.setParticleDisp(self.tag, i, p);
            return;
        if name == "net_force":
            raise AttributeError;
        if name == "net_torque":
            raise AttributeError;

        # otherwise, consider this an internal attribute to be set in the normal way
        self.__dict__[name] = value;

## \internal
# \brief Get data.boxdim from a SnapshotSystemData
def get_snapshot_box(snapshot):
    b = snapshot._global_box;
    L = b.getL();
    return boxdim(Lx=L.x, Ly=L.y, Lz=L.z, xy=b.getTiltFactorXY(), xz=b.getTiltFactorXZ(), yz=b.getTiltFactorYZ(), dimensions=snapshot._dimensions);

## \internal
# \brief Set data.boxdim to a SnapshotSystemData
def set_snapshot_box(snapshot, box):
    snapshot._global_box = box._getBoxDim();
    snapshot._dimensions = box.dimensions;

# Inject a box property into SnapshotSystemData that provides and accepts boxdim objects
hoomd.SnapshotSystemData_float.box = property(get_snapshot_box, set_snapshot_box);
hoomd.SnapshotSystemData_double.box = property(get_snapshot_box, set_snapshot_box);

## Make an empty snapshot
#
# \param N Number of particles to create
# \param box a data.boxdim object that defines the simulation box
# \param particle_types List of particle type names (must not be zero length)
# \param bond_types List of bond type names (may be zero length)
# \param angle_types List of angle type names (may be zero length)
# \param dihedral_types List of Dihedral type names (may be zero length)
# \param improper_types List of improper type names (may be zero length)
# \param dtype Data type for the real valued numpy arrays in the snapshot. Must be either 'float' or 'double'.
#
# \b Examples:
# \code
# snapshot = data.make_snapshot(N=1000, box=data.boxdim(L=10))
# snapshot = data.make_snapshot(N=64000, box=data.boxdim(L=1, dimensions=2, volume=1000), particle_types=['A', 'B'])
# snapshot = data.make_snapshot(N=64000, box=data.boxdim(L=20), bond_types=['polymer'], dihedral_types=['dihedralA', 'dihedralB'], improper_types=['improperA', 'improperB', 'improperC'])
# ... set properties in snapshot ...
# init.read_snapshot(snapshot);
# \endcode
#
# make_snapshot() creates particles with <b>default properties</b>. You must set reasonable values for particle
# properties before initializing the system with init.read_snapshot().
#
# The default properties are:
# - position 0,0,0
# - velocity 0,0,0
# - image 0,0,0
# - orientation 1,0,0,0
# - typeid 0
# - charge 0
# - mass 1.0
# - diameter 1.0
#
# make_snapshot() creates the particle, bond, angle, dihedral, and improper types with the names specified. Use these
# type names later in the job script to refer to particles (i.e. in lj.set_params).
#
# \sa hoomd_script.init.read_snapshot()
def make_snapshot(N, box, particle_types=['A'], bond_types=[], angle_types=[], dihedral_types=[], improper_types=[], dtype='float'):
    if dtype == 'float':
        snapshot = hoomd.SnapshotSystemData_float();
    elif dtype == 'double':
        snapshot = hoomd.SnapshotSystemData_double();
    else:
        raise ValueError("dtype must be either float or double");

    snapshot.box = box;
    if hoomd_script.comm.get_rank() == 0:
        snapshot.particles.resize(N);

    snapshot.particles.types = particle_types;
    snapshot.bonds.types = bond_types;
    snapshot.angles.types = angle_types;
    snapshot.dihedrals.types = dihedral_types;
    snapshot.impropers.types = improper_types;

    return snapshot;



## \class SnapshotParticleData
# \brief Snapshot that stores particle properties
#
# Users should not create SnapshotParticleData directly. Use data.make_snapshot() or system_data.take_snapshot()
# to get a snapshot of the system.
class SnapshotParticleData:
    # dummy class just to make doxygen happy

    def __init__(self):
        # doxygen even needs to see these variables to generate documentation for them
        self.N = None;
        self.position = None;
        self.velocity = None;
        self.acceleration = None;
        self.typeid = None;
        self.mass = None;
        self.charge = None;
        self.diameter = None;
        self.image = None;
        self.body = None;
        self.types = None;
        self.orientation = None;
        self.moment_inertia = None;
        self.angmom = None;

    ## \property N
    # Number of particles in the snapshot

    ## \property types
    # List of string type names (assignable)

    ## \property position
    # Nx3 numpy array containing the position of each particle (float or double)

    ## \property orientation
    # Nx4 numpy array containing the orientation quaternion of each particle (float or double)

    ## \property velocity
    # Nx3 numpy array containing the velocity of each particle (float or double)

    ## \property acceleration
    # Nx3 numpy array containing the acceleration of each particle (float or double)

    ## \property typeid
    # N length numpy array containing the type id of each particle (32-bit unsigned int)

    ## \property mass
    # N length numpy array containing the mass of each particle (float or double)

    ## \property charge
    # N length numpy array containing the charge of each particle (float or double)

    ## \property diameter
    # N length numpy array containing the diameter of each particle (float or double)

    ## \property image
    # Nx3 numpy array containing the image of each particle (32-bit int)

    ## \property body
    # N length numpy array containing the body of each particle (32-bit unsigned int)

    ## \property moment_inertia
    # Nx3 length numpy array containing the principal moments of inertia of each particle (float or double)

    ## \property angmom
    # Nx4 length numpy array containing the angular momentum quaternion of each particle (float or double)

    ## Resize the snapshot to hold N particles
    #
    # \param N new size of the snapshot
    #
    # resize() changes the size of the arrays in the snapshot to hold \a N particles. Existing particle properties are
    # preserved after the resize. Any newly created particles will have default values. After resizing,
    # existing references to the numpy arrays will be invalid, access them again
    # from `snapshot.particles.*`
    def resize(self, N):
        pass
